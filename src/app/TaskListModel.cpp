#include "app/TaskListModel.h"

namespace {
QString priorityText(Priority priority)
{
    switch (priority) {
    case Priority::High:
        return QObject::tr("高");
    case Priority::Medium:
        return QObject::tr("中");
    case Priority::Low:
        return QObject::tr("低");
    }
    return {};
}
}

TaskListModel::TaskListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int TaskListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_tasks.size();
}

QVariant TaskListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tasks.size()) {
        return {};
    }

    const Task& task = m_tasks.at(index.row());
    switch (role) {
    case IdRole:
        return task.id;
    case TitleRole:
        return task.title;
    case NotesRole:
        return task.notes;
    case DeadlineRole:
        return task.deadline;
    case DeadlineTextRole:
        return task.deadline.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    case EstimatedMinutesRole:
        return task.estimatedMinutes;
    case RemainingMinutesRole:
        return task.remainingMinutes;
    case PriorityRole:
        return static_cast<int>(task.priority);
    case PriorityTextRole:
        return priorityText(task.priority);
    case StatusRole:
        return static_cast<int>(task.status);
    case CategoryNameRole:
        return task.categoryName;
    case CategoryColorRole:
        return task.categoryColor.isEmpty() ? QStringLiteral("#7C8CFF") : task.categoryColor;
    case SelectedRole:
        return task.id == m_selectedTaskId;
    default:
        return {};
    }
}

QHash<int, QByteArray> TaskListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {TitleRole, "title"},
        {NotesRole, "notes"},
        {DeadlineRole, "deadline"},
        {DeadlineTextRole, "deadlineText"},
        {EstimatedMinutesRole, "estimatedMinutes"},
        {RemainingMinutesRole, "remainingMinutes"},
        {PriorityRole, "priority"},
        {PriorityTextRole, "priorityText"},
        {StatusRole, "status"},
        {CategoryNameRole, "categoryName"},
        {CategoryColorRole, "categoryColor"},
        {SelectedRole, "selected"}
    };
}

void TaskListModel::setTasks(const QVector<Task>& tasks)
{
    beginResetModel();
    m_tasks = tasks;
    endResetModel();
}

void TaskListModel::setSelectedTaskId(int taskId)
{
    if (m_selectedTaskId == taskId) {
        return;
    }
    m_selectedTaskId = taskId;
    if (!m_tasks.isEmpty()) {
        emit dataChanged(index(0, 0), index(m_tasks.size() - 1, 0), {SelectedRole});
    }
}

int TaskListModel::selectedTaskId() const
{
    return m_selectedTaskId;
}

std::optional<Task> TaskListModel::taskById(int taskId) const
{
    for (const auto& task : m_tasks) {
        if (task.id == taskId) {
            return task;
        }
    }
    return std::nullopt;
}
