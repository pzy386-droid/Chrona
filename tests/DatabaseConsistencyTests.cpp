#include "database/BackupService.h"
#include "database/Migrations.h"
#include "database/Repositories.h"
#include "core/scheduler/CascadePlanner.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include <algorithm>

class DatabaseConsistencyTests : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void freshDatabaseUsesCurrentSchema();
    void demoSeedCreatesCompleteDataset();
    void legacyCalendarEventsMigrateToUnifiedBlocks();
    void legacyDatabaseMigratesWithoutDataLoss();
    void taskCategoryAndColorPersist();
    void deadlineReminderStaysOutOfTaskScheduling();
    void taskColorOverrideDoesNotMutateCategory();
    void batchBlockFailureRollsBack();
    void deletingTaskCascadesToBlocks();
    void archivingTaskPreservesBlocksAndRemovesItFromActiveTasks();
    void eventLockPersists();
    void scheduleFailureKeepsPreviousState();
    void backupRoundTripPreservesCoreData();
    void cascadeResizePushesOverlappingBlocks();
    void cascadeResizePreservesBlockDurations();
    void cascadeResizeSkipsFixedIntervals();
    void cascadeResizeRejectsDayOverflow();
    void cascadeResizeUpPushesOverlappingBlocks();
    void cascadeResizeUpSkipsFixedIntervals();
    void cascadeResizeUpRejectsDayUnderflow();
    void horizontalRelocationPrefersUpForLeftExtension();
    void horizontalRelocationPrefersDownForRightExtension();
    void horizontalRelocationAvoidsFixedAndOtherBlocks();
    void horizontalRelocationRejectsFullDay();

private:
    Task createTask(const QString& title = QStringLiteral("Test task"));
    int scalar(const QString& sql) const;

    QString m_connectionName;
    QSqlDatabase m_db;
    std::unique_ptr<QTemporaryDir> m_tempDir;
};

void DatabaseConsistencyTests::init()
{
    m_tempDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_tempDir->isValid());
    m_connectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(m_tempDir->filePath(QStringLiteral("test.sqlite")));
    QVERIFY2(m_db.open(), qPrintable(m_db.lastError().text()));
    QVERIFY2(Migrations::run(m_db), qPrintable(Migrations::lastError()));
}

void DatabaseConsistencyTests::cleanup()
{
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
    m_tempDir.reset();
}

Task DatabaseConsistencyTests::createTask(const QString& title)
{
    Task task;
    task.title = title;
    task.startDate = QDateTime(QDate::currentDate(), QTime(0, 0));
    task.deadline = QDateTime(QDate::currentDate().addDays(2), QTime(18, 0));
    task.estimatedMinutes = 60;
    task.remainingMinutes = 60;
    return task;
}

