#pragma once

#include "app/TaskListModel.h"
#include "app/TimelineModel.h"
#include "ai/AIService.h"
#include "core/scheduler/ConflictResolver.h"
#include "core/scheduler/TaskScheduler.h"
#include "database/Repositories.h"
#include "managers/NLPTaskParser.h"
#include "managers/OCRManager.h"

#include <QObject>
#include <QVariantMap>
#include <QDate>
#include <QVariantList>
class ScheduleService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* timelineModel READ timelineModel CONSTANT)
    Q_PROPERTY(QObject* taskModel READ taskModel CONSTANT)
    Q_PROPERTY(QVariantMap focusItem READ focusItem NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap capacityStats READ capacityStats NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap selectedTask READ selectedTask NOTIFY selectedTaskChanged)
    Q_PROPERTY(QVariantList studyFrames READ studyFrames NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap selectedDetail READ selectedDetail NOTIFY selectedTaskChanged)
    Q_PROPERTY(int unscheduledCount READ unscheduledCount NOTIFY dataChanged)
    Q_PROPERTY(QVariantList scheduleIssues READ scheduleIssues NOTIFY dataChanged)

public:
    ScheduleService(TaskRepository tasks, CalendarRepository calendar, TimeBlockRepository blocks, StudyFrameRepository studyFrames, AIService* aiService = nullptr, QObject* parent = nullptr);

    QObject* timelineModel();
    QObject* taskModel();
    QVariantMap focusItem() const;
    QVariantMap capacityStats() const;
    QVariantMap selectedTask() const;
    QVariantList studyFrames() const;
    QVariantMap selectedDetail() const;
    int unscheduledCount() const;
    QVariantList scheduleIssues() const;

    Q_INVOKABLE void reschedule();
    Q_INVOKABLE void selectTask(int taskId);
    Q_INVOKABLE void selectTimelineItem(int taskId, int blockId);
    Q_INVOKABLE QVariantMap startFocus();
    Q_INVOKABLE QVariantMap stopFocus();
    Q_INVOKABLE void completeTask(int taskId);
    Q_INVOKABLE QVariantMap updateTask(int taskId, const QString& title, const QString& notes, const QString& deadlineText, int estimatedMinutes, int priority,
                                       const QString& categoryName, const QString& preferredStudyTime, bool autoScheduleEnabled, int deadlineType,
                                       int minChunkMinutes, int idealChunkMinutes, int effortLevel);
    Q_INVOKABLE QVariantMap deleteTask(int taskId);
    Q_INVOKABLE QVariantMap addFixedEvent(const QString& title, int dayOffset, int startMinute, int durationMinutes, bool locked, const QString& categoryName);
    Q_INVOKABLE QVariantMap addStudyFrame(const QString& name, int dayIndex, const QString& startText, const QString& endText, const QString& categoryName, const QString& energyLevel);
    Q_INVOKABLE bool setStudyFrameEnabled(int frameId, bool enabled);
    Q_INVOKABLE bool deleteStudyFrame(int frameId);
    Q_INVOKABLE QVariantMap updateSelectedEvent(const QString& title, int dayIndex, const QString& startText, const QString& endText, bool locked, const QString& categoryName);
    Q_INVOKABLE bool moveBlock(int blockId, int dayIndex, int startMinute, int durationMinutes);
    Q_INVOKABLE bool moveTimelineItem(int itemId, bool isEvent, int dayIndex, int startMinute, int durationMinutes);
    Q_INVOKABLE QVariantMap moveTimelineItemWithResult(int itemId, bool isEvent, int dayIndex, int startMinute, int durationMinutes);
    Q_INVOKABLE QVariantMap createSelectedTaskBlock(int dayIndex, const QString& startText, const QString& endText);
    Q_INVOKABLE QVariantMap moveSelectedTaskBlock(int dayIndex, const QString& startText, const QString& endText);
    Q_INVOKABLE bool setEventLocked(int eventId, bool locked);
    Q_INVOKABLE QVariantMap setSelectedBlockLocked(bool locked);
    Q_INVOKABLE QVariantMap previewTaskDraft(const QString& input);
    Q_INVOKABLE QVariantMap createTaskFromDraft(const QVariantMap& draft);
    Q_INVOKABLE QVariantMap quickAdd(const QString& input);
    Q_INVOKABLE QVariantMap previewImageTask(const QString& fileUrlOrPath);

signals:
    void dataChanged();
    void selectedTaskChanged();

private:
    ScheduleWindow currentWindow() const;
    void reload();
    void rebuildTimeline(const QVector<Task>& tasks, const QVector<CalendarEvent>& events, const QVector<TimeBlock>& blocks);
    void rebuildCapacityStats(const QVector<TimeBlock>& blocks);
    QVariantMap taskToMap(const Task& task) const;
    QVariantMap studyFrameToMap(const StudyFrame& frame) const;
    QVariantMap eventToDetailMap(const TimelineItem& item) const;

    TaskRepository m_tasks;
    CalendarRepository m_calendar;
    TimeBlockRepository m_blocks;
    StudyFrameRepository m_studyFrames;
    TaskScheduler m_scheduler;
    ConflictResolver m_conflicts;
    NLPTaskParser m_parser;
    OCRManager m_ocr;
    AIService* m_aiService = nullptr;
    SchedulingConfig m_config;
    TaskListModel m_taskModel;
    TimelineModel m_timelineModel;
    QVariantMap m_focusItem;
    QVariantMap m_capacityStats;
    QVariantList m_scheduleIssues;
    QVector<int> m_unscheduledTaskIds;
    int m_selectedTaskId = 0;
    int m_selectedBlockId = 0;
};
