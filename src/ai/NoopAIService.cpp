#include "ai/NoopAIService.h"

#include <QtConcurrent/QtConcurrent>

NoopAIService::NoopAIService(QObject* parent)
    : AIService(parent)
{
}

QString NoopAIService::providerName() const
{
    return QStringLiteral("noop");
}

bool NoopAIService::isConfigured() const
{
    return false;
}

bool NoopAIService::setApiKey(const QString& apiKey)
{
    Q_UNUSED(apiKey)
    return false;
}

QFuture<AIParseResult> NoopAIService::parseNaturalLanguageTask(const QString& input)
{
    Q_UNUSED(input)
    return QtConcurrent::run([] {
        return AIParseResult{false, {}, QObject::tr("AI 输入将在配置 API Key 后启用"), QStringLiteral("noop")};
    });
}

QFuture<AISuggestionResult> NoopAIService::suggestScheduleChanges(const ScheduleContext& context)
{
    Q_UNUSED(context)
    return QtConcurrent::run([] {
        return AISuggestionResult{false, {}, QObject::tr("智能调度建议将在配置 API Key 后启用"), QStringLiteral("noop")};
    });
}