int DatabaseConsistencyTests::scalar(const QString& sql) const
{
    QSqlQuery query(m_db);
    if (!query.exec(sql) || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

void DatabaseConsistencyTests::freshDatabaseUsesCurrentSchema()
{
    QCOMPARE(scalar(QStringLiteral("PRAGMA user_version")), Migrations::currentVersion());
    QCOMPARE(scalar(QStringLiteral("PRAGMA foreign_keys")), 1);
    QCOMPARE(scalar(QStringLiteral(
                 "SELECT COUNT(*) FROM pragma_table_info('time_blocks') WHERE name = 'updated_at'")),
             1);
    QCOMPARE(scalar(QStringLiteral(
                 "SELECT COUNT(*) FROM pragma_foreign_key_list('time_blocks') WHERE [table] = 'tasks'")),
             1);
}

void DatabaseConsistencyTests::demoSeedCreatesCompleteDataset()
{
    QVERIFY(Migrations::seed(m_db));
    QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM tasks")), 6);
    QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM calendar_events")), 0);
    QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM time_blocks")), 3);
    QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM study_frames")), 4);
}

void DatabaseConsistencyTests::legacyCalendarEventsMigrateToUnifiedBlocks()
{
    QSqlQuery downgrade(m_db);
    QVERIFY(downgrade.exec(QStringLiteral("PRAGMA user_version = 4")));
    QVERIFY(downgrade.exec(QStringLiteral(
        "INSERT INTO categories(id, name, color) VALUES(99, 'Course', '#123456')")));
    QVERIFY(downgrade.exec(QStringLiteral(
        "INSERT INTO calendar_events(title, start_time, end_time, type, category_id, is_locked, "
        "created_at, updated_at, color_override) VALUES("
        "'Calculus', '2026-06-21T09:00:00', '2026-06-21T10:30:00', 0, 99, 1, "
        "'2026-06-20T08:00:00', '2026-06-20T08:00:00', '#ABCDEF')")));

    QVERIFY2(Migrations::run(m_db), qPrintable(Migrations::lastError()));
    QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM calendar_events")), 0);
    QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM tasks WHERE title = 'Calculus'")), 1);
    QCOMPARE(scalar(QStringLiteral(
                 "SELECT COUNT(*) FROM time_blocks b JOIN tasks t ON t.id = b.task_id "
                 "WHERE t.title = 'Calculus' AND b.source = 2")),
             1);
}

void DatabaseConsistencyTests::legacyDatabaseMigratesWithoutDataLoss()
{
    const QString legacyName = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase legacy = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), legacyName);
    legacy.setDatabaseName(m_tempDir->filePath(QStringLiteral("legacy.sqlite")));
    QVERIFY(legacy.open());
    QSqlQuery query(legacy);
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE categories(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, color TEXT NOT NULL)")));
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE tasks(id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT NOT NULL, notes TEXT, "
        "deadline TEXT NOT NULL, estimated_minutes INTEGER NOT NULL, remaining_minutes INTEGER NOT NULL, "
        "priority INTEGER NOT NULL, status INTEGER NOT NULL, category_id INTEGER, "
        "created_at TEXT NOT NULL, updated_at TEXT NOT NULL)")));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO categories(id, name, color) VALUES(1, 'Legacy', '#123456')")));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO tasks(id, title, deadline, estimated_minutes, remaining_minutes, priority, status, "
        "category_id, created_at, updated_at) "
        "VALUES(1, 'Keep me', '2026-06-10T18:00:00', 60, 60, 1, 0, 1, "
        "'2026-06-01T00:00:00', '2026-06-01T00:00:00')")));

    QVERIFY2(Migrations::run(legacy), qPrintable(Migrations::lastError()));
    QVERIFY(query.exec(QStringLiteral("SELECT title FROM tasks WHERE id = 1")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("Keep me"));
    QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), Migrations::currentVersion());
    legacy.close();
    legacy = QSqlDatabase();
    QSqlDatabase::removeDatabase(legacyName);
}

void DatabaseConsistencyTests::taskCategoryAndColorPersist()
{
    TaskRepository tasks(m_db);
    Task task = createTask();
    QVERIFY2(tasks.createTask(task, QStringLiteral("Course")), qPrintable(tasks.lastError()));
    QVector<Task> stored = tasks.allTasks();
    QCOMPARE(stored.size(), 1);
    stored[0].title = QStringLiteral("Renamed task");
    QVERIFY2(tasks.updateTask(stored[0], QStringLiteral("Renamed category")), qPrintable(tasks.lastError()));
    QVERIFY2(tasks.updateCategoryColor(QStringLiteral("Renamed category"), QStringLiteral("#ABCDEF")),
             qPrintable(tasks.lastError()));

    TaskRepository reopened(m_db);
    const QVector<Task> reloaded = reopened.allTasks();
    QCOMPARE(reloaded.size(), 1);
    QCOMPARE(reloaded[0].title, QStringLiteral("Renamed task"));
    QCOMPARE(reloaded[0].categoryName, QStringLiteral("Renamed category"));
    QCOMPARE(reloaded[0].categoryColor, QStringLiteral("#ABCDEF"));
}

