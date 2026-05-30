#include "database/Repositories.h"

#include <QDateTime>
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
               t.priority, t.status, t.category_id, c.name, c.color, t.estimated_hours, t.preferred_study_time
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
        task.estimatedHours = query.value(11).toDouble();
        task.preferredStudyTime = query.value(12).toString();
        tasks.push_back(task);
    }
    return tasks;
}

bool TaskRepository::createTask(const Task& task, const QString& categoryName) const
{
    const int categoryId = ensureCategory(categoryName);
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"SQL(
        INSERT INTO tasks(title, notes, deadline, estimated_minutes, estimated_hours, remaining_minutes, preferred_study_time, priority, status, category_id, created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));
    query.addBindValue(task.title);
    query.addBindValue(task.notes);
    query.addBindValue(toIso(task.deadline));
    query.addBindValue(task.estimatedMinutes);
    query.addBindValue(task.estimatedMinutes / 60.0);
    query.addBindValue(task.estimatedMinutes);
    query.addBindValue(task.preferredStudyTime);
    query.addBindValue(static_cast<int>(task.priority));
    query.addBindValue(static_cast<int>(TaskStatus::Inbox));
    query.addBindValue(categoryId > 0 ? QVariant(categoryId) : QVariant());
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    return query.exec();
}

bool TaskRepository::updateTask(const Task& task, const QString& categoryName) const
{
    if (task.id <= 0 || task.title.trimmed().isEmpty() || !task.deadline.isValid() || task.estimatedMinutes <= 0) {
        return false;
    }

    const int categoryId = ensureCategory(categoryName);
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"SQL(
        UPDATE tasks
        SET title = ?, notes = ?, deadline = ?, estimated_minutes = ?, estimated_hours = ?,
            remaining_minutes = ?, preferred_study_time = ?, priority = ?, category_id = ?, updated_at = ?
        WHERE id = ?
    )SQL"));
    query.addBindValue(task.title.trimmed());
    query.addBindValue(task.notes);
    query.addBindValue(toIso(task.deadline));
    query.addBindValue(task.estimatedMinutes);
    query.addBindValue(task.estimatedMinutes / 60.0);
    query.addBindValue(qBound(0, task.remainingMinutes, task.estimatedMinutes));
    query.addBindValue(task.preferredStudyTime);
    query.addBindValue(static_cast<int>(task.priority));
    query.addBindValue(categoryId > 0 ? QVariant(categoryId) : QVariant());
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(task.id);
    return query.exec() && query.numRowsAffected() == 1;
}

bool TaskRepository::completeTask(int taskId) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE tasks SET status = ?, remaining_minutes = 0, updated_at = ? WHERE id = ?"));
    query.addBindValue(static_cast<int>(TaskStatus::Done));
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(taskId);
    return query.exec();
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
        return 0;
    }
    return insert.lastInsertId().toInt();
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
        SELECT e.id, e.title, e.start_time, e.end_time, e.type, e.category_id, c.color, e.is_locked
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
        event.categoryColor = query.value(6).toString();
        event.locked = query.value(7).toBool();
        events.push_back(event);
    }
    return events;
}

bool CalendarRepository::createEvent(const CalendarEvent& event, const QString& categoryName) const
{
    if (!event.start.isValid() || !event.end.isValid() || event.end <= event.start || event.title.trimmed().isEmpty()) {
        return false;
    }

    const int categoryId = ensureCategory(categoryName);
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"SQL(
        INSERT INTO calendar_events(title, start_time, end_time, type, category_id, is_locked, created_at, updated_at)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));
    query.addBindValue(event.title.trimmed());
    query.addBindValue(toIso(event.start));
    query.addBindValue(toIso(event.end));
    query.addBindValue(static_cast<int>(event.type));
    query.addBindValue(categoryId > 0 ? QVariant(categoryId) : QVariant());
    query.addBindValue(event.locked ? 1 : 0);
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    query.addBindValue(toIso(QDateTime::currentDateTime()));
    return query.exec();
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
        return 0;
    }
    return insert.lastInsertId().toInt();
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
        SELECT id, task_id, start_time, end_time, source, schedule_run_id, created_at
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
        blocks.push_back(block);
    }
    return blocks;
}

QVector<TimeBlock> TimeBlockRepository::lockedBlocksBetween(const QDateTime& start, const QDateTime& end) const
{
    QVector<TimeBlock> blocks = blocksBetween(start, end);
    blocks.erase(std::remove_if(blocks.begin(), blocks.end(), [](const TimeBlock& block) {
        return block.source == BlockSource::Auto;
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
    remove.prepare(QStringLiteral("DELETE FROM time_blocks WHERE source = ? AND end_time > ? AND start_time < ?"));
    remove.addBindValue(static_cast<int>(BlockSource::Auto));
    remove.addBindValue(toIso(window.start));
    remove.addBindValue(toIso(window.end));
    if (!remove.exec()) {
        db.rollback();
        return false;
    }

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(R"SQL(
        INSERT INTO time_blocks(task_id, start_time, end_time, source, schedule_run_id, created_at)
        VALUES(?, ?, ?, ?, ?, ?)
    )SQL"));
    for (const auto& block : blocks) {
        insert.addBindValue(block.taskId);
        insert.addBindValue(toIso(block.start));
        insert.addBindValue(toIso(block.end));
        insert.addBindValue(static_cast<int>(block.source));
        insert.addBindValue(scheduleRunId);
        insert.addBindValue(toIso(QDateTime::currentDateTime()));
        if (!insert.exec()) {
            db.rollback();
            return false;
        }
    }

    return db.commit();
}

bool TimeBlockRepository::moveBlock(int blockId, const QDateTime& start, const QDateTime& end) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE time_blocks SET start_time = ?, end_time = ?, source = ? WHERE id = ?"));
    query.addBindValue(toIso(start));
    query.addBindValue(toIso(end));
    query.addBindValue(static_cast<int>(BlockSource::UserDragged));
    query.addBindValue(blockId);
    return query.exec() && query.numRowsAffected() == 1;
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
