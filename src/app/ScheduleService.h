#pragma once

#include "app/TaskListModel.h"
#include "app/TimelineModel.h"
#include "ai/AIService.h"
#include "core/scheduler/ConflictResolver.h"
#include "core/scheduler/TaskScheduler.h"
#include "database/Repositories.h"
#include "database/BackupService.h"
#include "managers/NLPTaskParser.h"
#include "managers/OCRManager.h"

#include <QObject>
#include <QVariantMap>
#include <QDate>
#include <QHash>
#include <QVariantList>
#include <QTimer>
class ScheduleService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* timelineModel READ timelineModel CONSTANT)
    Q_PROPERTY(QObject* taskModel READ taskModel CONSTANT)
    Q_PROPERTY(QVariantMap focusItem READ focusItem NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap capacityStats READ capacityStats NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap aiStatus READ aiStatus NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap selectedTask READ selectedTask NOTIFY selectedTaskChanged)
    Q_PROPERTY(QVariantMap selectedDetail READ selectedDetail NOTIFY selectedTaskChanged)
    Q_PROPERTY(int unscheduledCount READ unscheduledCount NOTIFY dataChanged)
    Q_PROPERTY(int urgentDeadlineCount READ urgentDeadlineCount NOTIFY dataChanged)
    Q_PROPERTY(QVariantList deadlineReminders READ deadlineReminders NOTIFY dataChanged)
    Q_PROPERTY(QVariantList scheduleIssues READ scheduleIssues NOTIFY dataChanged)
    Q_PROPERTY(QVariantList dailyItems READ dailyItems NOTIFY selectedTaskChanged)
    Q_PROPERTY(bool demoModeEnabled READ demoModeEnabled NOTIFY dataChanged)
    Q_PROPERTY(bool firstLoginCompleted READ firstLoginCompleted NOTIFY loginStateChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY loginStateChanged)
    Q_PROPERTY(QString persistenceError READ persistenceError NOTIFY dataChanged)
    Q_PROPERTY(QString selectedDateText READ selectedDateText NOTIFY selectedDateChanged)