void DatabaseConsistencyTests::deadlineReminderStaysOutOfTaskScheduling()
{
    DeadlineReminderRepository deadlines(m_db);
    DeadlineReminder reminder;
    reminder.title = QStringLiteral("MOOC deadline");
    reminder.notes = QStringLiteral("Use spare time");
    reminder.dueAt = QDateTime(QDate::currentDate().addDays(10), QTime(23, 59));
    reminder.categoryName = QStringLiteral("Course");
    reminder.remindDaysBefore = 7;

    const int reminderId = deadlines.createReminder(reminder);
    QVERIFY2(reminderId > 0, qPrintable(deadlines.lastError()));
    QCOMPARE(deadlines.reminders().size(), 1);
    QCOMPARE(deadlines.reminders().first().title, QStringLiteral("MOOC deadline"));

    TaskRepository tasks(m_db);
    QCOMPARE(tasks.activeTasks().size(), 0);

    QVERIFY(deadlines.setStatus(reminderId, DeadlineReminderStatus::Done));
    const QVector<DeadlineReminder> stored = deadlines.reminders();
    QCOMPARE(stored.size(), 1);
    QCOMPARE(static_cast<int>(stored.first().status), static_cast<int>(DeadlineReminderStatus::Done));
}

void DatabaseConsistencyTests::taskColorOverrideDoesNotMutateCategory()
{
    TaskRepository tasks(m_db);
    QVERIFY2(tasks.createTask(createTask(QStringLiteral("First task")), QStringLiteral("Shared")),
             qPrintable(tasks.lastError()));
    QVERIFY2(tasks.createTask(createTask(QStringLiteral("Second task")), QStringLiteral("Shared")),
             qPrintable(tasks.lastError()));
    QVERIFY2(tasks.updateCategoryColor(QStringLiteral("Shared"), QStringLiteral("#123456")),
             qPrintable(tasks.lastError()));

    QVector<Task> stored = tasks.allTasks();
    QCOMPARE(stored.size(), 2);
    auto firstIt = std::find_if(stored.begin(), stored.end(), [](const Task& task) {
        return task.title == QStringLiteral("First task");
    });
    QVERIFY(firstIt != stored.end());
    firstIt->colorOverride = QStringLiteral("#ABCDEF");
    firstIt->categoryColor = firstIt->colorOverride;
    QVERIFY2(tasks.updateTask(*firstIt, QStringLiteral("Shared")), qPrintable(tasks.lastError()));

    const QVector<Task> reloaded = tasks.allTasks();
    QCOMPARE(reloaded.size(), 2);
    auto reloadedFirst = std::find_if(reloaded.begin(), reloaded.end(), [](const Task& task) {
        return task.title == QStringLiteral("First task");
    });
    auto reloadedSecond = std::find_if(reloaded.begin(), reloaded.end(), [](const Task& task) {
        return task.title == QStringLiteral("Second task");
    });
    QVERIFY(reloadedFirst != reloaded.end());
    QVERIFY(reloadedSecond != reloaded.end());
    QCOMPARE(reloadedFirst->categoryColor, QStringLiteral("#ABCDEF"));
    QCOMPARE(reloadedSecond->categoryColor, QStringLiteral("#123456"));
}

void DatabaseConsistencyTests::batchBlockFailureRollsBack()
{
    TaskRepository tasks(m_db);
    QVERIFY(tasks.createTask(createTask(), QStringLiteral("Study")));
    const int taskId = tasks.allTasks().first().id;

    TimeBlock valid;
    valid.taskId = taskId;
    valid.start = QDateTime(QDate::currentDate(), QTime(9, 0));
    valid.end = valid.start.addSecs(3600);
    valid.source = BlockSource::Locked;
    TimeBlock invalid = valid;
    invalid.taskId = 999999;
    invalid.start = valid.end;
    invalid.end = invalid.start.addSecs(3600);

    TimeBlockRepository blocks(m_db);
    QVERIFY(!blocks.upsertBlocks({valid, invalid}));
    QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM time_blocks")), 0);
}

