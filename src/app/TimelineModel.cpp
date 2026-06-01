#include "app/TimelineModel.h"

TimelineModel::TimelineModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int TimelineModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_items.size();
}

QVariant TimelineModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }
    const TimelineItem& item = m_items.at(index.row());
    switch (role) {
    case IdRole:
        return item.id;
    case TaskIdRole:
        return item.taskId;
    case TitleRole:
        return item.title;
    case SubtitleRole:
        return item.subtitle;
    case StartTimeRole:
        return item.start;
    case EndTimeRole:
        return item.end;
    case TimeRangeRole:
        return item.start.time().toString(QStringLiteral("hh:mm")) + QStringLiteral(" - ") + item.end.time().toString(QStringLiteral("hh:mm"));
    case DayIndexRole:
        return item.dayIndex;
    case StartMinuteRole:
        return item.startMinute;
    case DurationMinutesRole:
        return item.durationMinutes;
    case PriorityRole:
        return item.priority;
    case ColorRole:
        return item.color;
    case IsEventRole:
        return item.event;
    case IsLockedRole:
        return item.locked;
    case IsEventLockedRole:
        return item.eventLocked;
    case SelectedRole:
        return item.id == m_selectedItemId || (m_selectedItemId == 0 && item.taskId != 0 && item.taskId == m_selectedTaskId);
    case BlockOrdinalRole:
        return item.blockOrdinal;
    case BlockTotalRole:
        return item.blockTotal;
    case SpanDaysRole:
        return item.spanDays;
    case HiddenInSpanRole:
        return item.hiddenInSpan;
    default:
        return {};
    }
}

QHash<int, QByteArray> TimelineModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {TaskIdRole, "taskId"},
        {TitleRole, "title"},
        {SubtitleRole, "subtitle"},
        {StartTimeRole, "startTime"},
        {EndTimeRole, "endTime"},
        {TimeRangeRole, "timeRange"},
        {DayIndexRole, "dayIndex"},
        {StartMinuteRole, "startMinute"},
        {DurationMinutesRole, "durationMinutes"},
        {PriorityRole, "priority"},
        {ColorRole, "color"},
        {IsEventRole, "isEvent"},
        {IsLockedRole, "isLocked"},
        {IsEventLockedRole, "isEventLocked"},
        {SelectedRole, "selected"},
        {BlockOrdinalRole, "blockOrdinal"},
        {BlockTotalRole, "blockTotal"},
        {SpanDaysRole, "spanDays"},
        {HiddenInSpanRole, "hiddenInSpan"}
    };
}

void TimelineModel::setItems(const QVector<TimelineItem>& items)
{
    if (m_items.size() == items.size()) {
        bool sameItems = true;
        for (int i = 0; i < items.size(); ++i) {
            if (m_items.at(i).id != items.at(i).id) {
                sameItems = false;
                break;
            }
        }
        if (sameItems) {
            m_items = items;
            if (!m_items.isEmpty()) {
                emit dataChanged(index(0, 0), index(m_items.size() - 1, 0));
            }
            return;
        }
    }

    beginResetModel();
    m_items = items;
    endResetModel();
}

void TimelineModel::setSelectedTaskId(int taskId)
{
    if (m_selectedTaskId == taskId) {
        return;
    }
    m_selectedTaskId = taskId;
    m_selectedItemId = 0;
    if (!m_items.isEmpty()) {
        emit dataChanged(index(0, 0), index(m_items.size() - 1, 0), {SelectedRole});
    }
}

void TimelineModel::setSelectedItemId(int itemId)
{
    if (m_selectedItemId == itemId) {
        return;
    }
    m_selectedItemId = itemId;
    if (!m_items.isEmpty()) {
        emit dataChanged(index(0, 0), index(m_items.size() - 1, 0), {SelectedRole});
    }
}

std::optional<TimelineItem> TimelineModel::firstBlockForTask(int taskId) const
{
    for (const auto& item : m_items) {
        if (!item.event && item.taskId == taskId) {
            return item;
        }
    }
    return std::nullopt;
}

std::optional<TimelineItem> TimelineModel::itemById(int itemId) const
{
    for (const auto& item : m_items) {
        if (item.id == itemId) {
            return item;
        }
    }
    return std::nullopt;
}

const QVector<TimelineItem>& TimelineModel::items() const
{
    return m_items;
}
