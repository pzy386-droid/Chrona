#include "database/Repositories.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QtGlobal>
#include <QVariant>
#include <algorithm>
#include <utility>

namespace {
QDateTime fromIso(const QVariant& value)
{
    return QDateTime::fromString(value.toString(), Qt::ISODate);
}

QString toIso(const QDateTime& value)
{
    return value.toString(Qt::ISODate);
}

bool rollbackWithError(QSqlDatabase db, QString& target, const QSqlQuery& query)
{
    target = query.lastError().text();
    db.rollback();
    return false;
}
}

SettingsRepository::SettingsRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QString SettingsRepository::value(const QString& key, const QString& fallback) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT value FROM app_settings WHERE key = ?"));
    query.addBindValue(key);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return fallback;
}

bool SettingsRepository::setValue(const QString& key, const QString& value) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO app_settings(key, value) VALUES(?, ?)"));
    query.addBindValue(key);
    query.addBindValue(value);
    return query.exec();
}

TaskRepository::TaskRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QVector<Task> TaskRepository::activeTasks() const
{
    QVector<Task> tasks = allTasks();
    tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [](const Task& task) {
        return task.status == TaskStatus::Done || task.status == TaskStatus::Archived;
    }), tasks.end());
    return tasks;
}

QVector<Task> TaskRepository::allTasks() const
{
    QVector<Task> tasks;
    QSqlQuery query(m_db);
    query.exec(QStringLiteral(R"SQL(
        SELECT t.id, t.title, t.notes, t.deadline, t.estimated_minutes, t.remaining_minutes,
               t.priority, t.status, t.category_id, c.name, COALESCE(NULLIF(t.color_override, ''), c.color), t.color_override,
               t.estimated_hours, t.preferred_study_time,
               t.start_date, t.deadline_type, t.auto_schedule_enabled, t.min_chunk_minutes,
               t.ideal_chunk_minutes, t.effort_level, t.schedule_status, t.completed_at
        FROM tasks t
        LEFT JOIN categories c ON c.id = t.category_id
        ORDER BY t.status ASC, t.deadline ASC
    )SQL"));
    while (query.next()) {
        Task task;
        task.id = query.value(0).toInt();
        task.title = query.value(1).toString();
        task.notes = query.value(2).toString();
        task.deadline = fromIso(query.value(3));
        task.estimatedMinutes = query.value(4).toInt();
        task.remainingMinutes = query.value(5).toInt();
        task.priority = static_cast<Priority>(query.value(6).toInt());
        task.status = static_cast<TaskStatus>(query.value(7).toInt());
        if (!query.value(8).isNull()) {
            task.categoryId = query.value(8).toInt();
        }
        task.categoryName = query.value(9).toString();
        task.categoryColor = query.value(10).toString();
        task.colorOverride = query.value(11).toString();
        task.estimatedHours = query.value(12).toDouble();
        task.preferredStudyTime = query.value(13).toString();
        task.startDate = fromIso(query.value(14));
        if (!task.startDate.isValid()) {
            task.startDate = QDateTime(QDate::currentDate(), QTime(0, 0));
        }
        task.deadlineType = static_cast<DeadlineType>(query.value(15).toInt());
        task.autoScheduleEnabled = query.value(16).toBool();
        task.minChunkMinutes = query.value(17).toInt();
        task.idealChunkMinutes = query.value(18).toInt();
        task.effortLevel = query.value(19).toInt();
        task.scheduleStatus = static_cast<ScheduleStatus>(query.value(20).toInt());
        task.completedAt = fromIso(query.value(21));
        tasks.push_back(task);
    }
    return tasks;
}

bool TaskRepository::createTask(const Task& task, const QString& categoryName) const
{
    return createTaskReturningId(task, categoryName) > 0;
}

int TaskRepository::createTaskReturningId(const Task& task, const QString& categoryName) const
{
    m_lastError.clear();
    QSqlDatabase db = m_db;
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return 0;
    }

    const int categoryId = ensureCategory(categoryName);
    if (categoryId <= 0) {
        db.rollback();
        return 0;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"SQL(
        INSERT INTO tasks(title, notes, start_date, deadline, deadline_type, estimated_minutes, estimated_hours, remaining_minutes, preferred_study_time,
                          auto_schedule_enabled, min_chunk_minutes, ideal_chunk_minutes, effort_level, schedule_status, priority, status, category_id, color_override, created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));
    query.addBindValue(task.title);
    query.addBindValue(task.notes);
    query.addBindValue(toIso(task.startDate.isValid() ? task.startDate : QDateTime(QDate::currentDate(), QTime(0, 0))));
    query.addBindValue(toIso(task.deadline));
    query.addBindValue(static_cast<int>(task.deadlineType));
    query.addBindValue(task.estimatedMinutes);
    query.addBindValue(task.estimatedMinutes / 60.0);
    query.addBindValue(qBound(0, task.remainingMinutes, task.estimatedMinutes));
    query.addBindValue(task.preferredStudyTime);
    query.addBindValue(task.autoScheduleEnabled ? 1 : 0);
    query.addBindValue(qMax(15, task.minChunkMinutes));
    query.addBindValue(qMax(15, task.idealChunkMinutes));
    query.addBindValue(qBound(0, task.effortLevel, 2));
    query.addBindValue(static_cast<int>(ScheduleStatus::Unscheduled));
    query.addBindValue(static_cast<int>(task.priority));
    query.addBindValue(static_cast<int>(TaskStatus::Inbox));
    query.addBindValue(categoryId > 0 ? QVariant(categoryId) : QVariant());
    query.addBindValue(task.colorOverride.trimmed().isEmpty() ? QVariant() : QVariant(task.colorOverride.trimmed()));
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    if (!query.exec()) {
        rollbackWithError(db, m_lastError, query);
        return 0;
    }

    const int taskId = query.lastInsertId().toInt();
    if (!db.commit()) {
        m_lastError = db.lastError().text();
        return 0;
    }
    return taskId;
}

