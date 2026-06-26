#include "database/BackupService.h"

#include "database/Migrations.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUrl>

namespace {
constexpr int kBackupFormatVersion = 1;

struct TableSpec {
    QString table;
    QString jsonKey;
};

const QVector<TableSpec>& tableSpecs()
{
    static const QVector<TableSpec> specs = {
        {QStringLiteral("categories"), QStringLiteral("categories")},
        {QStringLiteral("tasks"), QStringLiteral("tasks")},
        {QStringLiteral("schedule_runs"), QStringLiteral("scheduleRuns")},
        {QStringLiteral("calendar_events"), QStringLiteral("calendarEvents")},
        {QStringLiteral("deadline_reminders"), QStringLiteral("deadlineReminders")},
        {QStringLiteral("time_blocks"), QStringLiteral("timeBlocks")},
        {QStringLiteral("study_frames"), QStringLiteral("studyFrames")},
        {QStringLiteral("app_settings"), QStringLiteral("settings")}
    };
    return specs;
}

QString localPath(const QString& path)
{
    const QUrl url(path);
    return url.isLocalFile() ? url.toLocalFile() : path;
}

QJsonValue jsonValue(const QVariant& value)
{
    if (value.isNull()) {
        return QJsonValue();
    }
    switch (value.metaType().id()) {
    case QMetaType::Bool:
        return value.toBool();
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return static_cast<double>(value.toLongLong());
    case QMetaType::Double:
        return value.toDouble();
    default:
        return value.toString();
    }
}

QVariant sqlValue(const QJsonValue& value)
{
    if (value.isNull() || value.isUndefined()) {
        return QVariant();
    }
    return value.toVariant();
}

bool collectIds(const QJsonArray& rows, QSet<int>& ids, QString& error, const QString& entity)
{
    for (const QJsonValue& value : rows) {
        if (!value.isObject()) {
            error = QStringLiteral("%1 contains a non-object row").arg(entity);
            return false;
        }
        const int id = value.toObject().value(QStringLiteral("id")).toInt();
        if (id <= 0 || ids.contains(id)) {
            error = QStringLiteral("%1 contains an invalid or duplicate id").arg(entity);
            return false;
        }
        ids.insert(id);
    }
    return true;
}

bool validateReferences(const QJsonObject& root, QString& error)
{
    QSet<int> categoryIds;
    QSet<int> taskIds;
    QSet<int> scheduleRunIds;
    if (!collectIds(root.value(QStringLiteral("categories")).toArray(), categoryIds, error, QStringLiteral("categories"))
        || !collectIds(root.value(QStringLiteral("tasks")).toArray(), taskIds, error, QStringLiteral("tasks"))
        || !collectIds(root.value(QStringLiteral("scheduleRuns")).toArray(), scheduleRunIds, error, QStringLiteral("scheduleRuns"))) {
        return false;
    }

    auto validOptionalCategory = [&](const QJsonObject& row) {
        const QJsonValue category = row.value(QStringLiteral("category_id"));
        return category.isNull() || category.isUndefined() || categoryIds.contains(category.toInt());
    };
    for (const QString& key : {QStringLiteral("tasks"), QStringLiteral("calendarEvents"), QStringLiteral("deadlineReminders"), QStringLiteral("studyFrames")}) {
        for (const QJsonValue& value : root.value(key).toArray()) {
            if (!value.isObject() || !validOptionalCategory(value.toObject())) {
                error = QStringLiteral("%1 contains an unknown category reference").arg(key);
                return false;
            }
        }
    }
    for (const QJsonValue& value : root.value(QStringLiteral("timeBlocks")).toArray()) {
        if (!value.isObject()) {
            error = QStringLiteral("timeBlocks contains a non-object row");
            return false;
        }
        const QJsonObject row = value.toObject();
        if (!taskIds.contains(row.value(QStringLiteral("task_id")).toInt())) {
            error = QStringLiteral("timeBlocks contains an unknown task reference");
            return false;
        }
        const QJsonValue runId = row.value(QStringLiteral("schedule_run_id"));
        if (!runId.isNull() && !runId.isUndefined() && !scheduleRunIds.contains(runId.toInt())) {
            error = QStringLiteral("timeBlocks contains an unknown schedule run reference");
            return false;
        }
    }
    return true;
}
}

