#pragma once

#include "core/models/Task.h"

#include <QAbstractListModel>
#include <QDate>
#include <QHash>
#include <QSet>

class TaskListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        NotesRole,
        DeadlineRole,
        DeadlineTextRole,
        EstimatedMinutesRole,
        RemainingMinutesRole,
        PriorityRole,
        PriorityTextRole,
        StatusRole,
        CategoryNameRole,
        CategoryColorRole,
        ScheduleTextRole,
        SelectedRole
    };

    explicit TaskListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setTasks(const QVector<Task>& tasks);
    void setDailyFilter(const QDate& date, const QSet<int>& scheduledTaskIds);
    void setScheduleTextByTaskId(const QHash<int, QString>& scheduleTextByTaskId);
    void setSelectedTaskId(int taskId);
    int selectedTaskId() const;
    std::optional<Task> taskById(int taskId) const;

private:
    void rebuildVisibleTasks();

    QVector<Task> m_allTasks;
    QVector<Task> m_tasks;
    QDate m_filterDate;
    QSet<int> m_scheduledTaskIds;
    QHash<int, QString> m_scheduleTextByTaskId;
    int m_selectedTaskId = 0;
};