bool TaskRepository::updateTask(const Task& task, const QString& categoryName) const
{
    m_lastError.clear();
    if (task.id <= 0 || task.title.trimmed().isEmpty() || !task.deadline.isValid() || task.estimatedMinutes <= 0) {
        m_lastError = QStringLiteral("Invalid task update");
        return false;
    }

    QSqlDatabase db = m_db;
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }
    const int categoryId = ensureCategory(categoryName);
    if (categoryId <= 0) {
        db.rollback();
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"SQL(
        UPDATE tasks
        SET title = ?, notes = ?, start_date = ?, deadline = ?, deadline_type = ?, estimated_minutes = ?, estimated_hours = ?,
            remaining_minutes = ?, preferred_study_time = ?, auto_schedule_enabled = ?, min_chunk_minutes = ?,
            ideal_chunk_minutes = ?, effort_level = ?, priority = ?, category_id = ?, color_override = ?, updated_at = ?
        WHERE id = ?
    )SQL"));
    query.addBindValue(task.title.trimmed());
    query.addBindValue(task.notes);
    query.addBindValue(toIso(task.startDate.isValid() ? task.startDate : QDateTime(QDate::currentDate(), QTime(0, 0))));
    query.addBindValue(toIso(task.deadline));
    query.addBindValue(static_cast<int>(task.deadlineType));
    query.addBindValue(task.estimatedMinutes);
    query.addBindValue(task.estimatedMinutes / 60.0);
    query.addBindValue(qBound(0, task.remainingMinutes, task.estimatedMinutes));
    query.addBindValue(task.preferredStudyTime);
    query.addBindValue(task.autoScheduleEnabled ? 1 : 0);
    query.addBindValue(qMax(15, task.minChunkMinutes));
    query.addBindValue(qMax(15, task.idealChunkMinutes));
    query.addBindValue(qBound(0, task.effortLevel, 2));
    query.addBindValue(static_cast<int>(task.priority));
    query.addBindValue(categoryId > 0 ? QVariant(categoryId) : QVariant());
    query.addBindValue(task.colorOverride.trimmed().isEmpty() ? QVariant() : QVariant(task.colorOverride.trimmed()));
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(task.id);
    if (!query.exec() || query.numRowsAffected() != 1) {
        return rollbackWithError(db, m_lastError, query);
    }
    if (!db.commit()) {
        m_lastError = db.lastError().text();
        return false;
    }
    return true;
}