BackupService::BackupService(QSqlDatabase db)
    : m_db(std::move(db))
{
}

bool BackupService::exportToFile(const QString& filePath) const
{
    m_lastError.clear();
    QJsonObject root;
    root.insert(QStringLiteral("backupFormatVersion"), kBackupFormatVersion);
    root.insert(QStringLiteral("schemaVersion"), Migrations::currentVersion());
    root.insert(QStringLiteral("exportedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));

    for (const TableSpec& spec : tableSpecs()) {
        QSqlQuery query(m_db);
        if (!query.exec(QStringLiteral("SELECT * FROM %1").arg(spec.table))) {
            m_lastError = query.lastError().text();
            return false;
        }
        QJsonArray rows;
        const QSqlRecord record = query.record();
        while (query.next()) {
            QJsonObject row;
            for (int i = 0; i < record.count(); ++i) {
                row.insert(record.fieldName(i), jsonValue(query.value(i)));
            }
            rows.push_back(row);
        }
        root.insert(spec.jsonKey, rows);
    }

    QFile file(localPath(filePath));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_lastError = file.errorString();
        return false;
    }
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0) {
        m_lastError = file.errorString();
        return false;
    }
    return true;
}

bool BackupService::importFromFile(const QString& filePath) const
{
    m_lastError.clear();
    QFile file(localPath(filePath));
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_lastError = parseError.errorString();
        return false;
    }

    QJsonObject root = document.object();
    if (root.value(QStringLiteral("backupFormatVersion")).toInt() != kBackupFormatVersion) {
        m_lastError = QStringLiteral("Unsupported backup format version");
        return false;
    }
    const int backupSchemaVersion = root.value(QStringLiteral("schemaVersion")).toInt();
    if (backupSchemaVersion > Migrations::currentVersion()) {
        m_lastError = QStringLiteral("Backup schema is newer than this application");
        return false;
    }
    for (const TableSpec& spec : tableSpecs()) {
        if (spec.jsonKey == QStringLiteral("deadlineReminders") && backupSchemaVersion < 4
            && root.value(spec.jsonKey).isUndefined()) {
            root.insert(spec.jsonKey, QJsonArray());
        }
        if (!root.value(spec.jsonKey).isArray()) {
            m_lastError = QStringLiteral("Backup is missing %1").arg(spec.jsonKey);
            return false;
        }
    }
    if (!validateReferences(root, m_lastError)) {
        return false;
    }

    QSqlDatabase db = m_db;
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }
    const QStringList deleteOrder = {
        QStringLiteral("time_blocks"),
        QStringLiteral("deadline_reminders"),
        QStringLiteral("calendar_events"),
        QStringLiteral("study_frames"),
        QStringLiteral("tasks"),
        QStringLiteral("schedule_runs"),
        QStringLiteral("categories"),
        QStringLiteral("app_settings")
    };
    for (const QString& table : deleteOrder) {
        QSqlQuery remove(db);
        if (!remove.exec(QStringLiteral("DELETE FROM %1").arg(table))) {
            m_lastError = remove.lastError().text();
            db.rollback();
            return false;
        }
    }

    for (const TableSpec& spec : tableSpecs()) {
        for (const QJsonValue& value : root.value(spec.jsonKey).toArray()) {
            const QJsonObject row = value.toObject();
            const QStringList columns = row.keys();
            QStringList placeholders;
            for (qsizetype i = 0; i < columns.size(); ++i) {
                placeholders.push_back(QStringLiteral("?"));
            }
            QSqlQuery insert(db);
            insert.prepare(QStringLiteral("INSERT INTO %1(%2) VALUES(%3)")
                               .arg(spec.table, columns.join(QStringLiteral(",")),
                                    placeholders.join(QStringLiteral(","))));
            for (const QString& column : columns) {
                insert.addBindValue(sqlValue(row.value(column)));
            }
            if (!insert.exec()) {
                m_lastError = insert.lastError().text();
                db.rollback();
                return false;
            }
        }
    }

    if (!db.commit()) {
        m_lastError = db.lastError().text();
        return false;
    }
    return true;
}

QString BackupService::lastError() const
{
    return m_lastError;
}
