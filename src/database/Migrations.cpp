#include "database/Migrations.h"

#include "core/models/ScheduleTypes.h"

#include <QDateTime>
#include <QSqlQuery>
#include <QVariant>
#include <tuple>

namespace {
bool exec(QSqlDatabase db, const QString& sql)
{
    QSqlQuery query(db);
    return query.exec(sql);
}

QString iso(const QDateTime& dateTime)
{
    return dateTime.toString(Qt::ISODate);
}

bool columnExists(QSqlDatabase db, const QString& table, const QString& column)
{
    QSqlQuery query(db);
    query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table));
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
    return exec(db, QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, declaration));
}
}

bool Migrations::run(QSqlDatabase db)
{
    return exec(db, QStringLiteral("PRAGMA foreign_keys = ON")) &&
        exec(db, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS categories (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              name TEXT NOT NULL,
              color TEXT NOT NULL
            )
        )SQL")) &&
        exec(db, QStringLiteral(R"SQL(
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
              created_at TEXT NOT NULL,
              updated_at TEXT NOT NULL
            )
        )SQL")) &&
        addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("start_date"), QStringLiteral("TEXT")) &&
        addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("deadline_type"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")) &&
        addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("estimated_hours"), QStringLiteral("REAL NOT NULL DEFAULT 0")) &&
        addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("preferred_study_time"), QStringLiteral("TEXT")) &&
        addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("auto_schedule_enabled"), QStringLiteral("INTEGER NOT NULL DEFAULT 1")) &&
        addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("min_chunk_minutes"), QStringLiteral("INTEGER NOT NULL DEFAULT 30")) &&
        addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("ideal_chunk_minutes"), QStringLiteral("INTEGER NOT NULL DEFAULT 90")) &&
        addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("effort_level"), QStringLiteral("INTEGER NOT NULL DEFAULT 1")) &&
        addColumnIfMissing(db, QStringLiteral("tasks"), QStringLiteral("schedule_status"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")) &&
        exec(db, QStringLiteral(R"SQL(
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
        )SQL")) &&
        exec(db, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS time_blocks (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              task_id INTEGER NOT NULL,
              start_time TEXT NOT NULL,
              end_time TEXT NOT NULL,
              source INTEGER NOT NULL,
              schedule_run_id INTEGER,
              created_at TEXT NOT NULL,
              FOREIGN KEY(task_id) REFERENCES tasks(id)
            )
        )SQL")) &&
        exec(db, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS schedule_runs (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              started_at TEXT NOT NULL,
              horizon_start TEXT NOT NULL,
              horizon_end TEXT NOT NULL,
              reason TEXT NOT NULL
            )
        )SQL")) &&
        exec(db, QStringLiteral(R"SQL(
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
              updated_at TEXT NOT NULL,
              FOREIGN KEY(category_id) REFERENCES categories(id)
            )
        )SQL")) &&
        exec(db, QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS app_settings (
              key TEXT PRIMARY KEY,
              value TEXT NOT NULL
            )
        )SQL")) &&
        exec(db, QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tasks_deadline ON tasks(deadline)")) &&
        exec(db, QStringLiteral("CREATE INDEX IF NOT EXISTS idx_time_blocks_range ON time_blocks(start_time, end_time)")) &&
        exec(db, QStringLiteral("CREATE INDEX IF NOT EXISTS idx_events_range ON calendar_events(start_time, end_time)")) &&
        exec(db, QStringLiteral("CREATE INDEX IF NOT EXISTS idx_study_frames_day ON study_frames(day_of_week, enabled)"));
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

    const QString now = iso(QDateTime::currentDateTime());
    QSqlQuery category(db);
    category.prepare(QStringLiteral("INSERT INTO categories(name, color) VALUES(?, ?)"));
    const QVector<QPair<QString, QString>> categories = {
        {QStringLiteral("高数"), QStringLiteral("#FF8A65")},
        {QStringLiteral("英语"), QStringLiteral("#7C8CFF")},
        {QStringLiteral("计算机"), QStringLiteral("#6FD6A7")}
    };
    for (const auto& item : categories) {
        category.addBindValue(item.first);
        category.addBindValue(item.second);
        if (!category.exec()) {
            return false;
        }
    }

    QSqlQuery task(db);
    task.prepare(QStringLiteral(R"SQL(
        INSERT INTO tasks(title, notes, start_date, deadline, deadline_type, estimated_minutes, estimated_hours, remaining_minutes, preferred_study_time,
                          auto_schedule_enabled, min_chunk_minutes, ideal_chunk_minutes, effort_level, schedule_status, priority, status, category_id, created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));

    struct SeedTask {
        QString title;
        QString notes;
        int days;
        int minutes;
        int priority;
        int category;
    };

    const QVector<SeedTask> tasks = {
        {QStringLiteral("高数期中复习"), QStringLiteral("重点：极限、导数、积分题型"), 2, 240, 2, 1},
        {QStringLiteral("英语阅读精练"), QStringLiteral("完成两篇阅读并整理生词"), 4, 120, 1, 2},
        {QStringLiteral("数据结构作业"), QStringLiteral("实现图遍历并写实验报告"), 3, 180, 2, 3},
        {QStringLiteral("线性代数错题整理"), QStringLiteral("矩阵和特征值章节"), 6, 90, 0, 1}
    };

    for (const auto& item : tasks) {
        task.addBindValue(item.title);
        task.addBindValue(item.notes);
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
        task.addBindValue(item.priority);
        task.addBindValue(static_cast<int>(TaskStatus::Inbox));
        task.addBindValue(item.category);
        task.addBindValue(now);
        task.addBindValue(now);
        if (!task.exec()) {
            return false;
        }
    }

    QSqlQuery event(db);
    event.prepare(QStringLiteral(R"SQL(
        INSERT INTO calendar_events(title, start_time, end_time, type, category_id, is_locked, created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));

    const QDate today = QDate::currentDate();
    const QVector<std::tuple<QString, QDateTime, QDateTime, int>> events = {
        {QStringLiteral("高数课"), QDateTime(today, QTime(10, 0)), QDateTime(today, QTime(11, 40)), 1},
        {QStringLiteral("英语听力课"), QDateTime(today.addDays(1), QTime(14, 0)), QDateTime(today.addDays(1), QTime(15, 30)), 2},
        {QStringLiteral("实验课"), QDateTime(today.addDays(2), QTime(9, 0)), QDateTime(today.addDays(2), QTime(11, 30)), 3}
    };

    for (const auto& item : events) {
        event.addBindValue(std::get<0>(item));
        event.addBindValue(iso(std::get<1>(item)));
        event.addBindValue(iso(std::get<2>(item)));
        event.addBindValue(static_cast<int>(EventType::Course));
        event.addBindValue(std::get<3>(item));
        event.addBindValue(1);
        event.addBindValue(now);
        event.addBindValue(now);
        if (!event.exec()) {
            return false;
        }
    }

    QSqlQuery frame(db);
    frame.prepare(QStringLiteral(R"SQL(
        INSERT INTO study_frames(name, day_of_week, start_time, end_time, category_id, energy_level, enabled, created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));
    struct SeedFrame {
        QString name;
        int dayOfWeek;
        QString startTime;
        QString endTime;
        int category;
        QString energyLevel;
    };
    const QVector<SeedFrame> frames = {
        {QStringLiteral("Deep Study"), 1, QStringLiteral("19:00"), QStringLiteral("22:00"), 1, QStringLiteral("high")},
        {QStringLiteral("Reading"), 2, QStringLiteral("08:00"), QStringLiteral("09:30"), 2, QStringLiteral("medium")},
        {QStringLiteral("Deep Study"), 3, QStringLiteral("19:00"), QStringLiteral("22:00"), 1, QStringLiteral("high")},
        {QStringLiteral("Deep Study"), 5, QStringLiteral("19:00"), QStringLiteral("22:00"), 1, QStringLiteral("high")}
    };
    for (const auto& item : frames) {
        frame.addBindValue(item.name);
        frame.addBindValue(item.dayOfWeek);
        frame.addBindValue(item.startTime);
        frame.addBindValue(item.endTime);
        frame.addBindValue(item.category);
        frame.addBindValue(item.energyLevel);
        frame.addBindValue(1);
        frame.addBindValue(now);
        frame.addBindValue(now);
        if (!frame.exec()) {
            return false;
        }
    }

    QSqlQuery locale(db);
    locale.prepare(QStringLiteral("INSERT OR REPLACE INTO app_settings(key, value) VALUES('locale', 'zh_CN')"));
    return locale.exec();
}