bool TaskRepository::updateCategoryColor(const QString& categoryName, const QString& color) const
{
    m_lastError.clear();
    const QString normalizedName = categoryName.trimmed();
    const QString normalizedColor = color.trimmed();
    if (normalizedName.isEmpty() || normalizedColor.isEmpty()) {
        m_lastError = QStringLiteral("Category name and color are required");
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE categories SET color = ? WHERE name = ?"));
    query.addBindValue(normalizedColor);
    query.addBindValue(normalizedName);
    if (!query.exec() || query.numRowsAffected() != 1) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool TaskRepository::updateScheduleStatuses(const QVector<int>& scheduledTaskIds, const QVector<int>& couldNotFitTaskIds) const
{
    QSqlDatabase db = m_db;
    if (!db.transaction()) {
        return false;
    }

    QSqlQuery reset(db);
    reset.prepare(QStringLiteral("UPDATE tasks SET schedule_status = ?, updated_at = ? WHERE status NOT IN (?, ?)"));
    reset.addBindValue(static_cast<int>(ScheduleStatus::Unscheduled));
    reset.addBindValue(toIso(QDateTime::currentDateTime()));
    reset.addBindValue(static_cast<int>(TaskStatus::Done));
    reset.addBindValue(static_cast<int>(TaskStatus::Archived));
    if (!reset.exec()) {
        db.rollback();
        return false;
    }

    const QString now = toIso(QDateTime::currentDateTime());
    auto updateOne = [&](int id, ScheduleStatus status) {
        QSqlQuery update(db);
        update.prepare(QStringLiteral("UPDATE tasks SET schedule_status = ?, updated_at = ? WHERE id = ?"));
        update.addBindValue(static_cast<int>(status));
        update.addBindValue(now);
        update.addBindValue(id);
        return update.exec();
    };
    for (const int id : scheduledTaskIds) {
        if (!updateOne(id, ScheduleStatus::Scheduled)) {
            db.rollback();
            return false;
        }
    }
    for (const int id : couldNotFitTaskIds) {
        if (!updateOne(id, ScheduleStatus::CouldNotFit)) {
            db.rollback();
            return false;
        }
    }

    return db.commit();
}

bool TaskRepository::completeTask(int taskId) const
{
    QSqlDatabase db = m_db;
    if (!db.transaction()) {
        return false;
    }

    const QString now = toIso(QDateTime::currentDateTime());
    QSqlQuery blocks(db);
    blocks.prepare(QStringLiteral("UPDATE time_blocks SET completed_at = ? WHERE task_id = ? AND completed_at IS NULL"));
    blocks.addBindValue(now);
    blocks.addBindValue(taskId);
    if (!blocks.exec()) {
        db.rollback();
        return false;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral("UPDATE tasks SET status = ?, remaining_minutes = 0, completed_at = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(static_cast<int>(TaskStatus::Done));
    query.addBindValue(now);
    query.addBindValue(now);
    query.addBindValue(taskId);
    if (!query.exec()) {
        db.rollback();
        return false;
    }
    return db.commit();
}

bool TaskRepository::archiveTask(int taskId) const
{
    if (taskId <= 0) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE tasks SET status = ?, auto_schedule_enabled = 0, updated_at = ? WHERE id = ?"));
    query.addBindValue(static_cast<int>(TaskStatus::Archived));
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(taskId);
    return query.exec() && query.numRowsAffected() == 1;
}

bool TaskRepository::deleteTask(int taskId) const
{
    if (taskId <= 0) {
        return false;
    }

    QSqlDatabase db = m_db;
    if (!db.transaction()) {
        return false;
    }

    QSqlQuery removeBlocks(db);
    removeBlocks.prepare(QStringLiteral("DELETE FROM time_blocks WHERE task_id = ?"));
    removeBlocks.addBindValue(taskId);
    if (!removeBlocks.exec()) {
        db.rollback();
        return false;
    }

    QSqlQuery removeTask(db);
    removeTask.prepare(QStringLiteral("DELETE FROM tasks WHERE id = ?"));
    removeTask.addBindValue(taskId);
    if (!removeTask.exec() || removeTask.numRowsAffected() != 1) {
        db.rollback();
        return false;
    }

    return db.commit();
}

int TaskRepository::ensureCategory(const QString& name) const
{
    const QString normalized = name.trimmed().isEmpty() ? QStringLiteral("学习") : name.trimmed();
    QSqlQuery select(m_db);
    select.prepare(QStringLiteral("SELECT id FROM categories WHERE name = ?"));
    select.addBindValue(normalized);
    if (select.exec() && select.next()) {
        return select.value(0).toInt();
    }

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral("INSERT INTO categories(name, color) VALUES(?, ?)"));
    insert.addBindValue(normalized);
    insert.addBindValue(QStringLiteral("#7C8CFF"));
    if (!insert.exec()) {
        m_lastError = insert.lastError().text();
        return 0;
    }
    return insert.lastInsertId().toInt();
}

QString TaskRepository::lastError() const
{
    return m_lastError;
}

CalendarRepository::CalendarRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QVector<CalendarEvent> CalendarRepository::eventsBetween(const QDateTime& start, const QDateTime& end) const
{
    QVector<CalendarEvent> events;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"SQL(
        SELECT e.id, e.title, e.start_time, e.end_time, e.type, e.category_id, c.name,
               COALESCE(NULLIF(e.color_override, ''), c.color), e.color_override, e.is_locked
        FROM calendar_events e
        LEFT JOIN categories c ON c.id = e.category_id
        WHERE e.end_time > ? AND e.start_time < ?
        ORDER BY e.start_time ASC
    )SQL"));
    query.addBindValue(toIso(start));
    query.addBindValue(toIso(end));
    query.exec();
    while (query.next()) {
        CalendarEvent event;
        event.id = query.value(0).toInt();
        event.title = query.value(1).toString();
        event.start = fromIso(query.value(2));
        event.end = fromIso(query.value(3));
        event.type = static_cast<EventType>(query.value(4).toInt());
        if (!query.value(5).isNull()) {
            event.categoryId = query.value(5).toInt();
        }
        event.categoryName = query.value(6).toString();
        event.categoryColor = query.value(7).toString();
        event.colorOverride = query.value(8).toString();
        event.locked = query.value(9).toBool();
        events.push_back(event);
    }
    return events;
}

bool CalendarRepository::createEvent(const CalendarEvent& event, const QString& categoryName) const
{
    m_lastError.clear();
    if (!event.start.isValid() || !event.end.isValid() || event.end <= event.start || event.title.trimmed().isEmpty()) {
        m_lastError = QStringLiteral("Invalid calendar event");
        return false;
    }

    QSqlDatabase db = m_db;
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }
    const int categoryId = ensureCategory(categoryName);
    if (categoryId <= 0) {
        db.rollback();
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"SQL(
        INSERT INTO calendar_events(title, start_time, end_time, type, category_id, color_override, is_locked, created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));
    query.addBindValue(event.title.trimmed());
    query.addBindValue(toIso(event.start));
    query.addBindValue(toIso(event.end));
    query.addBindValue(static_cast<int>(event.type));
    query.addBindValue(categoryId > 0 ? QVariant(categoryId) : QVariant());
    query.addBindValue(event.colorOverride.trimmed().isEmpty() ? QVariant() : QVariant(event.colorOverride.trimmed()));
    query.addBindValue(event.locked ? 1 : 0);
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    if (!query.exec()) {
        return rollbackWithError(db, m_lastError, query);
    }
    if (!db.commit()) {
        m_lastError = db.lastError().text();
        return false;
    }
    return true;
}

bool CalendarRepository::updateEvent(const CalendarEvent& event, const QString& categoryName) const
{
    m_lastError.clear();
    if (event.id <= 0 || !event.start.isValid() || !event.end.isValid() || event.end <= event.start || event.title.trimmed().isEmpty()) {
        m_lastError = QStringLiteral("Invalid calendar event update");
        return false;
    }

    QSqlDatabase db = m_db;
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }
    const int categoryId = ensureCategory(categoryName);
    if (categoryId <= 0) {
        db.rollback();
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"SQL(
        UPDATE calendar_events
        SET title = ?, start_time = ?, end_time = ?, category_id = ?, color_override = ?, is_locked = ?, updated_at = ?
        WHERE id = ?
    )SQL"));
    query.addBindValue(event.title.trimmed());
    query.addBindValue(toIso(event.start));
    query.addBindValue(toIso(event.end));
    query.addBindValue(categoryId > 0 ? QVariant(categoryId) : QVariant());
    query.addBindValue(event.colorOverride.trimmed().isEmpty() ? QVariant() : QVariant(event.colorOverride.trimmed()));
    query.addBindValue(event.locked ? 1 : 0);
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(event.id);
    if (!query.exec() || query.numRowsAffected() != 1) {
        return rollbackWithError(db, m_lastError, query);
    }
    if (!db.commit()) {
        m_lastError = db.lastError().text();
        return false;
    }
    return true;
}

