#pragma once

#include "ai/AIService.h"

class DeepSeekProvider final : public AIService {
    Q_OBJECT

public:
    explicit DeepSeekProvider(QString apiKey, QObject* parent = nullptr);

    QFuture<AIParseResult> parseNaturalLanguageTask(const QString& input) override;
    QFuture<AISuggestionResult> suggestScheduleChanges(const ScheduleContext& context) override;
    QString providerName() const override;
    bool isConfigured() const override;
    bool setApiKey(const QString& apiKey) override;

private:
    QString m_apiKey;
    QString m_endpoint = QStringLiteral("https://api.deepseek.com/chat/completions");
    QString m_model = QStringLiteral("deepseek-chat");
};
