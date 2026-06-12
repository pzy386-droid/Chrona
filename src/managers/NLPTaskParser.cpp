#include "managers/NLPTaskParser.h"

#include <QObject>
#include <QRegularExpression>
#include <QtMath>

namespace {
bool containsAny(const QString& input, std::initializer_list<QString> words)
{
    for (const QString& word : words) {
        if (input.contains(word, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

int weekdayFromText(const QString& input)
{
    const QVector<QStringList> names{
        {QStringLiteral("周一"), QStringLiteral("星期一")},
        {QStringLiteral("周二"), QStringLiteral("星期二")},
        {QStringLiteral("周三"), QStringLiteral("星期三")},
        {QStringLiteral("周四"), QStringLiteral("星期四")},
        {QStringLiteral("周五"), QStringLiteral("星期五")},
        {QStringLiteral("周六"), QStringLiteral("星期六")},
        {QStringLiteral("周日"), QStringLiteral("星期日"), QStringLiteral("星期天")}
    };
    for (int i = 0; i < names.size(); ++i) {
        for (const QString& name : names.at(i)) {
            if (input.contains(name)) {
                return i + 1;
            }
        }
    }
    return containsAny(input, {QStringLiteral("周末"), QStringLiteral("这个周末")}) ? 6 : 0;
}
}

ParsedTaskDraft NLPTaskParser::parse(const QString& input, const QDateTime& now) const
{
    ParsedTaskDraft draft;
    const QString text = input.trimmed();
    if (text.isEmpty()) {
        draft.message = QObject::tr("请输入任务内容");
        return draft;
    }

    const QDate date = resolveRelativeDate(text, now.date());
    const QTime time = resolveTime(text);
    draft.deadline = QDateTime(date, time);
    draft.estimatedMinutes = resolveDurationMinutes(text);
    draft.priority = resolvePriority(text);
    draft.categoryName = resolveCategory(text);
    draft.preferredStudyTime = time.hour() < 12 ? QStringLiteral("morning") : time.hour() < 18 ? QStringLiteral("afternoon") : QStringLiteral("evening");
    draft.hasTimeAnchor = hasSchedulingAnchor(text);
    if (draft.hasTimeAnchor) {
        draft.scheduledStart = QDateTime(date, time);
        draft.scheduledEnd = draft.scheduledStart.addSecs(draft.estimatedMinutes * 60);
        draft.deadline = draft.scheduledEnd;
    }
    draft.title = text;
    draft.notes = QObject::tr("由 Quick Add 本地规则解析创建");
    draft.valid = true;
    draft.message = QObject::tr("已识别任务、截止时间、优先级和学习分类");
    return draft;
}

QDate NLPTaskParser::resolveRelativeDate(const QString& input, const QDate& today) const
{
    if (containsAny(input, {QStringLiteral("今晚"), QStringLiteral("今天")})) {
        return today;
    }
    if (containsAny(input, {QStringLiteral("明早"), QStringLiteral("明天")})) {
        return today.addDays(1);
    }
    if (input.contains(QStringLiteral("后天"))) {
        return today.addDays(2);
    }
    if (input.contains(QStringLiteral("月底"))) {
        return QDate(today.year(), today.month(), today.daysInMonth());
    }

    const int targetWeekday = weekdayFromText(input);
    if (targetWeekday > 0) {
        int delta = targetWeekday - today.dayOfWeek();
        const bool nextWeek = containsAny(input, {QStringLiteral("下周"), QStringLiteral("下星期")});
        const bool thisWeek = containsAny(input, {QStringLiteral("本周"), QStringLiteral("这周"), QStringLiteral("这星期")});
        if (nextWeek) {
            delta += delta > 0 ? 7 : 14;
        } else if (delta < 0 || (!thisWeek && delta == 0)) {
            delta += 7;
        }
        return today.addDays(delta);
    }

    const QRegularExpression monthDay(QStringLiteral("(\\d{1,2})\\s*月\\s*(\\d{1,2})\\s*(?:日|号)?"));
    const auto match = monthDay.match(input);
    if (match.hasMatch()) {
        QDate date(today.year(), match.captured(1).toInt(), match.captured(2).toInt());
        if (date.isValid() && date < today) {
            date = date.addYears(1);
        }
        if (date.isValid()) {
            return date;
        }
    }
    return today.addDays(2);
}

QTime NLPTaskParser::resolveTime(const QString& input) const
{
    const QRegularExpression explicitTime(
        QStringLiteral("(凌晨|早上|上午|中午|下午|晚上|今晚|明早)?\\s*(\\d{1,2})\\s*(?:点|:|：)\\s*(\\d{1,2})?"));
    const auto match = explicitTime.match(input);
    if (match.hasMatch()) {
        int hour = match.captured(2).toInt();
        const int minute = match.captured(3).isEmpty() ? 0 : match.captured(3).toInt();
        const QString period = match.captured(1);
        if (containsAny(period, {QStringLiteral("下午"), QStringLiteral("晚上"), QStringLiteral("今晚")}) && hour < 12) {
            hour += 12;
        }
        if (period == QStringLiteral("中午") && hour < 11) {
            hour += 12;
        }
        return QTime(qBound(0, hour, 23), qBound(0, minute, 59));
    }
    if (containsAny(input, {QStringLiteral("明早"), QStringLiteral("早上"), QStringLiteral("上午")})) {
        return QTime(9, 0);
    }
    if (input.contains(QStringLiteral("下午"))) {
        return QTime(15, 0);
    }
    if (input.contains(QStringLiteral("中午")) || input.contains(QStringLiteral("午饭"))) {
        return QTime(12, 0);
    }
    if (input.contains(QStringLiteral("今晚")) || input.contains(QStringLiteral("晚上"))) {
        return QTime(21, 0);
    }
    if (containsAny(input, {QStringLiteral("考试"), QStringLiteral("截止")})
        || input.contains(QStringLiteral("DDL"), Qt::CaseInsensitive)) {
        return QTime(23, 59);
    }
    return QTime(23, 0);
}

bool NLPTaskParser::hasSchedulingAnchor(const QString& input) const
{
    const bool hasTimeWord = containsAny(input, {
        QStringLiteral("凌晨"), QStringLiteral("早上"), QStringLiteral("上午"), QStringLiteral("中午"),
        QStringLiteral("下午"), QStringLiteral("晚上"), QStringLiteral("今晚"), QStringLiteral("明早"),
        QStringLiteral("午饭"), QStringLiteral("晚饭")
    });
    QRegularExpression explicitTime(QStringLiteral("(\\d{1,2})\\s*(点|:|：)\\s*(\\d{1,2})?"));
    const bool hasExplicitTime = explicitTime.match(input).hasMatch();
    const bool deadlineOnly = input.contains(QStringLiteral("截止"))
        || input.contains(QStringLiteral("DDL"), Qt::CaseInsensitive)
        || input.contains(QStringLiteral("deadline"), Qt::CaseInsensitive)
        || input.contains(QStringLiteral("交"));
    return (hasTimeWord || hasExplicitTime) && !deadlineOnly;
}

Priority NLPTaskParser::resolvePriority(const QString& input) const
{
    if (containsAny(input, {QStringLiteral("很急"), QStringLiteral("紧急"), QStringLiteral("考试"), QStringLiteral("截止")})
        || input.contains(QStringLiteral("DDL"), Qt::CaseInsensitive)) {
        return Priority::High;
    }
    if (containsAny(input, {QStringLiteral("整理"), QStringLiteral("预习")})) {
        return Priority::Low;
    }
    return Priority::Medium;
}

QString NLPTaskParser::resolveCategory(const QString& input) const
{
    const QVector<QString> subjects{
        QStringLiteral("高数"), QStringLiteral("数学"), QStringLiteral("英语"), QStringLiteral("数据库"),
        QStringLiteral("数据结构"), QStringLiteral("计算机"), QStringLiteral("线性代数")
    };
    for (const QString& subject : subjects) {
        if (input.contains(subject, Qt::CaseInsensitive)) {
            return subject;
        }
    }
    if (containsAny(input, {QStringLiteral("实验报告"), QStringLiteral("报告")})) {
        return QObject::tr("实验报告");
    }
    if (input.contains(QStringLiteral("PPT"), Qt::CaseInsensitive)) {
        return QStringLiteral("PPT");
    }
    if (input.contains(QStringLiteral("演讲"))) {
        return QObject::tr("演讲");
    }
    if (containsAny(input, {QStringLiteral("复习"), QStringLiteral("考试")})) {
        return QObject::tr("复习");
    }
    if (input.contains(QStringLiteral("作业"))) {
        return QObject::tr("作业");
    }
    if (containsAny(input, {QStringLiteral("吃饭"), QStringLiteral("午饭"), QStringLiteral("晚饭")})) {
        return QObject::tr("生活");
    }
    return QObject::tr("学习");
}

int NLPTaskParser::resolveDurationMinutes(const QString& input) const
{
    const QRegularExpression hour(
        QStringLiteral("(\\d+(?:\\.\\d+)?)\\s*(小时|个小时|h)"),
        QRegularExpression::CaseInsensitiveOption);
    const auto hourMatch = hour.match(input);
    if (hourMatch.hasMatch()) {
        return qMax(30, qRound(hourMatch.captured(1).toDouble() * 60.0));
    }

    const QRegularExpression chineseHour(
        QStringLiteral("(半|一|两|二|三|四|五|六|七|八)\\s*(?:个)?小时"));
    const auto chineseMatch = chineseHour.match(input);
    if (chineseMatch.hasMatch()) {
        const QString value = chineseMatch.captured(1);
        const QHash<QString, int> minutes{
            {QStringLiteral("半"), 30}, {QStringLiteral("一"), 60}, {QStringLiteral("两"), 120},
            {QStringLiteral("二"), 120}, {QStringLiteral("三"), 180}, {QStringLiteral("四"), 240},
            {QStringLiteral("五"), 300}, {QStringLiteral("六"), 360}, {QStringLiteral("七"), 420},
            {QStringLiteral("八"), 480}
        };
        return minutes.value(value, 60);
    }

    const QRegularExpression minute(
        QStringLiteral("(\\d+)\\s*(分钟|min)"),
        QRegularExpression::CaseInsensitiveOption);
    const auto minuteMatch = minute.match(input);
    if (minuteMatch.hasMatch()) {
        return qMax(30, minuteMatch.captured(1).toInt());
    }
    if (containsAny(input, {QStringLiteral("考试"), QStringLiteral("复习"), QStringLiteral("报告"), QStringLiteral("PPT"), QStringLiteral("演讲")})) {
        return 120;
    }
    return input.contains(QStringLiteral("作业")) ? 90 : 60;
}