bool CalendarRepository::deleteEvent(int eventId) const
{
    if (eventId <= 0) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM calendar_events WHERE id = ?"));
    query.addBindValue(eventId);
    return query.exec() && query.numRowsAffected() == 1;
}

bool CalendarRepository::setEventLocked(int eventId, bool locked) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE calendar_events SET is_locked = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(locked ? 1 : 0);
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(eventId);
    return query.exec() && query.numRowsAffected() == 1;
}

bool CalendarRepository::moveEvent(int eventId, const QDateTime& start, const QDateTime& end) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE calendar_events SET start_time = ?, end_time = ?, updated_at = ? WHERE id = ? AND is_locked = 0"));
    query.addBindValue(toIso(start));
    query.addBindValue(toIso(end));
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(eventId);
    return query.exec() && query.numRowsAffected() == 1;
}

bool CalendarRepository::setCategoryColor(const QString& categoryName, const QString& color) const
{
    const QString normalized = categoryName.trimmed().isEmpty() ? QStringLiteral("课程") : categoryName.trimmed();
    const QString normalizedColor = color.trimmed();
    if (normalizedColor.isEmpty()) {
        return false;
    }

    QSqlQuery select(m_db);
    select.prepare(QStringLiteral("SELECT id FROM categories WHERE name = ?"));
    select.addBindValue(normalized);
    if (select.exec() && select.next()) {
        QSqlQuery update(m_db);
        update.prepare(QStringLiteral("UPDATE categories SET color = ? WHERE id = ?"));
        update.addBindValue(normalizedColor);
        update.addBindValue(select.value(0).toInt());
        return update.exec();
    }

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral("INSERT INTO categories(name, color) VALUES(?, ?)"));
    insert.addBindValue(normalized);
    insert.addBindValue(normalizedColor);
    return insert.exec();
}

int CalendarRepository::ensureCategory(const QString& name) const
{
    const QString normalized = name.trimmed().isEmpty() ? QStringLiteral("课程") : name.trimmed();
    QSqlQuery select(m_db);
    select.prepare(QStringLiteral("SELECT id FROM categories WHERE name = ?"));
    select.addBindValue(normalized);
    if (select.exec() && select.next()) {
        return select.value(0).toInt();
    }

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral("INSERT INTO categories(name, color) VALUES(?, ?)"));
    insert.addBindValue(normalized);
    insert.addBindValue(QStringLiteral("#6FD6A7"));
    if (!insert.exec()) {
        m_lastError = insert.lastError().text();
        return 0;
    }
    return insert.lastInsertId().toInt();
}

