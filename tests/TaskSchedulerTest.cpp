#include "core/scheduler/TaskScheduler.h"

#include <QtTest/QtTest>

namespace {
Task makeTask(int id, const QDateTime& deadline, int remainingMinutes)
{
    Task task;
    task.id = id;
    task.title = QStringLiteral("Task %1").arg(id);
    task.startDate = QDateTime(deadline.date(), QTime(0, 0));
    task.deadline = deadline;
    task.estimatedMinutes = remainingMinutes;
    task.remainingMinutes = remainingMinutes;
    task.minChunkMinutes = 30;
    task.idealChunkMinutes = 60;
    task.priority = Priority::Medium;
    task.autoScheduleEnabled = true;
    return task;
}

bool overlaps(const QDateTime& aStart, const QDateTime& aEnd, const QDateTime& bStart, const QDateTime& bEnd)
{
    return aStart < bEnd && bStart < aEnd;
}
}

class TaskSchedulerTest : public QObject {
    Q_OBJECT

private slots:
    void doesNotCoverFixedEvents();
    void doesNotScheduleAfterDeadline();
    void keepsLockedBlocksUnavailable();
    void dynamicPriorityPrefersUrgentHardTask();
};

void TaskSchedulerTest::doesNotCoverFixedEvents()
{
    const QDate day(2026, 1, 5);
    const ScheduleWindow window{QDateTime(day, QTime(0, 0)), QDateTime(day, QTime(23, 59, 59))};
    SchedulingConfig config;
    config.workdayStartHour = 8;
    config.workdayEndHour = 18;
    config.breakMinutes = 0;

    CalendarEvent course;
    course.id = 1;
    course.title = QStringLiteral("Course");
    course.start = QDateTime(day, QTime(8, 0));
    course.end = QDateTime(day, QTime(10, 0));

    TaskScheduler scheduler;
    const auto result = scheduler.generateSchedule(
        {makeTask(1, QDateTime(day, QTime(18, 0)), 60)},
        {course},
        {},
        {},
        window,
        config);

    QVERIFY(!result.generatedBlocks.isEmpty());
    for (const auto& block : result.generatedBlocks) {
        QVERIFY(!overlaps(block.start, block.end, course.start, course.end));
    }
}

void TaskSchedulerTest::doesNotScheduleAfterDeadline()
{
    const QDate day(2026, 1, 5);
    const QDateTime deadline(day, QTime(9, 30));
    const ScheduleWindow window{QDateTime(day, QTime(0, 0)), QDateTime(day, QTime(23, 59, 59))};
    SchedulingConfig config;
    config.workdayStartHour = 8;
    config.workdayEndHour = 18;
    config.breakMinutes = 0;

    TaskScheduler scheduler;
    const auto result = scheduler.generateSchedule(
        {makeTask(1, deadline, 150)},
        {},
        {},
        {},
        window,
        config);

    QVERIFY(!result.generatedBlocks.isEmpty());
    QVERIFY(result.unscheduledTaskIds.contains(1));
    for (const auto& block : result.generatedBlocks) {
        QVERIFY(block.end <= deadline);
    }
}

void TaskSchedulerTest::keepsLockedBlocksUnavailable()
{
    const QDate day(2026, 1, 5);
    const ScheduleWindow window{QDateTime(day, QTime(0, 0)), QDateTime(day, QTime(23, 59, 59))};
    SchedulingConfig config;
    config.workdayStartHour = 8;
    config.workdayEndHour = 12;
    config.breakMinutes = 0;

    TimeBlock locked;
    locked.id = 10;
    locked.taskId = 99;
    locked.start = QDateTime(day, QTime(9, 0));
    locked.end = QDateTime(day, QTime(10, 0));
    locked.source = BlockSource::Locked;

    TaskScheduler scheduler;
    const auto result = scheduler.generateSchedule(
        {makeTask(1, QDateTime(day, QTime(12, 0)), 60)},
        {},
        {locked},
        {},
        window,
        config);

    QVERIFY(!result.generatedBlocks.isEmpty());
    for (const auto& block : result.generatedBlocks) {
        QVERIFY(!overlaps(block.start, block.end, locked.start, locked.end));
    }
}


void TaskSchedulerTest::dynamicPriorityPrefersUrgentHardTask()
{
    const QDateTime now(QDate(2026, 6, 24), QTime(12, 0));
    Task normal = makeTask(1, now.addDays(7), 60);
    normal.priority = Priority::High;
    Task urgent = makeTask(2, now.addSecs(4 * 60 * 60), 240);
    urgent.priority = Priority::Medium;
    urgent.deadlineType = DeadlineType::Hard;
    urgent.scheduleStatus = ScheduleStatus::CouldNotFit;

    PriorityEvaluator evaluator;
    QVERIFY(evaluator.score(urgent, now) > evaluator.score(normal, now));
    QCOMPARE(evaluator.details(urgent, now).value(QStringLiteral("level")).toString(), QStringLiteral("critical"));
}

QTEST_MAIN(TaskSchedulerTest)
#include "TaskSchedulerTest.moc"
