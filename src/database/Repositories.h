#pragma once

#include "core/models/CalendarEvent.h"
#include "core/models/StudyFrame.h"
#include "core/models/Task.h"
#include "core/models/TimeBlock.h"

#include <QSqlDatabase>

class SettingsRepository {
public:
    explicit SettingsRepository(QSqlDatabase db);
    QString value(const QString& key, const QString& fallback = {}) const;
    bool setValue(const QString& key, const QString& value) const;

private:
    QSqlDatabase m_db;
};

class TaskRepository {
public:
    explicit TaskRepository(QSqlDatabase db);
    QVector<Task> activeTasks() const;
    QVector<Task> allTasks() const;
    bool createTask(const Task& task, const QString& categoryName) const;
    bool updateTask(const Task& task, const QString& categoryName) const;
    bool updateScheduleStatuses(const QVector<int>& scheduledTaskIds, const QVector<int>& couldNotFitTaskIds) const;
    bool completeTask(int taskId) const;
    bool deleteTask(int taskId) const;

private:
    int ensureCategory(const QString& name) const;
    QSqlDatabase m_db;
};

class CalendarRepository {
public:
    explicit CalendarRepository(QSqlDatabase db);
    QVector<CalendarEvent> eventsBetween(const QDateTime& start, const QDateTime& end) const;
    bool createEvent(const CalendarEvent& event, const QString& categoryName) const;
    bool updateEvent(const CalendarEvent& event, const QString& categoryName) const;
    bool setEventLocked(int eventId, bool locked) const;
    bool moveEvent(int eventId, const QDateTime& start, const QDateTime& end) const;

private:
    int ensureCategory(const QString& name) const;
    QSqlDatabase m_db;
};

class TimeBlockRepository {
public:
    explicit TimeBlockRepository(QSqlDatabase db);
    QVector<TimeBlock> blocksBetween(const QDateTime& start, const QDateTime& end) const;
    QVector<TimeBlock> lockedBlocksBetween(const QDateTime& start, const QDateTime& end) const;
    int createScheduleRun(const ScheduleWindow& window, const QString& reason) const;
    bool replaceAutoBlocks(const ScheduleWindow& window, const QVector<TimeBlock>& blocks, int scheduleRunId) const;
    int createBlock(const TimeBlock& block) const;
    bool moveBlock(int blockId, const QDateTime& start, const QDateTime& end) const;
    bool setBlockSource(int blockId, BlockSource source) const;
    bool completeBlock(int blockId) const;

private:
    QSqlDatabase m_db;
};

class StudyFrameRepository {
public:
    explicit StudyFrameRepository(QSqlDatabase db);
    QVector<StudyFrame> allFrames() const;
    QVector<StudyFrame> enabledFrames() const;
    bool createFrame(const StudyFrame& frame, const QString& categoryName) const;
    bool setEnabled(int frameId, bool enabled) const;
    bool deleteFrame(int frameId) const;

private:
    int ensureCategory(const QString& name) const;
    QSqlDatabase m_db;
};