bool CalendarRepository::upsertEvents(const QVector<CalendarEvent>& events, const QString& categoryName,
                                      const QVector<int>& removeEventIds, QVector<int>* eventIds) const
{
    m_lastError.clear();
    QSqlDatabase db = m_db;
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }
    const int categoryId = ensureCategory(categoryName);
    if (categoryId <= 0) {
        db.rollback();
        return false;
    }
    for (int eventId : removeEventIds) {
        QSqlQuery remove(db);
        remove.prepare(QStringLiteral("DELETE FROM calendar_events WHERE id = ?"));
        remove.addBindValue(eventId);
        if (!remove.exec() || remove.numRowsAffected() != 1) {
            return rollbackWithError(db, m_lastError, remove);
        }
    }

    QVector<int> ids;
    ids.reserve(events.size());
    const QString now = toIso(QDateTime::currentDateTime());
    for (const CalendarEvent& event : events) {
        if (event.title.trimmed().isEmpty() || !event.start.isValid() || !event.end.isValid() || event.end <= event.start) {
            m_lastError = QStringLiteral("Invalid calendar event in batch");
            db.rollback();
            return false;
        }

        QSqlQuery query(db);
        if (event.id > 0) {
            query.prepare(QStringLiteral(R"SQL(
                UPDATE calendar_events
                SET title = ?, start_time = ?, end_time = ?, type = ?, category_id = ?,
                    color_override = ?, is_locked = ?, updated_at = ?
                WHERE id = ?
            )SQL"));
        } else {
            query.prepare(QStringLiteral(R"SQL(
                INSERT INTO calendar_events(title, start_time, end_time, type, category_id,
                                            color_override, is_locked, created_at, updated_at)
                VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)
            )SQL"));
        }
        query.addBindValue(event.title.trimmed());
        query.addBindValue(toIso(event.start));
        query.addBindValue(toIso(event.end));
        query.addBindValue(static_cast<int>(event.type));
        query.addBindValue(categoryId);
        query.addBindValue(event.colorOverride.trimmed().isEmpty() ? QVariant() : QVariant(event.colorOverride.trimmed()));
        query.addBindValue(event.locked ? 1 : 0);
        query.addBindValue(now);
        if (event.id > 0) {
            query.addBindValue(event.id);
        } else {
            query.addBindValue(now);
        }
        if (!query.exec() || (event.id > 0 && query.numRowsAffected() != 1)) {
            return rollbackWithError(db, m_lastError, query);
        }
        ids.push_back(event.id > 0 ? event.id : query.lastInsertId().toInt());
    }

    if (!db.commit()) {
        m_lastError = db.lastError().text();
        return false;
    }
    if (eventIds) {
        *eventIds = ids;
    }
    return true;
}

QString CalendarRepository::lastError() const
{
    return m_lastError;
}

TimeBlockRepository::TimeBlockRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QVector<TimeBlock> TimeBlockRepository::blocksBetween(const QDateTime& start, const QDateTime& end) const
{
    QVector<TimeBlock> blocks;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"SQL(
        SELECT id, task_id, start_time, end_time, source, schedule_run_id, created_at, explanation, completed_at
        FROM time_blocks
        WHERE end_time > ? AND start_time < ?
        ORDER BY start_time ASC
    )SQL"));
    query.addBindValue(toIso(start));
    query.addBindValue(toIso(end));
    query.exec();
    while (query.next()) {
        TimeBlock block;
        block.id = query.value(0).toInt();
        block.taskId = query.value(1).toInt();
        block.start = fromIso(query.value(2));
        block.end = fromIso(query.value(3));
        block.source = static_cast<BlockSource>(query.value(4).toInt());
        block.scheduleRunId = query.value(5).toInt();
        block.createdAt = query.value(6).toString();
        block.explanation = query.value(7).toString();
        block.completedAt = fromIso(query.value(8));
        blocks.push_back(block);
    }
    return blocks;
}

QVector<TimeBlock> TimeBlockRepository::lockedBlocksBetween(const QDateTime& start, const QDateTime& end) const
{
    QVector<TimeBlock> blocks = blocksBetween(start, end);
    blocks.erase(std::remove_if(blocks.begin(), blocks.end(), [](const TimeBlock& block) {
        return block.source == BlockSource::Auto && !block.completedAt.isValid();
    }), blocks.end());
    return blocks;
}

int TimeBlockRepository::createScheduleRun(const ScheduleWindow& window, const QString& reason) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"SQL(
        INSERT INTO schedule_runs(started_at, horizon_start, horizon_end, reason)
        VALUES(?, ?, ?, ?)
    )SQL"));
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(toIso(window.start));
    query.addBindValue(toIso(window.end));
    query.addBindValue(reason);
    if (!query.exec()) {
        return 0;
    }
    return query.lastInsertId().toInt();
}

bool TimeBlockRepository::replaceAutoBlocks(const ScheduleWindow& window, const QVector<TimeBlock>& blocks, int scheduleRunId) const
{
    QSqlDatabase db = m_db;
    if (!db.transaction()) {
        return false;
    }

    QSqlQuery remove(db);
    remove.prepare(QStringLiteral("DELETE FROM time_blocks WHERE source = ? AND completed_at IS NULL AND end_time > ? AND start_time < ?"));
    remove.addBindValue(static_cast<int>(BlockSource::Auto));
    remove.addBindValue(toIso(window.start));
    remove.addBindValue(toIso(window.end));
    if (!remove.exec()) {
        db.rollback();
        return false;
    }

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(R"SQL(
        INSERT INTO time_blocks(task_id, start_time, end_time, source, schedule_run_id, explanation, completed_at, created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));
    for (const auto& block : blocks) {
        const QString now = toIso(QDateTime::currentDateTime());
        insert.bindValue(0, block.taskId);
        insert.bindValue(1, toIso(block.start));
        insert.bindValue(2, toIso(block.end));
        insert.bindValue(3, static_cast<int>(block.source));
        insert.bindValue(4, scheduleRunId);
        insert.bindValue(5, block.explanation);
        insert.bindValue(6, block.completedAt.isValid() ? QVariant(toIso(block.completedAt)) : QVariant());
        insert.bindValue(7, now);
        insert.bindValue(8, now);
        if (!insert.exec()) {
            db.rollback();
            return false;
        }
    }

    return db.commit();
}

