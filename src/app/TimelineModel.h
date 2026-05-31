#pragma once

#include <QAbstractListModel>
#include <QDateTime>

struct TimelineItem {
    int id = 0;
    int taskId = 0;
    QString title;
    QString subtitle;
    QDateTime start;
    QDateTime end;
    int dayIndex = 0;
    int startMinute = 0;
    int durationMinutes = 0;
    int priority = 1;
    QString color;
    bool event = false;
    bool locked = false;
    bool eventLocked = false;
    bool selected = false;
    int blockOrdinal = 0;
    int blockTotal = 0;
};

class TimelineModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TaskIdRole,
        TitleRole,
        SubtitleRole,
        StartTimeRole,
        EndTimeRole,
        TimeRangeRole,
        DayIndexRole,
        StartMinuteRole,
        DurationMinutesRole,
        PriorityRole,
        ColorRole,
        IsEventRole,
        IsLockedRole,
        IsEventLockedRole,
        SelectedRole,
        BlockOrdinalRole,
        BlockTotalRole
    };

    explicit TimelineModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(const QVector<TimelineItem>& items);
    void setSelectedTaskId(int taskId);
    void setSelectedItemId(int itemId);
    std::optional<TimelineItem> firstBlockForTask(int taskId) const;
    std::optional<TimelineItem> itemById(int itemId) const;
    const QVector<TimelineItem>& items() const;

private:
    QVector<TimelineItem> m_items;
    int m_selectedTaskId = 0;
    int m_selectedItemId = 0;
};
