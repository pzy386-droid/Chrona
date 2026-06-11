#include "ai/providers/DeepSeekProvider.h"

#include <QDate>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>
#include <QtGlobal>

#include <utility>

namespace {
QJsonObject callDeepSeekJson(const QString& endpoint, const QString& model, const QString& apiKey,
                             const QString& systemPrompt, const QString& userPrompt, QString* error)
{
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(endpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(apiKey).toUtf8());

    const QJsonObject payload{
        {QStringLiteral("model"), model},
        {QStringLiteral("temperature"), 0.2},
        {QStringLiteral("response_format"), QJsonObject{{QStringLiteral("type"), QStringLiteral("json_object")}}},
        {QStringLiteral("messages"), QJsonArray{
            QJsonObject{{QStringLiteral("role"), QStringLiteral("system")}, {QStringLiteral("content"), systemPrompt}},
            QJsonObject{{QStringLiteral("role"), QStringLiteral("user")}, {QStringLiteral("content"), userPrompt}}
        }}
    };

    QNetworkReply* reply = manager.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(15000);
    loop.exec();

    if (!timeout.isActive()) {
        reply->abort();
        reply->deleteLater();
        if (error) {
            *error = QObject::tr("AI 请求超时，已回退到本地规则");
        }
        return {};
    }
    timeout.stop();

    const QByteArray body = reply->readAll();
    const auto networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    reply->deleteLater();
    if (networkError != QNetworkReply::NoError) {
        if (error) {
            *error = QObject::tr("AI 请求失败：%1").arg(networkErrorText);
        }
        return {};
    }

    const QJsonObject response = QJsonDocument::fromJson(body).object();
    const QJsonArray choices = response.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        if (error) {
            *error = QObject::tr("AI 返回为空，已回退到本地规则");
        }
        return {};
    }
    const QString content = choices.first().toObject()
        .value(QStringLiteral("message")).toObject()
        .value(QStringLiteral("content")).toString();
    const QJsonDocument contentDoc = QJsonDocument::fromJson(content.toUtf8());
    if (!contentDoc.isObject()) {
        if (error) {
            *error = QObject::tr("AI 未返回有效 JSON，已回退到本地规则");
        }
        return {};
    }
    return contentDoc.object();
}

QVariantMap normalizedTaskDraft(const QJsonObject& object)
{
    return {
        {QStringLiteral("title"), object.value(QStringLiteral("title")).toString().trimmed()},
        {QStringLiteral("notes"), object.value(QStringLiteral("notes")).toString(QObject::tr("由 AI 规划助手解析创建"))},
        {QStringLiteral("deadline"), object.value(QStringLiteral("deadline")).toString().trimmed()},
        {QStringLiteral("estimatedMinutes"), qBound(30, object.value(QStringLiteral("estimatedMinutes")).toInt(60), 1440)},
        {QStringLiteral("priority"), qBound(0, object.value(QStringLiteral("priority")).toInt(1), 2)},
        {QStringLiteral("categoryName"), object.value(QStringLiteral("categoryName")).toString(QObject::tr("学习"))},
        {QStringLiteral("preferredStudyTime"), object.value(QStringLiteral("preferredStudyTime")).toString(QStringLiteral("evening"))},
        {QStringLiteral("explanation"), object.value(QStringLiteral("explanation")).toString()},
        {QStringLiteral("source"), QStringLiteral("deepseek")}
    };
}

QVariantList normalizedTaskDrafts(const QJsonObject& object)
{
    QVariantList drafts;
    const QJsonArray tasks = object.value(QStringLiteral("tasks")).toArray();
    for (const QJsonValue& value : tasks) {
        if (!value.isObject()) {
            continue;
        }
        const QVariantMap draft = normalizedTaskDraft(value.toObject());
        if (!draft.value(QStringLiteral("title")).toString().isEmpty()
            && !draft.value(QStringLiteral("deadline")).toString().isEmpty()) {
            drafts.push_back(draft);
        }
    }
    if (drafts.isEmpty()) {
        const QVariantMap draft = normalizedTaskDraft(object);
        if (!draft.value(QStringLiteral("title")).toString().isEmpty()
            && !draft.value(QStringLiteral("deadline")).toString().isEmpty()) {
            drafts.push_back(draft);
        }
    }
    return drafts;
}
}