int TimeBlockRepository::createBlock(const TimeBlock& block) const
{
    if (block.taskId <= 0 || !block.start.isValid() || !block.end.isValid() || block.end <= block.start) {
        return 0;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"SQL(
        INSERT INTO time_blocks(task_id, start_time, end_time, source, schedule_run_id, explanation, completed_at, created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));
    query.addBindValue(block.taskId);
    query.addBindValue(toIso(block.start));
    query.addBindValue(toIso(block.end));
    query.addBindValue(static_cast<int>(block.source));
    query.addBindValue(block.scheduleRunId > 0 ? QVariant(block.scheduleRunId) : QVariant());
    query.addBindValue(block.explanation);
    query.addBindValue(block.completedAt.isValid() ? QVariant(toIso(block.completedAt)) : QVariant());
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    if (!query.exec()) {
        return 0;
    }
    return query.lastInsertId().toInt();
}

bool TimeBlockRepository::moveBlock(int blockId, const QDateTime& start, const QDateTime& end) const
{
    if (blockId <= 0 || !start.isValid() || !end.isValid() || end <= start) {
        m_lastError = QStringLiteral("Invalid time block move");
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE time_blocks SET start_time = ?, end_time = ?, source = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(toIso(start));
    query.addBindValue(toIso(end));
    query.addBindValue(static_cast<int>(BlockSource::UserDragged));
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(blockId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

StudyFrameRepository::StudyFrameRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QVector<StudyFrame> StudyFrameRepository::allFrames() const
{
    QVector<StudyFrame> frames;
    QSqlQuery query(m_db);
    query.exec(QStringLiteral(R"SQL(
        SELECT f.id, f.name, f.day_of_week, f.start_time, f.end_time, f.category_id,
               c.name, c.color, f.energy_level, f.enabled
        FROM study_frames f
        LEFT JOIN categories c ON c.id = f.category_id
        ORDER BY f.day_of_week ASC, f.start_time ASC
    )SQL"));
    while (query.next()) {
        StudyFrame frame;
        frame.id = query.value(0).toInt();
        frame.name = query.value(1).toString();
        frame.dayOfWeek = query.value(2).toInt();
        frame.startTime = QTime::fromString(query.value(3).toString(), QStringLiteral("HH:mm"));
        frame.endTime = QTime::fromString(query.value(4).toString(), QStringLiteral("HH:mm"));
        if (!query.value(5).isNull()) {
            frame.categoryId = query.value(5).toInt();
        }
        frame.categoryName = query.value(6).toString();
        frame.categoryColor = query.value(7).toString();
        frame.energyLevel = query.value(8).toString();
        frame.enabled = query.value(9).toBool();
        frames.push_back(frame);
    }
    return frames;
}

QVector<StudyFrame> StudyFrameRepository::enabledFrames() const
{
    QVector<StudyFrame> frames = allFrames();
    frames.erase(std::remove_if(frames.begin(), frames.end(), [](const StudyFrame& frame) {
        return !frame.enabled;
    }), frames.end());
    return frames;
}

bool StudyFrameRepository::createFrame(const StudyFrame& frame, const QString& categoryName) const
{
    if (frame.name.trimmed().isEmpty() || frame.dayOfWeek < 1 || frame.dayOfWeek > 7 || !frame.startTime.isValid() || !frame.endTime.isValid() || frame.endTime <= frame.startTime) {
        return false;
    }

    const int categoryId = ensureCategory(categoryName);
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"SQL(
        INSERT INTO study_frames(name, day_of_week, start_time, end_time, category_id, energy_level, enabled, created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));
    query.addBindValue(frame.name.trimmed());
    query.addBindValue(frame.dayOfWeek);
    query.addBindValue(frame.startTime.toString(QStringLiteral("HH:mm")));
    query.addBindValue(frame.endTime.toString(QStringLiteral("HH:mm")));
    query.addBindValue(categoryId > 0 ? QVariant(categoryId) : QVariant());
    query.addBindValue(frame.energyLevel.trimmed().isEmpty() ? QStringLiteral("medium") : frame.energyLevel.trimmed());
    query.addBindValue(frame.enabled ? 1 : 0);
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    return query.exec();
}

bool StudyFrameRepository::setEnabled(int frameId, bool enabled) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE study_frames SET enabled = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(enabled ? 1 : 0);
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(frameId);
    return query.exec() && query.numRowsAffected() == 1;
}

bool StudyFrameRepository::deleteFrame(int frameId) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM study_frames WHERE id = ?"));
    query.addBindValue(frameId);
    return query.exec() && query.numRowsAffected() == 1;
}

int StudyFrameRepository::ensureCategory(const QString& name) const
{
    const QString normalized = name.trimmed();
    if (normalized.isEmpty()) {
        return 0;
    }

    QSqlQuery select(m_db);
    select.prepare(QStringLiteral("SELECT id FROM categories WHERE name = ?"));
    select.addBindValue(normalized);
    if (select.exec() && select.next()) {
        return select.value(0).toInt();
    }

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral("INSERT INTO categories(name, color) VALUES(?, ?)"));
    insert.addBindValue(normalized);
    insert.addBindValue(QStringLiteral("#7C8CFF"));
    if (!insert.exec()) {
        return 0;
    }
    return insert.lastInsertId().toInt();
}

