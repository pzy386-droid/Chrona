#pragma once

#include "ai/AIService.h"

class DeepSeekProvider final : public AIService {
    Q_OBJECT

public:
    explicit DeepSeekProvider(QObject* parent = nullptr)
        : AIService(parent)
    {
    }

    QFuture<AIParseResult> parseNaturalLanguageTask(const QString& input) override
    {
        Q_UNUSED(input)
        return {};
    }

    QFuture<AISuggestionResult> suggestScheduleChanges(const ScheduleContext& context) override
    {
        Q_UNUSED(context)
        return {};
    }
};