DeepSeekProvider::DeepSeekProvider(QString apiKey, QObject* parent)
    : AIService(parent)
    , m_apiKey(std::move(apiKey))
{
}

QFuture<AIParseResult> DeepSeekProvider::parseNaturalLanguageTask(const QString& input)
{
    const QString apiKey = m_apiKey;
    const QString endpoint = m_endpoint;
    const QString model = m_model;
    return QtConcurrent::run([apiKey, endpoint, model, input] {
        if (apiKey.trimmed().isEmpty()) {
            return AIParseResult{false, {}, QObject::tr("未配置 DEEPSEEK_API_KEY，使用本地规则解析"), QStringLiteral("deepseek")};
        }
        const QString systemPrompt = QStringLiteral(
            "You are Chrona's AI planning parser. Extract study tasks from Chinese or English input. "
            "Return strict JSON only as {\"tasks\":[...],\"message\":\"...\"}. Each task has: title, notes, "
            "deadline, estimatedMinutes, priority, categoryName, preferredStudyTime, explanation. Split clearly "
            "distinct tasks. deadline must be yyyy-MM-dd HH:mm; priority is 0, 1, or 2; preferredStudyTime is "
            "morning, afternoon, evening, or any. Use conservative defaults for ambiguity. No markdown.");
        const QString userPrompt = QStringLiteral("Today is %1. User input: %2")
            .arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")), input);
        QString error;
        const QJsonObject object = callDeepSeekJson(endpoint, model, apiKey, systemPrompt, userPrompt, &error);
        if (object.isEmpty()) {
            return AIParseResult{false, {}, error, QStringLiteral("deepseek")};
        }
        const QVariantList drafts = normalizedTaskDrafts(object);
        if (drafts.isEmpty()) {
            return AIParseResult{false, {}, QObject::tr("AI 结果缺少任务标题或截止时间，使用本地规则解析"), QStringLiteral("deepseek")};
        }
        const QString message = object.value(QStringLiteral("message")).toString(
            QObject::tr("AI 已生成任务草稿，请确认后加入日程"));
        return AIParseResult{true, drafts.first().toMap(), message, QStringLiteral("deepseek"), drafts};
    });
}

QFuture<AISuggestionResult> DeepSeekProvider::suggestScheduleChanges(const ScheduleContext& context)
{
    const QString apiKey = m_apiKey;
    const QString endpoint = m_endpoint;
    const QString model = m_model;
    const QVariantMap data = context.data;
    return QtConcurrent::run([apiKey, endpoint, model, data] {
        if (apiKey.trimmed().isEmpty()) {
            return AISuggestionResult{false, {}, QObject::tr("未配置 DEEPSEEK_API_KEY，使用本地建议"), QStringLiteral("deepseek")};
        }
        const QString systemPrompt = QStringLiteral(
            "You are Chrona's study schedule advisor. The deterministic local scheduler is authoritative. Never "
            "claim to have changed data. Return strict JSON only with keys title, summary, riskLevel, currentFocus, "
            "reasons, suggestions, providerLabel. reasons is a string array. suggestions is an array with title, "
            "description, impact, actionType, taskId, proposedDeadline. Allowed actionType values are reschedule, "
            "extend_deadline, review_task. Use only supplied facts; do not invent tasks, events, or times. No markdown.");
        const QString userPrompt = QStringLiteral("Mode: %1\nContext JSON: %2")
            .arg(data.value(QStringLiteral("mode")).toString(),
                 QString::fromUtf8(QJsonDocument::fromVariant(data).toJson(QJsonDocument::Compact)));
        QString error;
        const QJsonObject object = callDeepSeekJson(endpoint, model, apiKey, systemPrompt, userPrompt, &error);
        if (object.isEmpty()) {
            return AISuggestionResult{false, {}, error, QStringLiteral("deepseek")};
        }
        QVariantMap suggestion = object.toVariantMap();
        const QString summary = suggestion.value(QStringLiteral("summary")).toString().trimmed();
        if (summary.isEmpty()) {
            return AISuggestionResult{false, {}, QObject::tr("AI 建议缺少摘要，使用本地建议"), QStringLiteral("deepseek")};
        }
        suggestion.insert(QStringLiteral("provider"), QStringLiteral("deepseek"));
        suggestion.insert(QStringLiteral("model"), model);
        return AISuggestionResult{true, suggestion, summary, QStringLiteral("deepseek")};
    });
}