void DatabaseConsistencyTests::deletingTaskCascadesToBlocks()
{
    TaskRepository tasks(m_db);
    QVERIFY(tasks.createTask(createTask(), QStringLiteral("Study")));
    const int taskId = tasks.allTasks().first().id;
    TimeBlock block;
    block.taskId = taskId;
    block.start = QDateTime(QDate::currentDate(), QTime(9, 0));
    block.end = block.start.addSecs(3600);
    block.source = BlockSource::Locked;
    TimeBlockRepository blocks(m_db);
    QVERIFY(blocks.createBlock(block) > 0);

    QSqlQuery remove(m_db);
    remove.prepare(QStringLiteral("DELETE FROM tasks WHERE id = ?"));
    remove.addBindValue(taskId);
    QVERIFY(remove.exec());
    QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM time_blocks")), 0);
}

void DatabaseConsistencyTests::archivingTaskPreservesBlocksAndRemovesItFromActiveTasks()
{
    TaskRepository tasks(m_db);
    QVERIFY(tasks.createTask(createTask(QStringLiteral("Historical task")), QStringLiteral("Study")));
    const int taskId = tasks.allTasks().first().id;

    TimeBlock block;
    block.taskId = taskId;
    block.start = QDateTime(QDate::currentDate().addDays(-40), QTime(9, 0));
    block.end = block.start.addSecs(3600);
    block.source = BlockSource::Locked;
    TimeBlockRepository blocks(m_db);
    QVERIFY(blocks.createBlock(block) > 0);

    QVERIFY(tasks.archiveTask(taskId));
    QCOMPARE(tasks.activeTasks().size(), 0);
    QCOMPARE(tasks.allTasks().size(), 1);
    QCOMPARE(tasks.allTasks().first().status, TaskStatus::Archived);
    QCOMPARE(blocks.blocksBetween(block.start.addDays(-1), block.end.addDays(1)).size(), 1);
}

void DatabaseConsistencyTests::eventLockPersists()
{
    CalendarRepository calendar(m_db);
    CalendarEvent event;
    event.title = QStringLiteral("Course");
    event.start = QDateTime(QDate::currentDate(), QTime(10, 0));
    event.end = event.start.addSecs(3600);
    event.locked = true;
    QVERIFY(calendar.createEvent(event, QStringLiteral("Course")));
    const int eventId = calendar.eventsBetween(event.start.addDays(-1), event.end.addDays(1)).first().id;
    QVERIFY(calendar.setEventLocked(eventId, false));

    CalendarRepository reopened(m_db);
    const QVector<CalendarEvent> events = reopened.eventsBetween(event.start.addDays(-1), event.end.addDays(1));
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.first().locked, false);
}

void DatabaseConsistencyTests::scheduleFailureKeepsPreviousState()
{
    TaskRepository tasks(m_db);
    QVERIFY(tasks.createTask(createTask(), QStringLiteral("Study")));
    const int taskId = tasks.allTasks().first().id;
    TimeBlockRepository blocks(m_db);
    TimeBlock oldBlock;
    oldBlock.taskId = taskId;
    oldBlock.start = QDateTime(QDate::currentDate(), QTime(9, 0));
    oldBlock.end = oldBlock.start.addSecs(3600);
    oldBlock.source = BlockSource::Auto;
    QVERIFY(blocks.createBlock(oldBlock) > 0);

    TimeBlock generated = oldBlock;
    generated.id = 0;
    generated.start = QDateTime(QDate::currentDate(), QTime(14, 0));
    generated.end = generated.start.addSecs(3600);
    const ScheduleWindow window {
        QDateTime(QDate::currentDate(), QTime(0, 0)),
        QDateTime(QDate::currentDate().addDays(1), QTime(0, 0))
    };
    QVERIFY(!blocks.commitSchedule(window, {generated}, {999999}, {}, QStringLiteral("test")));
    QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM schedule_runs")), 0);
    QSqlQuery query(m_db);
    QVERIFY(query.exec(QStringLiteral("SELECT start_time FROM time_blocks")));
    QVERIFY(query.next());
    QCOMPARE(QDateTime::fromString(query.value(0).toString(), Qt::ISODate), oldBlock.start);
    QVERIFY(!query.next());
}

