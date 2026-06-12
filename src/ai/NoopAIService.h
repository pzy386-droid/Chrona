#pragma once

#include "ai/AIService.h"

class NoopAIService final : public AIService {
    Q_OBJECT

public:
    explicit NoopAIService(QObject* parent = nullptr);

    QFuture<AIParseResult> parseNaturalLanguageTask(const QString& input) override;
    QFuture<AISuggestionResult> suggestScheduleChanges(const ScheduleContext& context) override;
    QString providerName() const override;
    bool isConfigured() const override;
    bool setApiKey(const QString& apiKey) override;
};
