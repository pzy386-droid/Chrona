#pragma once

#include <QFuture>
#include <QObject>
#include <QString>
#include <QVariantMap>

struct AIParseResult {
    bool supported = false;
    QVariantMap taskDraft;
    QString message;
    QString provider;
};

struct AISuggestionResult {
    bool supported = false;
    QVariantMap suggestion;
    QString message;
    QString provider;
};

struct ScheduleContext {
    QVariantMap data;
};

class AIService : public QObject {
    Q_OBJECT

public:
    explicit AIService(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    virtual QFuture<AIParseResult> parseNaturalLanguageTask(const QString& input) = 0;
    virtual QFuture<AISuggestionResult> suggestScheduleChanges(const ScheduleContext& context) = 0;
};
