#include "ai/NoopAIService.h"

#include <QtConcurrent/QtConcurrent>

NoopAIService::NoopAIService(QObject* parent)
    : AIService(parent)
{
}

QFuture<AIParseResult> NoopAIService::parseNaturalLanguageTask(const QString& input)
{
    Q_UNUSED(input)
    return QtConcurrent::run([] {
        return AIParseResult{false, {}, QObject::tr("AI 输入将在后续版本接入")};
    });
}

QFuture<AISuggestionResult> NoopAIService::suggestScheduleChanges(const ScheduleContext& context)
{
    Q_UNUSED(context)
    return QtConcurrent::run([] {
        return AISuggestionResult{false, {}, QObject::tr("智能调度建议将在后续版本接入")};
    });
}
