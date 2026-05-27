#include "database/Database.h"

#include <QDir>
#include <QSqlError>
#include <QStandardPaths>

bool Database::open()
{
    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataRoot);

    QSqlDatabase db = QSqlDatabase::contains(m_connectionName)
        ? QSqlDatabase::database(m_connectionName)
        : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);

    db.setDatabaseName(dataRoot + QStringLiteral("/chrona.sqlite"));
    if (!db.open()) {
        m_lastError = db.lastError().text();
        return false;
    }
    return true;
}

QSqlDatabase Database::connection() const
{
    return QSqlDatabase::database(m_connectionName);
}

QString Database::lastError() const
{
    return m_lastError;
}
