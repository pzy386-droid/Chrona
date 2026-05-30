#include "TimeBlock.h"

QJsonObject TimeBlock::toJson() const
{
    return {
        {QStringLiteral("blockId"), id},
        {QStringLiteral("relatedTaskId"), taskId},
        {QStringLiteral("startTime"), start.toString(Qt::ISODate)},
        {QStringLiteral("endTime"), end.toString(Qt::ISODate)},
        {QStringLiteral("source"), static_cast<int>(source)},
        {QStringLiteral("isLocked"), source == BlockSource::Locked || source == BlockSource::UserDragged},
        {QStringLiteral("scheduleRunId"), scheduleRunId}
    };
}

TimeBlock TimeBlock::fromJson(const QJsonObject& object)
{
    TimeBlock block;
    block.id = object.value(QStringLiteral("blockId")).toInt();
    block.taskId = object.value(QStringLiteral("relatedTaskId")).toInt();
    block.start = QDateTime::fromString(object.value(QStringLiteral("startTime")).toString(), Qt::ISODate);
    block.end = QDateTime::fromString(object.value(QStringLiteral("endTime")).toString(), Qt::ISODate);
    block.source = object.value(QStringLiteral("isLocked")).toBool()
        ? BlockSource::Locked
        : static_cast<BlockSource>(object.value(QStringLiteral("source")).toInt());
    block.scheduleRunId = object.value(QStringLiteral("scheduleRunId")).toInt();
    return block;
}
