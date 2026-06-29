#include "database/Migrations.h"

#include "core/models/ScheduleTypes.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>
#include <functional>
#include <tuple>

namespace {
constexpr int kCurrentVersion = 7;
QString g_lastError;

QString iso(const QDateTime& dateTime)
{
    return dateTime.toString(Qt::ISODate);
}

bool execSql(QSqlDatabase db, const QString& sql)
{
    QSqlQuery query(db);
    if (query.exec(sql)) {
        return true;
    }
    g_lastError = query.lastError().text() + QStringLiteral(" | ") + sql.left(160);
    return false;
}

bool columnExists(QSqlDatabase db, const QString& table, const QString& column)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        g_lastError = query.lastError().text();
        return false;
    }
    while (query.next()) {
        if (query.value(1).toString() == column) {
            return true;
        }
    }
    return false;
}

bool addColumnIfMissing(QSqlDatabase db, const QString& table, const QString& column, const QString& declaration)
{
    if (columnExists(db, table, column)) {
        return true;
    }
    return execSql(db, QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, declaration));
}

int userVersion(QSqlDatabase db)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        g_lastError = query.lastError().text();
        return -1;
    }
    return query.value(0).toInt();
}

bool setUserVersion(QSqlDatabase db, int version)
{
    return execSql(db, QStringLiteral("PRAGMA user_version = %1").arg(version));
}

bool runInTransaction(QSqlDatabase db, const std::function<bool()>& migration)
{
    if (!db.transaction()) {
        g_lastError = db.lastError().text();
        return false;
    }
    if (!migration()) {
        db.rollback();
        return false;
    }
    if (!db.commit()) {
        g_lastError = db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

bool migrateToV1(QSqlDatabase db)
{
    return runInTransaction(db, [&] {
        return execSql(db, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS categories (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              name TEXT NOT NULL,
              color TEXT NOT NULL
            )
        )SQL"))
            && execSql(db, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS tasks (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              title TEXT NOT NULL,
              notes TEXT,
              start_date TEXT,
              deadline TEXT NOT NULL,
              deadline_type INTEGER NOT NULL DEFAULT 0,
              estimated_minutes INTEGER NOT NULL,
              estimated_hours REAL NOT NULL DEFAULT 0,
              remaining_minutes INTEGER NOT NULL,
              preferred_study_time TEXT,
              auto_schedule_enabled INTEGER NOT NULL DEFAULT 1,
              min_chunk_minutes INTEGER NOT NULL DEFAULT 30,
              ideal_chunk_minutes INTEGER NOT NULL DEFAULT 90,
              effort_level INTEGER NOT NULL DEFAULT 1,
              schedule_status INTEGER NOT NULL DEFAULT 0,
              priority INTEGER NOT NULL,
              status INTEGER NOT NULL,
              category_id INTEGER,
              completed_at TEXT,
              created_at TEXT NOT NULL,
              updated_at TEXT NOT NULL
            )
        )SQL"))
            && addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("start_date"), QStringLiteral("TEXT"))
            && addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("deadline_type"), QStringLiteral("INTEGER NOT NULL DEFAULT 0"))
            && addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("estimated_hours"), QStringLiteral("REAL NOT NULL DEFAULT 0"))
            && addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("preferred_study_time"), QStringLiteral("TEXT"))
            && addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("auto_schedule_enabled"), QStringLiteral("INTEGER NOT NULL DEFAULT 1"))
            && addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("min_chunk_minutes"), QStringLiteral("INTEGER NOT NULL DEFAULT 30"))
            && addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("ideal_chunk_minutes"), QStringLiteral("INTEGER NOT NULL DEFAULT 90"))
            && addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("effort_level"), QStringLiteral("INTEGER NOT NULL DEFAULT 1"))
            && addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("schedule_status"), QStringLiteral("INTEGER NOT NULL DEFAULT 0"))
            && addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("completed_at"), QStringLiteral("TEXT"))
            && execSql(db, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS calendar_events (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              title TEXT NOT NULL,
              start_time TEXT NOT NULL,
              end_time TEXT NOT NULL,
              type INTEGER NOT NULL,
              category_id INTEGER,
              is_locked INTEGER NOT NULL DEFAULT 1,
              created_at TEXT NOT NULL,
              updated_at TEXT NOT NULL
            )
        )SQL"))
            && execSql(db, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS schedule_runs (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              started_at TEXT NOT NULL,
              horizon_start TEXT NOT NULL,
              horizon_end TEXT NOT NULL,
              reason TEXT NOT NULL
            )
        )SQL"))
            && execSql(db, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS time_blocks (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              task_id INTEGER NOT NULL,
              start_time TEXT NOT NULL,
              end_time TEXT NOT NULL,
              source INTEGER NOT NULL,
              schedule_run_id INTEGER,
              explanation TEXT,
              completed_at TEXT,
              created_at TEXT NOT NULL
            )
        )SQL"))
            && addColumnIfMissing(db, QStringLiteral("time_blocks"), QStringLiteral("explanation"), QStringLiteral("TEXT"))
            && addColumnIfMissing(db, QStringLiteral("time_blocks"), QStringLiteral("completed_at"), QStringLiteral("TEXT"))
            && execSql(db, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS study_frames (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              name TEXT NOT NULL,
              day_of_week INTEGER NOT NULL,
              start_time TEXT NOT NULL,
              end_time TEXT NOT NULL,
              category_id INTEGER,
              energy_level TEXT NOT NULL,
              enabled INTEGER NOT NULL DEFAULT 1,
              created_at TEXT NOT NULL,
              updated_at TEXT NOT NULL
            )
        )SQL"))
            && execSql(db, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS app_settings (
              key TEXT PRIMARY KEY,
              value TEXT NOT NULL
            )
        )SQL"))
            && setUserVersion(db, 1);
    });
}