bool TimeBlockRepository::setBlockSource(int blockId, BlockSource source) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE time_blocks SET source = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(static_cast<int>(source));
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(blockId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool TimeBlockRepository::upsertBlocks(const QVector<TimeBlock>& blocks, const QVector<int>& removeBlockIds,
                                       QVector<int>* blockIds) const
{
    m_lastError.clear();
    QSqlDatabase db = m_db;
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }
    for (int blockId : removeBlockIds) {
        QSqlQuery remove(db);
        remove.prepare(QStringLiteral("DELETE FROM time_blocks WHERE id = ?"));
        remove.addBindValue(blockId);
        if (!remove.exec() || remove.numRowsAffected() != 1) {
            return rollbackWithError(db, m_lastError, remove);
        }
    }

    QVector<int> ids;
    ids.reserve(blocks.size());
    const QString now = toIso(QDateTime::currentDateTime());
    for (const TimeBlock& block : blocks) {
        if (block.taskId <= 0 || !block.start.isValid() || !block.end.isValid() || block.end <= block.start) {
            m_lastError = QStringLiteral("Invalid time block in batch");
            db.rollback();
            return false;
        }

        QSqlQuery query(db);
        if (block.id > 0) {
            query.prepare(QStringLiteral(R"SQL(
                UPDATE time_blocks
                SET task_id = ?, start_time = ?, end_time = ?, source = ?, explanation = ?,
                    completed_at = ?, updated_at = ?
                WHERE id = ?
            )SQL"));
        } else {
            query.prepare(QStringLiteral(R"SQL(
                INSERT INTO time_blocks(task_id, start_time, end_time, source, schedule_run_id,
                                        explanation, completed_at, created_at, updated_at)
                VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)
            )SQL"));
        }
        query.addBindValue(block.taskId);
        query.addBindValue(toIso(block.start));
        query.addBindValue(toIso(block.end));
        query.addBindValue(static_cast<int>(block.source));
        if (block.id <= 0) {
            query.addBindValue(block.scheduleRunId > 0 ? QVariant(block.scheduleRunId) : QVariant());
        }
        query.addBindValue(block.explanation);
        query.addBindValue(block.completedAt.isValid() ? QVariant(toIso(block.completedAt)) : QVariant());
        if (block.id <= 0) {
            query.addBindValue(now);
        }
        query.addBindValue(now);
        if (block.id > 0) {
            query.addBindValue(block.id);
        }
        if (!query.exec() || (block.id > 0 && query.numRowsAffected() != 1)) {
            return rollbackWithError(db, m_lastError, query);
        }
        ids.push_back(block.id > 0 ? block.id : query.lastInsertId().toInt());
    }

    if (!db.commit()) {
        m_lastError = db.lastError().text();
        return false;
    }
    if (blockIds) {
        *blockIds = ids;
    }
    return true;
}

bool TimeBlockRepository::commitSchedule(const ScheduleWindow& window, const QVector<TimeBlock>& blocks,
                                         const QVector<int>& scheduledTaskIds, const QVector<int>& couldNotFitTaskIds,
                                         const QString& reason, int* scheduleRunId) const
{
    m_lastError.clear();
    if (!window.start.isValid() || !window.end.isValid() || window.end <= window.start) {
        m_lastError = QStringLiteral("Invalid schedule window");
        return false;
    }

    QSqlDatabase db = m_db;
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }
    const QString now = toIso(QDateTime::currentDateTime());

    QSqlQuery run(db);
    run.prepare(QStringLiteral(R"SQL(
        INSERT INTO schedule_runs(started_at, horizon_start, horizon_end, reason)
        VALUES(?, ?, ?, ?)
    )SQL"));
    run.addBindValue(now);
    run.addBindValue(toIso(window.start));
    run.addBindValue(toIso(window.end));
    run.addBindValue(reason);
    if (!run.exec()) {
        return rollbackWithError(db, m_lastError, run);
    }
    const int runId = run.lastInsertId().toInt();

    QSqlQuery remove(db);
    remove.prepare(QStringLiteral(R"SQL(
        DELETE FROM time_blocks
        WHERE source = ? AND completed_at IS NULL AND end_time > ? AND start_time < ?
    )SQL"));
    remove.addBindValue(static_cast<int>(BlockSource::Auto));
    remove.addBindValue(toIso(window.start));
    remove.addBindValue(toIso(window.end));
    if (!remove.exec()) {
        return rollbackWithError(db, m_lastError, remove);
    }

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(R"SQL(
        INSERT INTO time_blocks(task_id, start_time, end_time, source, schedule_run_id,
                                explanation, completed_at, created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));
    for (const TimeBlock& block : blocks) {
        if (block.taskId <= 0 || !block.start.isValid() || !block.end.isValid() || block.end <= block.start) {
            m_lastError = QStringLiteral("Invalid generated time block");
            db.rollback();
            return false;
        }
        insert.bindValue(0, block.taskId);
        insert.bindValue(1, toIso(block.start));
        insert.bindValue(2, toIso(block.end));
        insert.bindValue(3, static_cast<int>(block.source));
        insert.bindValue(4, runId);
        insert.bindValue(5, block.explanation);
        insert.bindValue(6, block.completedAt.isValid() ? QVariant(toIso(block.completedAt)) : QVariant());
        insert.bindValue(7, now);
        insert.bindValue(8, now);
        if (!insert.exec()) {
            return rollbackWithError(db, m_lastError, insert);
        }
    }

    QSqlQuery reset(db);
    reset.prepare(QStringLiteral("UPDATE tasks SET schedule_status = ?, updated_at = ? WHERE status NOT IN (?, ?)"));
    reset.addBindValue(static_cast<int>(ScheduleStatus::Unscheduled));
    reset.addBindValue(now);
    reset.addBindValue(static_cast<int>(TaskStatus::Done));
    reset.addBindValue(static_cast<int>(TaskStatus::Archived));
    if (!reset.exec()) {
        return rollbackWithError(db, m_lastError, reset);
    }

    auto updateStatus = [&](int taskId, ScheduleStatus status) {
        QSqlQuery update(db);
        update.prepare(QStringLiteral("UPDATE tasks SET schedule_status = ?, updated_at = ? WHERE id = ?"));
        update.addBindValue(static_cast<int>(status));
        update.addBindValue(now);
        update.addBindValue(taskId);
        if (!update.exec() || update.numRowsAffected() != 1) {
            m_lastError = update.lastError().text();
            return false;
        }
        return true;
    };
    for (int taskId : scheduledTaskIds) {
        if (!updateStatus(taskId, ScheduleStatus::Scheduled)) {
            db.rollback();
            return false;
        }
    }
    for (int taskId : couldNotFitTaskIds) {
        if (!updateStatus(taskId, ScheduleStatus::CouldNotFit)) {
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        m_lastError = db.lastError().text();
        return false;
    }
    if (scheduleRunId) {
        *scheduleRunId = runId;
    }
    return true;
}

QString TimeBlockRepository::lastError() const
{
    return m_lastError;
}

bool TimeBlockRepository::completeBlock(int blockId) const
{
    return setBlockCompleted(blockId, true);
}

bool TimeBlockRepository::setBlockCompleted(int blockId, bool completed) const
{
    if (blockId <= 0) {
        return false;
    }

    QSqlDatabase db = m_db;
    if (!db.transaction()) {
        return false;
    }

    QSqlQuery blockQuery(db);
    blockQuery.prepare(QStringLiteral("SELECT task_id FROM time_blocks WHERE id = ?"));
    blockQuery.addBindValue(blockId);
    if (!blockQuery.exec() || !blockQuery.next()) {
        db.rollback();
        return false;
    }

    const int taskId = blockQuery.value(0).toInt();
    const QString now = toIso(QDateTime::currentDateTime());
    QSqlQuery updateBlock(db);
    updateBlock.prepare(QStringLiteral("UPDATE time_blocks SET completed_at = ?, updated_at = ? WHERE id = ?"));
    updateBlock.addBindValue(completed ? QVariant(now) : QVariant());
    updateBlock.addBindValue(now);
    updateBlock.addBindValue(blockId);
    if (!updateBlock.exec() || updateBlock.numRowsAffected() != 1) {
        db.rollback();
        return false;
    }

    QSqlQuery totals(db);
    totals.prepare(QStringLiteral(R"SQL(
        SELECT t.estimated_minutes,
               COALESCE(SUM((julianday(b.end_time) - julianday(b.start_time)) * 24 * 60), 0)
        FROM tasks t
        LEFT JOIN time_blocks b ON b.task_id = t.id AND b.completed_at IS NOT NULL
        WHERE t.id = ?
        GROUP BY t.id, t.estimated_minutes
    )SQL"));
    totals.addBindValue(taskId);
    if (!totals.exec() || !totals.next()) {
        db.rollback();
        return false;
    }

    const int estimatedMinutes = totals.value(0).toInt();
    const int completedMinutes = qRound(totals.value(1).toDouble());
    const int remainingMinutes = qMax(0, estimatedMinutes - completedMinutes);
    const bool taskDone = remainingMinutes <= 0;

    QSqlQuery updateTask(db);
    updateTask.prepare(QStringLiteral(R"SQL(
        UPDATE tasks
        SET status = ?, remaining_minutes = ?, completed_at = ?, updated_at = ?
        WHERE id = ?
    )SQL"));
    updateTask.addBindValue(static_cast<int>(taskDone ? TaskStatus::Done : TaskStatus::Inbox));
    updateTask.addBindValue(remainingMinutes);
    updateTask.addBindValue(taskDone ? QVariant(now) : QVariant());
    updateTask.addBindValue(now);
    updateTask.addBindValue(taskId);
    if (!updateTask.exec() || updateTask.numRowsAffected() != 1) {
        db.rollback();
        return false;
    }

    return db.commit();
}

bool TimeBlockRepository::deleteBlock(int blockId) const
{
    if (blockId <= 0) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM time_blocks WHERE id = ?"));
    query.addBindValue(blockId);
    return query.exec() && query.numRowsAffected() == 1;
}
