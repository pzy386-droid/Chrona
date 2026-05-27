#include "managers/NLPTaskParser.h"

#include <QObject>
#include <QRegularExpression>
#include <QtMath>

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
    draft.title = text;
    draft.notes = QObject::tr("由 Quick Add 规则解析创建");
    draft.valid = true;
    draft.message = QObject::tr("已识别任务、截止时间和课程分类");
    return draft;
}

QDate NLPTaskParser::resolveRelativeDate(const QString& input, const QDate& today) const
{
    if (input.contains(QStringLiteral("明天"))) {
        return today.addDays(1);
    }
    if (input.contains(QStringLiteral("后天"))) {
        return today.addDays(2);
    }

    static const QVector<QString> weekdays = {
        QStringLiteral("周一"), QStringLiteral("周二"), QStringLiteral("周三"), QStringLiteral("周四"), QStringLiteral("周五"), QStringLiteral("周六"), QStringLiteral("周日")
    };
    for (int i = 0; i < weekdays.size(); ++i) {
        if (input.contains(weekdays.at(i))) {
            const int target = i + 1;
            int delta = target - today.dayOfWeek();
            if (delta <= 0 || input.contains(QStringLiteral("下周"))) {
                delta += 7;
            }
            return today.addDays(delta);
        }
    }

    QRegularExpression md(QStringLiteral("(\\d{1,2})月(\\d{1,2})日"));
    const auto match = md.match(input);
    if (match.hasMatch()) {
        return QDate(today.year(), match.captured(1).toInt(), match.captured(2).toInt());
    }

    return today.addDays(2);
}

QTime NLPTaskParser::resolveTime(const QString& input) const
{
    QRegularExpression explicitTime(QStringLiteral("(上午|下午|晚上|中午)?\\s*(\\d{1,2})(?:点|:)(\\d{1,2})?"));
    const auto match = explicitTime.match(input);
    if (match.hasMatch()) {
        int hour = match.captured(2).toInt();
        const int minute = match.captured(3).isEmpty() ? 0 : match.captured(3).toInt();
        const QString period = match.captured(1);
        if ((period == QStringLiteral("下午") || period == QStringLiteral("晚上")) && hour < 12) {
            hour += 12;
        }
        if (period == QStringLiteral("中午") && hour < 11) {
            hour += 12;
        }
        return QTime(qBound(0, hour, 23), qBound(0, minute, 59));
    }
    if (input.contains(QStringLiteral("上午"))) {
        return QTime(10, 0);
    }
    if (input.contains(QStringLiteral("下午"))) {
        return QTime(15, 0);
    }
    if (input.contains(QStringLiteral("晚上"))) {
        return QTime(21, 0);
    }
    return QTime(23, 0);
}

Priority NLPTaskParser::resolvePriority(const QString& input) const
{
    if (input.contains(QStringLiteral("考试")) || input.contains(QStringLiteral("ddl"), Qt::CaseInsensitive) || input.contains(QStringLiteral("紧急"))) {
        return Priority::High;
    }
    if (input.contains(QStringLiteral("整理")) || input.contains(QStringLiteral("预习"))) {
        return Priority::Low;
    }
    return Priority::Medium;
}

QString NLPTaskParser::resolveCategory(const QString& input) const
{
    const QVector<QString> categories = {
        QStringLiteral("高数"), QStringLiteral("数学"), QStringLiteral("英语"), QStringLiteral("数据库"), QStringLiteral("数据结构"), QStringLiteral("计算机")
    };
    for (const auto& category : categories) {
        if (input.contains(category)) {
            return category;
        }
    }
    return QStringLiteral("学习");
}

int NLPTaskParser::resolveDurationMinutes(const QString& input) const
{
    QRegularExpression hour(QStringLiteral("(\\d+(?:\\.\\d+)?)\\s*(小时|h)"));
    const auto hourMatch = hour.match(input);
    if (hourMatch.hasMatch()) {
        return qMax(30, qRound(hourMatch.captured(1).toDouble() * 60.0));
    }

    QRegularExpression minute(QStringLiteral("(\\d+)\\s*(分钟|min)"));
    const auto minuteMatch = minute.match(input);
    if (minuteMatch.hasMatch()) {
        return qMax(30, minuteMatch.captured(1).toInt());
    }

    if (input.contains(QStringLiteral("考试")) || input.contains(QStringLiteral("复习"))) {
        return 120;
    }
    if (input.contains(QStringLiteral("作业")) || input.contains(QStringLiteral("报告"))) {
        return 90;
    }
    return 60;
}