bool migrateToV2(QSqlDatabase db)
{
    return runInTransaction(db, [&] {
        const QString now = iso(QDateTime::currentDateTime());
        QSqlQuery fillNames(db);
        fillNames.prepare(QStringLiteral("UPDATE categories SET name = 'Category ' || id WHERE TRIM(name) = ''"));
        if (!fillNames.exec()) {
            g_lastError = fillNames.lastError().text();
            return false;
        }

        // Point all duplicate category references at the oldest row before deleting duplicates.
        const QStringList referencingTables = {
            QStringLiteral("tasks"),
            QStringLiteral("calendar_events"),
            QStringLiteral("study_frames")
        };
        for (const QString& table : referencingTables) {
            if (!execSql(db, QStringLiteral(R"SQL(
                UPDATE %1
                SET category_id = (
                    SELECT MIN(c2.id) FROM categories c2
                    WHERE c2.name = (SELECT c1.name FROM categories c1 WHERE c1.id = %1.category_id)
                )
                WHERE category_id IS NOT NULL
            )SQL").arg(table))) {
                return false;
            }
        }
        if (!execSql(db, QStringLiteral(
                "DELETE FROM categories WHERE id NOT IN (SELECT MIN(id) FROM categories GROUP BY name)"))
            || !execSql(db, QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS idx_categories_name ON categories(name)"))
            || !addColumnIfMissing(db, QStringLiteral("time_blocks"), QStringLiteral("updated_at"),
                                   QStringLiteral("TEXT NOT NULL DEFAULT ''"))) {
            return false;
        }

        QSqlQuery stamp(db);
        stamp.prepare(QStringLiteral("UPDATE time_blocks SET updated_at = COALESCE(NULLIF(updated_at, ''), created_at, ?)"));
        stamp.addBindValue(now);
        if (!stamp.exec()) {
            g_lastError = stamp.lastError().text();
            return false;
        }
        return setUserVersion(db, 2);
    });
}

bool migrateToV3(QSqlDatabase db)
{
    if (!execSql(db, QStringLiteral("PRAGMA foreign_keys = OFF"))) {
        return false;
    }

    const bool ok = runInTransaction(db, [&] {
        return execSql(db, QStringLiteral(R"SQL(
            CREATE TABLE tasks_v3 (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              title TEXT NOT NULL,
              notes TEXT,
              start_date TEXT,
              deadline TEXT NOT NULL,
              deadline_type INTEGER NOT NULL DEFAULT 0,
              estimated_minutes INTEGER NOT NULL,
              estimated_hours REAL NOT NULL DEFAULT 0,
              remaining_minutes INTEGER NOT NULL,
              preferred_study_time TEXT,
              auto_schedule_enabled INTEGER NOT NULL DEFAULT 1,
              min_chunk_minutes INTEGER NOT NULL DEFAULT 30,
              ideal_chunk_minutes INTEGER NOT NULL DEFAULT 90,
              effort_level INTEGER NOT NULL DEFAULT 1,
              schedule_status INTEGER NOT NULL DEFAULT 0,
              priority INTEGER NOT NULL,
              status INTEGER NOT NULL,
              category_id INTEGER,
              completed_at TEXT,
              created_at TEXT NOT NULL,
              updated_at TEXT NOT NULL,
              FOREIGN KEY(category_id) REFERENCES categories(id) ON DELETE SET NULL
            )
        )SQL"))
            && execSql(db, QStringLiteral(R"SQL(
            INSERT INTO tasks_v3
            SELECT id, title, notes, start_date, deadline, deadline_type, estimated_minutes,
                   estimated_hours, remaining_minutes, preferred_study_time, auto_schedule_enabled,
                   min_chunk_minutes, ideal_chunk_minutes, effort_level, schedule_status, priority,
                   status, category_id, completed_at, created_at, updated_at
            FROM tasks
        )SQL"))
            && execSql(db, QStringLiteral(R"SQL(
            CREATE TABLE calendar_events_v3 (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              title TEXT NOT NULL,
              start_time TEXT NOT NULL,
              end_time TEXT NOT NULL,
              type INTEGER NOT NULL,
              category_id INTEGER,
              is_locked INTEGER NOT NULL DEFAULT 1,
              created_at TEXT NOT NULL,
              updated_at TEXT NOT NULL,
              FOREIGN KEY(category_id) REFERENCES categories(id) ON DELETE SET NULL
            )
        )SQL"))
            && execSql(db, QStringLiteral(R"SQL(
            INSERT INTO calendar_events_v3
            SELECT id, title, start_time, end_time, type, category_id, is_locked, created_at, updated_at
            FROM calendar_events
        )SQL"))
            && execSql(db, QStringLiteral(R"SQL(
            CREATE TABLE study_frames_v3 (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              name TEXT NOT NULL,
              day_of_week INTEGER NOT NULL,
              start_time TEXT NOT NULL,
              end_time TEXT NOT NULL,
              category_id INTEGER,
              energy_level TEXT NOT NULL,
              enabled INTEGER NOT NULL DEFAULT 1,
              created_at TEXT NOT NULL,
              updated_at TEXT NOT NULL,
              FOREIGN KEY(category_id) REFERENCES categories(id) ON DELETE SET NULL
            )
        )SQL"))
            && execSql(db, QStringLiteral(R"SQL(
            INSERT INTO study_frames_v3
            SELECT id, name, day_of_week, start_time, end_time, category_id, energy_level,
                   enabled, created_at, updated_at
            FROM study_frames
        )SQL"))
            && execSql(db, QStringLiteral(R"SQL(
            CREATE TABLE time_blocks_v3 (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              task_id INTEGER NOT NULL,
              start_time TEXT NOT NULL,
              end_time TEXT NOT NULL,
              source INTEGER NOT NULL,
              schedule_run_id INTEGER,
              explanation TEXT,
              completed_at TEXT,
              created_at TEXT NOT NULL,
              updated_at TEXT NOT NULL,
              FOREIGN KEY(task_id) REFERENCES tasks_v3(id) ON DELETE CASCADE,
              FOREIGN KEY(schedule_run_id) REFERENCES schedule_runs(id) ON DELETE SET NULL
            )
        )SQL"))
            && execSql(db, QStringLiteral(R"SQL(
            INSERT INTO time_blocks_v3
            SELECT id, task_id, start_time, end_time, source, schedule_run_id, explanation,
                   completed_at, created_at, updated_at
            FROM time_blocks
        )SQL"))
            && execSql(db, QStringLiteral("DROP TABLE time_blocks"))
            && execSql(db, QStringLiteral("DROP TABLE calendar_events"))
            && execSql(db, QStringLiteral("DROP TABLE study_frames"))
            && execSql(db, QStringLiteral("DROP TABLE tasks"))
            && execSql(db, QStringLiteral("ALTER TABLE tasks_v3 RENAME TO tasks"))
            && execSql(db, QStringLiteral("ALTER TABLE calendar_events_v3 RENAME TO calendar_events"))
            && execSql(db, QStringLiteral("ALTER TABLE study_frames_v3 RENAME TO study_frames"))
            && execSql(db, QStringLiteral("ALTER TABLE time_blocks_v3 RENAME TO time_blocks"))
            && execSql(db, QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tasks_deadline ON tasks(deadline)"))
            && execSql(db, QStringLiteral("CREATE INDEX IF NOT EXISTS idx_time_blocks_range ON time_blocks(start_time, end_time)"))
            && execSql(db, QStringLiteral("CREATE INDEX IF NOT EXISTS idx_events_range ON calendar_events(start_time, end_time)"))
            && execSql(db, QStringLiteral("CREATE INDEX IF NOT EXISTS idx_study_frames_day ON study_frames(day_of_week, enabled)"))
            && setUserVersion(db, 3);
    });

    const bool enabled = execSql(db, QStringLiteral("PRAGMA foreign_keys = ON"));
    if (!ok || !enabled) {
        return false;
    }
    QSqlQuery check(db);
    if (!check.exec(QStringLiteral("PRAGMA foreign_key_check"))) {
        g_lastError = check.lastError().text();
        return false;
    }
    if (check.next()) {
        g_lastError = QStringLiteral("Foreign key validation failed after migration");
        return false;
    }
    return true;
}

bool migrateToV4(QSqlDatabase db)
{
    return runInTransaction(db, [&] {
        return addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("color_override"), QStringLiteral("TEXT"))
            && addColumnIfMissing(db, QStringLiteral("calendar_events"), QStringLiteral("color_override"), QStringLiteral("TEXT"))
            && setUserVersion(db, 4);
    });
}

bool migrateToV5(QSqlDatabase db)
{
    return runInTransaction(db, [&] {
        if (!addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("color_override"), QStringLiteral("TEXT"))
            || !addColumnIfMissing(db, QStringLiteral("calendar_events"), QStringLiteral("color_override"), QStringLiteral("TEXT"))) {
            return false;
        }

        QSqlQuery events(db);
        if (!events.exec(QStringLiteral(R"SQL(
            SELECT id, title, start_time, end_time, category_id, is_locked,
                   created_at, updated_at, color_override
            FROM calendar_events
            ORDER BY id
        )SQL"))) {
            g_lastError = events.lastError().text();
            return false;
        }

        while (events.next()) {
            const QString title = events.value(1).toString();
            const QDateTime start = QDateTime::fromString(events.value(2).toString(), Qt::ISODate);
            const QDateTime end = QDateTime::fromString(events.value(3).toString(), Qt::ISODate);
            if (!start.isValid() || !end.isValid() || end <= start) {
                g_lastError = QStringLiteral("Invalid calendar event during unified-block migration: %1").arg(title);
                return false;
            }

            const int durationMinutes = qMax(15, static_cast<int>(start.secsTo(end) / 60));
            const QString createdAt = events.value(6).toString().isEmpty() ? iso(QDateTime::currentDateTime()) : events.value(6).toString();
            const QString updatedAt = events.value(7).toString().isEmpty() ? createdAt : events.value(7).toString();

            QSqlQuery task(db);
            task.prepare(QStringLiteral(R"SQL(
                INSERT INTO tasks(
                    title, notes, start_date, deadline, deadline_type,
                    estimated_minutes, estimated_hours, remaining_minutes,
                    preferred_study_time, auto_schedule_enabled,
                    min_chunk_minutes, ideal_chunk_minutes, effort_level,
                    schedule_status, priority, status, category_id,
                    completed_at, created_at, updated_at, color_override
                )
                VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NULL, ?, ?, ?)
            )SQL"));
            task.addBindValue(title);
            task.addBindValue(QStringLiteral("由固定日程迁移为统一时间块"));
            task.addBindValue(iso(QDateTime(start.date(), QTime(0, 0))));
            task.addBindValue(iso(end));
            task.addBindValue(static_cast<int>(DeadlineType::Hard));
            task.addBindValue(durationMinutes);
            task.addBindValue(durationMinutes / 60.0);
            task.addBindValue(durationMinutes);
            task.addBindValue(QStringLiteral("evening"));
            task.addBindValue(0);
            task.addBindValue(qMin(60, durationMinutes));
            task.addBindValue(durationMinutes);
            task.addBindValue(1);
            task.addBindValue(static_cast<int>(ScheduleStatus::Scheduled));
            task.addBindValue(static_cast<int>(Priority::Medium));
            task.addBindValue(static_cast<int>(TaskStatus::Scheduled));
            task.addBindValue(events.value(4));
            task.addBindValue(createdAt);
            task.addBindValue(updatedAt);
            task.addBindValue(events.value(8));
            if (!task.exec()) {
                g_lastError = task.lastError().text();
                return false;
            }

            QSqlQuery block(db);
            block.prepare(QStringLiteral(R"SQL(
                INSERT INTO time_blocks(
                    task_id, start_time, end_time, source, schedule_run_id,
                    explanation, completed_at, created_at, updated_at
                )
                VALUES(?, ?, ?, ?, NULL, ?, NULL, ?, ?)
            )SQL"));
            block.addBindValue(task.lastInsertId());
            block.addBindValue(iso(start));
            block.addBindValue(iso(end));
            block.addBindValue(events.value(5).toBool()
                                   ? static_cast<int>(BlockSource::Locked)
                                   : static_cast<int>(BlockSource::UserDragged));
            block.addBindValue(QStringLiteral("由固定日程迁移"));
            block.addBindValue(createdAt);
            block.addBindValue(updatedAt);
            if (!block.exec()) {
                g_lastError = block.lastError().text();
                return false;
            }
        }

        return execSql(db, QStringLiteral("DELETE FROM calendar_events"))
            && setUserVersion(db, 5);
    });
}

bool migrateToV6(QSqlDatabase db)
{
    return runInTransaction(db, [&] {
        return execSql(db, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS deadline_reminders (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              title TEXT NOT NULL,
              notes TEXT,
              due_at TEXT NOT NULL,
              category_id INTEGER,
              remind_days_before INTEGER NOT NULL DEFAULT 3,
              status INTEGER NOT NULL DEFAULT 0,
              completed_at TEXT,
              created_at TEXT NOT NULL,
              updated_at TEXT NOT NULL,
              FOREIGN KEY(category_id) REFERENCES categories(id) ON DELETE SET NULL
            )
        )SQL"))
            && execSql(db, QStringLiteral("CREATE INDEX IF NOT EXISTS idx_deadline_reminders_due_at ON deadline_reminders(due_at)"))
            && execSql(db, QStringLiteral("CREATE INDEX IF NOT EXISTS idx_deadline_reminders_status ON deadline_reminders(status)"))
            && setUserVersion(db, 6);
    });
}

bool migrateToV7(QSqlDatabase db)
{
    return runInTransaction(db, [&] {
        return execSql(db, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS ai_memories (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              kind TEXT NOT NULL,
              memory_key TEXT NOT NULL,
              content TEXT NOT NULL,
              weight REAL NOT NULL DEFAULT 1.0,
              created_at TEXT NOT NULL,
              updated_at TEXT NOT NULL,
              last_used_at TEXT,
              UNIQUE(kind, memory_key)
            )
        )SQL"))
            && execSql(db, QStringLiteral("CREATE INDEX IF NOT EXISTS idx_ai_memories_weight ON ai_memories(weight DESC, updated_at DESC)"))
            && setUserVersion(db, 7);
    });
}
}

bool Migrations::run(QSqlDatabase db)
{
    g_lastError.clear();
    if (!db.isOpen()) {
        g_lastError = QStringLiteral("Database is not open");
        return false;
    }
    int version = userVersion(db);
    if (version < 0) {
        return false;
    }
    if (version > kCurrentVersion) {
        g_lastError = QStringLiteral("Database version %1 is newer than supported version %2")
                          .arg(version)
                          .arg(kCurrentVersion);
        return false;
    }
    if (version < 1 && !migrateToV1(db)) {
        return false;
    }
    if (version < 2 && !migrateToV2(db)) {
        return false;
    }
    if (version < 3 && !migrateToV3(db)) {
        return false;
    }
    if (version < 4 && !migrateToV4(db)) {
        return false;
    }
    if (version < 5 && !migrateToV5(db)) {
        return false;
    }
    if (version < 6 && !migrateToV6(db)) {
        return false;
    }
    if (version < 7 && !migrateToV7(db)) {
        return false;
    }
    return execSql(db, QStringLiteral("PRAGMA foreign_keys = ON"));
}

int Migrations::currentVersion()
{
    return kCurrentVersion;
}

QString Migrations::lastError()
{
    return g_lastError;
}

bool Migrations::seed(QSqlDatabase db)
{
    QSqlQuery count(db);
    if (!count.exec(QStringLiteral("SELECT COUNT(*) FROM tasks")) || !count.next()) {
        return false;
    }
    if (count.value(0).toInt() > 0) {
        return true;
    }

    if (!db.transaction()) {
        return false;
    }
    const QString now = iso(QDateTime::currentDateTime());
    QSqlQuery category(db);
    category.prepare(QStringLiteral("INSERT INTO categories(name, color) VALUES(?, ?)"));
    const QVector<QPair<QString, QString>> categories = {
        {QStringLiteral("Mathematics"), QStringLiteral("#FF8A65")},
        {QStringLiteral("English"), QStringLiteral("#7C8CFF")},
        {QStringLiteral("Computer Science"), QStringLiteral("#6FD6A7")}
    };
    for (const auto& item : categories) {
        category.addBindValue(item.first);
        category.addBindValue(item.second);
        if (!category.exec()) {
            db.rollback();
            return false;
        }
    }

    QSqlQuery task(db);
    task.prepare(QStringLiteral(R"SQL(
        INSERT INTO tasks(title, notes, start_date, deadline, deadline_type, estimated_minutes,
                          estimated_hours, remaining_minutes, preferred_study_time,
                          auto_schedule_enabled, min_chunk_minutes, ideal_chunk_minutes,
                          effort_level, schedule_status, priority, status, category_id,
                          created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));
    struct SeedTask {
        QString title;
        int days;
        int minutes;
        int category;
    };
    const QVector<SeedTask> tasks = {
        {QStringLiteral("Review calculus"), 2, 240, 1},
        {QStringLiteral("English reading"), 4, 120, 2},
        {QStringLiteral("Data structures assignment"), 3, 180, 3}
    };
    for (const auto& item : tasks) {
        task.addBindValue(item.title);
        task.addBindValue(QString());
        task.addBindValue(iso(QDateTime(QDate::currentDate(), QTime(8, 0))));
        task.addBindValue(iso(QDateTime(QDate::currentDate().addDays(item.days), QTime(23, 59))));
        task.addBindValue(static_cast<int>(DeadlineType::Soft));
        task.addBindValue(item.minutes);
        task.addBindValue(item.minutes / 60.0);
        task.addBindValue(item.minutes);
        task.addBindValue(QStringLiteral("evening"));
        task.addBindValue(1);
        task.addBindValue(30);
        task.addBindValue(90);
        task.addBindValue(1);
        task.addBindValue(static_cast<int>(ScheduleStatus::Unscheduled));
        task.addBindValue(static_cast<int>(Priority::Medium));
        task.addBindValue(static_cast<int>(TaskStatus::Inbox));
        task.addBindValue(item.category);
        task.addBindValue(now);
        task.addBindValue(now);
        if (!task.exec()) {
            db.rollback();
            return false;
        }
    }

    const QDate today = QDate::currentDate();
    const QVector<std::tuple<QString, QDateTime, QDateTime, int>> events = {
        {QStringLiteral("Calculus class"), QDateTime(today, QTime(10, 0)), QDateTime(today, QTime(11, 40)), 1},
        {QStringLiteral("English listening"), QDateTime(today.addDays(1), QTime(14, 0)), QDateTime(today.addDays(1), QTime(15, 30)), 2},
        {QStringLiteral("Laboratory"), QDateTime(today.addDays(2), QTime(9, 0)), QDateTime(today.addDays(2), QTime(11, 30)), 3}
    };
    for (const auto& item : events) {
        const int minutes = static_cast<int>(std::get<1>(item).secsTo(std::get<2>(item)) / 60);
        QSqlQuery seededTask(db);
        seededTask.prepare(QStringLiteral(R"SQL(
            INSERT INTO tasks(
                title, notes, start_date, deadline, deadline_type,
                estimated_minutes, estimated_hours, remaining_minutes,
                preferred_study_time, auto_schedule_enabled,
                min_chunk_minutes, ideal_chunk_minutes, effort_level,
                schedule_status, priority, status, category_id,
                completed_at, created_at, updated_at
            )
            VALUES(?, '', ?, ?, ?, ?, ?, ?, 'evening', 0, ?, ?, 1, ?, ?, ?, ?, NULL, ?, ?)
        )SQL"));
        seededTask.addBindValue(std::get<0>(item));
        seededTask.addBindValue(iso(QDateTime(std::get<1>(item).date(), QTime(0, 0))));
        seededTask.addBindValue(iso(std::get<2>(item)));
        seededTask.addBindValue(static_cast<int>(DeadlineType::Hard));
        seededTask.addBindValue(minutes);
        seededTask.addBindValue(minutes / 60.0);
        seededTask.addBindValue(minutes);
        seededTask.addBindValue(qMin(60, minutes));
        seededTask.addBindValue(minutes);
        seededTask.addBindValue(static_cast<int>(ScheduleStatus::Scheduled));
        seededTask.addBindValue(static_cast<int>(Priority::Medium));
        seededTask.addBindValue(static_cast<int>(TaskStatus::Scheduled));
        seededTask.addBindValue(std::get<3>(item));
        seededTask.addBindValue(now);
        seededTask.addBindValue(now);
        if (!seededTask.exec()) {
            db.rollback();
            return false;
        }

        QSqlQuery seededBlock(db);
        seededBlock.prepare(QStringLiteral(R"SQL(
            INSERT INTO time_blocks(
                task_id, start_time, end_time, source, schedule_run_id,
                explanation, completed_at, created_at, updated_at
            )
            VALUES(?, ?, ?, ?, NULL, 'Demo unified block', NULL, ?, ?)
        )SQL"));
        seededBlock.addBindValue(seededTask.lastInsertId());
        seededBlock.addBindValue(iso(std::get<1>(item)));
        seededBlock.addBindValue(iso(std::get<2>(item)));
        seededBlock.addBindValue(static_cast<int>(BlockSource::Locked));
        seededBlock.addBindValue(now);
        seededBlock.addBindValue(now);
        if (!seededBlock.exec()) {
            db.rollback();
            return false;
        }
    }

    QSqlQuery frame(db);
    frame.prepare(QStringLiteral(R"SQL(
        INSERT INTO study_frames(name, day_of_week, start_time, end_time, category_id,
                                 energy_level, enabled, created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));
    const QVector<std::tuple<QString, int, QString, QString, int, QString>> frames = {
        {QStringLiteral("Deep Study"), 1, QStringLiteral("19:00"), QStringLiteral("22:00"), 1, QStringLiteral("high")},
        {QStringLiteral("Reading"), 2, QStringLiteral("08:00"), QStringLiteral("09:30"), 2, QStringLiteral("medium")},
        {QStringLiteral("Deep Study"), 3, QStringLiteral("19:00"), QStringLiteral("22:00"), 1, QStringLiteral("high")},
        {QStringLiteral("Deep Study"), 5, QStringLiteral("19:00"), QStringLiteral("22:00"), 1, QStringLiteral("high")}
    };
    for (const auto& item : frames) {
        frame.addBindValue(std::get<0>(item));
        frame.addBindValue(std::get<1>(item));
        frame.addBindValue(std::get<2>(item));
        frame.addBindValue(std::get<3>(item));
        frame.addBindValue(std::get<4>(item));
        frame.addBindValue(std::get<5>(item));
        frame.addBindValue(1);
        frame.addBindValue(now);
        frame.addBindValue(now);
        if (!frame.exec()) {
            db.rollback();
            return false;
        }
    }

    QSqlQuery locale(db);
    locale.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO app_settings(key, value) VALUES('locale', 'zh_CN')"));
    if (!locale.exec()) {
        db.rollback();
        return false;
    }
    return db.commit();
}
