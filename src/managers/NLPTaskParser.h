#pragma once

#include "core/models/Task.h"

#include <QDateTime>
#include <QString>

struct ParsedTaskDraft {
    QString title;
    QString notes;
    QDateTime deadline;
    int estimatedMinutes = 90;
    Priority priority = Priority::Medium;
    QString categoryName;
    QString preferredStudyTime;
    bool hasTimeAnchor = false;
    QDateTime scheduledStart;
    QDateTime scheduledEnd;
    bool valid = false;
    QString message;
};

class NLPTaskParser {
public:
    ParsedTaskDraft parse(const QString& input, const QDateTime& now = QDateTime::currentDateTime()) const;

private:
    QDate resolveRelativeDate(const QString& input, const QDate& today) const;
    QTime resolveTime(const QString& input) const;
    bool hasSchedulingAnchor(const QString& input) const;
    Priority resolvePriority(const QString& input) const;
    QString resolveCategory(const QString& input) const;
    int resolveDurationMinutes(const QString& input) const;
};
