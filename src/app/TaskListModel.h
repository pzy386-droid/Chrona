#pragma once

#include "core/models/Task.h"

#include <QAbstractListModel>

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
        SelectedRole
    };

    explicit TaskListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setTasks(const QVector<Task>& tasks);
    void setSelectedTaskId(int taskId);
    int selectedTaskId() const;
    std::optional<Task> taskById(int taskId) const;

private:
    QVector<Task> m_tasks;
    int m_selectedTaskId = 0;
};