void DatabaseConsistencyTests::backupRoundTripPreservesCoreData()
{
    TaskRepository tasks(m_db);
    QVERIFY(tasks.createTask(createTask(QStringLiteral("Backup task")), QStringLiteral("Backup")));
    const int taskId = tasks.allTasks().first().id;
    TimeBlock block;
    block.taskId = taskId;
    block.start = QDateTime(QDate::currentDate(), QTime(11, 0));
    block.end = block.start.addSecs(3600);
    block.source = BlockSource::Locked;
    TimeBlockRepository blocks(m_db);
    QVERIFY(blocks.createBlock(block) > 0);

    const QString backupPath = m_tempDir->filePath(QStringLiteral("backup.json"));
    BackupService backup(m_db);
    QVERIFY2(backup.exportToFile(backupPath), qPrintable(backup.lastError()));

    QSqlQuery clear(m_db);
    QVERIFY(clear.exec(QStringLiteral("DELETE FROM tasks")));
    QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM tasks")), 0);
    QVERIFY2(backup.importFromFile(backupPath), qPrintable(backup.lastError()));
    QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM tasks")), 1);
    QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM time_blocks")), 1);
    QCOMPARE(tasks.allTasks().first().title, QStringLiteral("Backup task"));
}

void DatabaseConsistencyTests::cascadeResizePushesOverlappingBlocks()
{
    const QDate date(2026, 6, 21);
    TimeBlock first;
    first.id = 1;
    first.start = QDateTime(date, QTime(10, 0));
    first.end = QDateTime(date, QTime(11, 0));
    TimeBlock second;
    second.id = 2;
    second.start = QDateTime(date, QTime(10, 45));
    second.end = QDateTime(date, QTime(12, 15));

    const CascadePlanResult result = planCascadeDown(
        QDateTime(date, QTime(9, 0)), QDateTime(date, QTime(10, 30)),
        QDateTime(date.addDays(1), QTime(0, 0)), {first, second}, {});

    QVERIFY(result.ok);
    QCOMPARE(result.movedBlocks.size(), 2);
    QCOMPARE(result.movedBlocks.at(0).start.time(), QTime(10, 30));
    QCOMPARE(result.movedBlocks.at(0).end.time(), QTime(11, 30));
    QCOMPARE(result.movedBlocks.at(1).start.time(), QTime(11, 30));
    QCOMPARE(result.movedBlocks.at(1).end.time(), QTime(13, 0));
}

void DatabaseConsistencyTests::cascadeResizePreservesBlockDurations()
{
    const QDate date(2026, 6, 21);
    TimeBlock block;
    block.id = 1;
    block.start = QDateTime(date, QTime(10, 0));
    block.end = QDateTime(date, QTime(12, 45));

    const CascadePlanResult result = planCascadeDown(
        QDateTime(date, QTime(9, 0)), QDateTime(date, QTime(11, 0)),
        QDateTime(date.addDays(1), QTime(0, 0)), {block}, {});

    QVERIFY(result.ok);
    QCOMPARE(result.movedBlocks.size(), 1);
    QCOMPARE(result.movedBlocks.first().start.secsTo(result.movedBlocks.first().end),
             block.start.secsTo(block.end));
}

void DatabaseConsistencyTests::cascadeResizeSkipsFixedIntervals()
{
    const QDate date(2026, 6, 21);
    TimeBlock block;
    block.id = 1;
    block.start = QDateTime(date, QTime(10, 0));
    block.end = QDateTime(date, QTime(11, 0));
    const TimeInterval fixed {
        QDateTime(date, QTime(10, 30)),
        QDateTime(date, QTime(12, 0))
    };

    const CascadePlanResult result = planCascadeDown(
        QDateTime(date, QTime(9, 0)), QDateTime(date, QTime(10, 30)),
        QDateTime(date.addDays(1), QTime(0, 0)), {block}, {fixed});

    QVERIFY(result.ok);
    QCOMPARE(result.movedBlocks.size(), 1);
    QCOMPARE(result.movedBlocks.first().start.time(), QTime(12, 0));
    QCOMPARE(result.movedBlocks.first().end.time(), QTime(13, 0));
}

