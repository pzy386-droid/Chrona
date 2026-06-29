#include "managers/NLPTaskParser.h"

#include <QTest>

class NLPTaskParserTests : public QObject {
    Q_OBJECT

private slots:
    void parsesNextFridayHomework()
    {
        NLPTaskParser parser;
        const QDateTime now(QDate(2026, 6, 11), QTime(10, 0)); // Thursday
        const ParsedTaskDraft draft = parser.parse(
            QStringLiteral("下周五前完成高数作业，大约三小时"), now);

        QVERIFY(draft.valid);
        QCOMPARE(draft.deadline.date(), QDate(2026, 6, 19));
        QCOMPARE(draft.estimatedMinutes, 180);
        QCOMPARE(draft.categoryName, QStringLiteral("高数"));
    }

    void parsesExplicitTimeRange()
    {
        NLPTaskParser parser;
        const QDateTime now(QDate(2026, 6, 27), QTime(10, 0));
        const ParsedTaskDraft draft = parser.parse(
            QStringLiteral("今天从中午12:00到晚上22:00复习微积分"), now);

        QVERIFY(draft.valid);
        QVERIFY(draft.hasTimeAnchor);
        QCOMPARE(draft.scheduledStart, QDateTime(QDate(2026, 6, 27), QTime(12, 0)));
        QCOMPARE(draft.scheduledEnd, QDateTime(QDate(2026, 6, 27), QTime(22, 0)));
        QCOMPARE(draft.estimatedMinutes, 600);
        QCOMPARE(static_cast<int>(draft.priority), static_cast<int>(Priority::Medium));
    }

};

QTEST_MAIN(NLPTaskParserTests)
#include "NLPTaskParserTests.moc"