public:
    ScheduleService(TaskRepository tasks, CalendarRepository calendar, DeadlineReminderRepository deadlines, TimeBlockRepository blocks,
                    StudyFrameRepository studyFrames, SettingsRepository settings, BackupService backup,
                    AIService* aiService = nullptr, QObject* parent = nullptr);

    QObject* timelineModel();
    QObject* taskModel();
    QVariantMap focusItem() const;
    QVariantMap capacityStats() const;
    QVariantMap aiStatus() const;
    QVariantMap selectedTask() const;
    QVariantMap selectedDetail() const;
    int unscheduledCount() const;
    int urgentDeadlineCount() const;
    QVariantList deadlineReminders() const;
    QVariantList scheduleIssues() const;
    QVariantList dailyItems() const;
    bool demoModeEnabled() const;
    bool firstLoginCompleted() const;
    QString userName() const;
    QString persistenceError() const;
    QString selectedDateText() const;

    Q_INVOKABLE QVariantMap reschedule();
    Q_INVOKABLE void selectTask(int taskId);
    Q_INVOKABLE void selectTimelineItem(int taskId, int blockId);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE QVariantMap startFocus();
    Q_INVOKABLE QVariantMap stopFocus();
    Q_INVOKABLE void completeTask(int taskId);
    Q_INVOKABLE QVariantMap completeBlock(int blockId);
    Q_INVOKABLE QVariantMap reopenBlock(int blockId);
    Q_INVOKABLE QVariantMap updateTask(int taskId, const QString& title, const QString& notes, const QString& deadlineText, int estimatedMinutes, int priority,
                                       const QString& categoryName, const QString& categoryColor, const QString& preferredStudyTime, bool autoScheduleEnabled, int deadlineType,
                                       int minChunkMinutes, int idealChunkMinutes, int effortLevel);
    Q_INVOKABLE QVariantMap archiveTask(int taskId);
    Q_INVOKABLE QVariantMap deleteTask(int taskId);
    Q_INVOKABLE QVariantMap setSelectedDate(const QString& dateText);
    Q_INVOKABLE QVariantMap goToToday();
    Q_INVOKABLE QVariantList monthOverview(int year, int month) const;
    Q_INVOKABLE QVariantMap updateCategoryColor(const QString& categoryName, const QString& color);
    Q_INVOKABLE QVariantMap createScheduledTask(const QString& title, int dayOffset, int startMinute, int durationMinutes, bool locked, const QString& categoryName, const QString& categoryColor);
    Q_INVOKABLE QVariantMap addDeadlineReminder(const QString& title, const QString& dueText, const QString& notes, const QString& categoryName, int remindDaysBefore);
    Q_INVOKABLE QVariantMap updateDeadlineReminder(int reminderId, const QString& title, const QString& dueText, const QString& notes, const QString& categoryName, int remindDaysBefore);
    Q_INVOKABLE QVariantMap completeDeadlineReminder(int reminderId);
    Q_INVOKABLE QVariantMap archiveDeadlineReminder(int reminderId);
    Q_INVOKABLE QVariantMap deleteDeadlineReminder(int reminderId);
    Q_INVOKABLE bool setCategoryColor(const QString& categoryName, const QString& color);
    Q_INVOKABLE bool moveBlock(int blockId, int dayIndex, int startMinute, int durationMinutes);
    Q_INVOKABLE QVariantMap moveTimelineItem(int itemId, int dayIndex, int startMinute, int durationMinutes);
    Q_INVOKABLE QVariantMap resizeTimelineItemTime(int itemId, int startMinute, int durationMinutes);
    Q_INVOKABLE QVariantMap stretchTimelineItemDays(int itemId, int endDayIndex);
    Q_INVOKABLE QVariantMap resizeTimelineItemDays(int itemId, int startDayIndex, int endDayIndex);
    Q_INVOKABLE QVariantMap moveSelectedTaskBlock(int dayIndex, const QString& startText, const QString& endText);
    Q_INVOKABLE QVariantMap scheduleSelectedTaskBlocks(int startDayIndex, int endDayIndex, const QString& startText, const QString& endText, bool locked);
    Q_INVOKABLE QVariantMap setSelectedBlockLocked(bool locked);
    Q_INVOKABLE QVariantMap previewTaskDraft(const QString& input);
    Q_INVOKABLE void requestTaskDraft(const QString& input);
    Q_INVOKABLE QVariantMap createTaskFromDraft(const QVariantMap& draft);
    Q_INVOKABLE QVariantMap createTasksFromDrafts(const QVariantList& drafts);
    Q_INVOKABLE QVariantMap setDeepSeekApiKey(const QString& apiKey);
    Q_INVOKABLE QVariantMap quickAdd(const QString& input);
    Q_INVOKABLE QVariantMap aiTodayPlan();
    Q_INVOKABLE QVariantMap previewScheduleSuggestions();
    Q_INVOKABLE void requestSchedulePlan(const QString& mode);
    Q_INVOKABLE QVariantMap explainSelectedSchedule();
    Q_INVOKABLE QVariantMap previewSelectedFocusBuffer();
    Q_INVOKABLE QVariantMap applyFocusBufferSuggestion(const QVariantMap& suggestion);
    Q_INVOKABLE QVariantMap applyScheduleSuggestion(const QVariantMap& suggestion);
    Q_INVOKABLE QVariantMap previewImageTask(const QString& fileUrlOrPath);
    Q_INVOKABLE QVariantMap eveningReview() const;
    Q_INVOKABLE QVariantMap completeFirstLogin(const QString& name);
    Q_INVOKABLE bool signOutForLogin();
    Q_INVOKABLE bool setDemoModeEnabled(bool enabled);
    Q_INVOKABLE QVariantMap exportData(const QString& filePath);
    Q_INVOKABLE QVariantMap importData(const QString& filePath);

signals:
    void dataChanged();
    void selectedTaskChanged();
    void selectedDateChanged();
    void loginStateChanged();
    void taskDraftReady(const QVariantMap& result);
    void schedulePlanReady(const QString& mode, const QVariantMap& result);

private:
    ScheduleWindow currentWindow() const;
    ScheduleWindow displayWindow() const;
    int normalizeManualBlockBackedTasks(const ScheduleWindow& window, const QVector<TimeBlock>& manualBlocks);
    void reload();
    void rebuildTimeline(const QVector<Task>& tasks, const QVector<CalendarEvent>& events, const QVector<TimeBlock>& blocks);
    void rebuildCapacityStats(const QVector<TimeBlock>& blocks, const QVector<CalendarEvent>& events);
    QVariantMap taskToMap(const Task& task) const;
    QVariantMap deadlineReminderToMap(const DeadlineReminder& reminder) const;
    QVariantMap eventToDetailMap(const TimelineItem& item) const;
    ScheduleContext buildAIContext(const QString& mode) const;
    QVariantMap localAIPlan(const QString& mode) const;
    QVariantMap requestAIPlan(const QString& mode);

    TaskRepository m_tasks;
    CalendarRepository m_calendar;
    DeadlineReminderRepository m_deadlines;
    TimeBlockRepository m_blocks;
    StudyFrameRepository m_studyFrames;
    SettingsRepository m_settings;
    BackupService m_backup;
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
    QVariantList m_dailyItems;
    QVector<int> m_unscheduledTaskIds;
    int m_selectedTaskId = 0;
    int m_selectedBlockId = 0;
    QString m_persistenceError;
    QDate m_selectedDate = QDate::currentDate();
    QDate m_lastObservedDate = QDate::currentDate();
    bool m_followToday = true;
    bool m_relocateHorizontalConflicts = false;
    QTimer m_capacityRefreshTimer;
};