void DatabaseConsistencyTests::cascadeResizeRejectsDayOverflow()
{
    const QDate date(2026, 6, 21);
    TimeBlock block;
    block.id = 1;
    block.start = QDateTime(date, QTime(22, 30));
    block.end = QDateTime(date, QTime(23, 45));

    const CascadePlanResult result = planCascadeDown(
        QDateTime(date, QTime(21, 0)), QDateTime(date, QTime(23, 30)),
        QDateTime(date.addDays(1), QTime(0, 0)), {block}, {});

    QVERIFY(!result.ok);
    QCOMPARE(result.error, QStringLiteral("day-overflow"));
    QVERIFY(result.movedBlocks.isEmpty());
}

void DatabaseConsistencyTests::cascadeResizeUpPushesOverlappingBlocks()
{
    const QDate date(2026, 6, 21);
    TimeBlock nearest;
    nearest.id = 1;
    nearest.start = QDateTime(date, QTime(8, 30));
    nearest.end = QDateTime(date, QTime(10, 30));
    TimeBlock earlier;
    earlier.id = 2;
    earlier.start = QDateTime(date, QTime(7, 0));
    earlier.end = QDateTime(date, QTime(9, 0));

    const CascadePlanResult result = planCascadeUp(
        QDateTime(date, QTime(9, 30)), QDateTime(date, QTime(13, 0)),
        QDateTime(date, QTime(0, 0)), {earlier, nearest}, {});

    QVERIFY(result.ok);
    QCOMPARE(result.movedBlocks.size(), 2);
    QCOMPARE(result.movedBlocks.at(0).start.time(), QTime(7, 30));
    QCOMPARE(result.movedBlocks.at(0).end.time(), QTime(9, 30));
    QCOMPARE(result.movedBlocks.at(1).start.time(), QTime(5, 30));
    QCOMPARE(result.movedBlocks.at(1).end.time(), QTime(7, 30));
}

void DatabaseConsistencyTests::cascadeResizeUpSkipsFixedIntervals()
{
    const QDate date(2026, 6, 21);
    TimeBlock block;
    block.id = 1;
    block.start = QDateTime(date, QTime(8, 30));
    block.end = QDateTime(date, QTime(10, 30));
    const TimeInterval fixed {
        QDateTime(date, QTime(6, 30)),
        QDateTime(date, QTime(8, 30))
    };

    const CascadePlanResult result = planCascadeUp(
        QDateTime(date, QTime(9, 30)), QDateTime(date, QTime(13, 0)),
        QDateTime(date, QTime(0, 0)), {block}, {fixed});

    QVERIFY(result.ok);
    QCOMPARE(result.movedBlocks.size(), 1);
    QCOMPARE(result.movedBlocks.first().start.time(), QTime(4, 30));
    QCOMPARE(result.movedBlocks.first().end.time(), QTime(6, 30));
}

void DatabaseConsistencyTests::cascadeResizeUpRejectsDayUnderflow()
{
    const QDate date(2026, 6, 21);
    TimeBlock block;
    block.id = 1;
    block.start = QDateTime(date, QTime(0, 30));
    block.end = QDateTime(date, QTime(2, 30));

    const CascadePlanResult result = planCascadeUp(
        QDateTime(date, QTime(1, 0)), QDateTime(date, QTime(5, 0)),
        QDateTime(date, QTime(0, 0)), {block}, {});

    QVERIFY(!result.ok);
    QCOMPARE(result.error, QStringLiteral("day-underflow"));
    QVERIFY(result.movedBlocks.isEmpty());
}

void DatabaseConsistencyTests::horizontalRelocationPrefersUpForLeftExtension()
{
    const QDate date(2026, 6, 21);
    TimeBlock block;
    block.id = 1;
    block.start = QDateTime(date, QTime(10, 30));
    block.end = QDateTime(date, QTime(11, 30));

    const CascadePlanResult result = planHorizontalRelocation(
        QDateTime(date, QTime(10, 0)), QDateTime(date, QTime(12, 0)),
        QDateTime(date, QTime(0, 0)), QDateTime(date.addDays(1), QTime(0, 0)),
        {block}, {}, true);

    QVERIFY(result.ok);
    QCOMPARE(result.movedBlocks.size(), 1);
    QCOMPARE(result.movedBlocks.first().start.time(), QTime(9, 0));
    QCOMPARE(result.movedBlocks.first().end.time(), QTime(10, 0));
}

void DatabaseConsistencyTests::horizontalRelocationPrefersDownForRightExtension()
{
    const QDate date(2026, 6, 21);
    TimeBlock block;
    block.id = 1;
    block.start = QDateTime(date, QTime(10, 30));
    block.end = QDateTime(date, QTime(11, 30));

    const CascadePlanResult result = planHorizontalRelocation(
        QDateTime(date, QTime(10, 0)), QDateTime(date, QTime(12, 0)),
        QDateTime(date, QTime(0, 0)), QDateTime(date.addDays(1), QTime(0, 0)),
        {block}, {}, false);

    QVERIFY(result.ok);
    QCOMPARE(result.movedBlocks.size(), 1);
    QCOMPARE(result.movedBlocks.first().start.time(), QTime(12, 0));
    QCOMPARE(result.movedBlocks.first().end.time(), QTime(13, 0));
}

void DatabaseConsistencyTests::horizontalRelocationAvoidsFixedAndOtherBlocks()
{
    const QDate date(2026, 6, 21);
    TimeBlock first;
    first.id = 1;
    first.start = QDateTime(date, QTime(10, 0));
    first.end = QDateTime(date, QTime(11, 0));
    TimeBlock second;
    second.id = 2;
    second.start = QDateTime(date, QTime(11, 0));
    second.end = QDateTime(date, QTime(12, 30));
    TimeBlock existing;
    existing.id = 3;
    existing.start = QDateTime(date, QTime(12, 30));
    existing.end = QDateTime(date, QTime(14, 0));
    const TimeInterval fixed {
        QDateTime(date, QTime(8, 0)),
        QDateTime(date, QTime(9, 0))
    };

    const CascadePlanResult result = planHorizontalRelocation(
        QDateTime(date, QTime(9, 30)), QDateTime(date, QTime(12, 30)),
        QDateTime(date, QTime(0, 0)), QDateTime(date.addDays(1), QTime(0, 0)),
        {first, second, existing}, {fixed}, true);

    QVERIFY(result.ok);
    QCOMPARE(result.movedBlocks.size(), 2);
    for (const TimeBlock& moved : result.movedBlocks) {
        QCOMPARE(moved.start.secsTo(moved.end),
                 moved.id == first.id ? first.start.secsTo(first.end) : second.start.secsTo(second.end));
        QVERIFY(!(moved.start < existing.end && existing.start < moved.end));
        QVERIFY(!(moved.start < fixed.end && fixed.start < moved.end));
    }
    QVERIFY(!(result.movedBlocks.at(0).start < result.movedBlocks.at(1).end
              && result.movedBlocks.at(1).start < result.movedBlocks.at(0).end));
}

void DatabaseConsistencyTests::horizontalRelocationRejectsFullDay()
{
    const QDate date(2026, 6, 21);
    TimeBlock block;
    block.id = 1;
    block.start = QDateTime(date, QTime(10, 0));
    block.end = QDateTime(date, QTime(12, 0));
    const QVector<TimeInterval> fixed {
        {QDateTime(date, QTime(0, 0)), QDateTime(date, QTime(1, 0))},
        {QDateTime(date, QTime(23, 0)), QDateTime(date.addDays(1), QTime(0, 0))}
    };

    const CascadePlanResult result = planHorizontalRelocation(
        QDateTime(date, QTime(1, 0)), QDateTime(date, QTime(23, 0)),
        QDateTime(date, QTime(0, 0)), QDateTime(date.addDays(1), QTime(0, 0)),
        {block}, fixed, false);

    QVERIFY(!result.ok);
    QCOMPARE(result.error, QStringLiteral("no-horizontal-space"));
    QVERIFY(result.movedBlocks.isEmpty());
}

QTEST_MAIN(DatabaseConsistencyTests)
#include "DatabaseConsistencyTests.moc"
