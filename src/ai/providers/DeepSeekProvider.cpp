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
#include <QtGlobal>
#include <QtConcurrent/QtConcurrent>

#include <utility>

namespace {
QJsonObject callDeepSeekJson(const QString& endpoint, const QString& model, const QString& apiKey, const QString& systemPrompt, const QString& userPrompt, QString* error)
{
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(endpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(apiKey).toUtf8());

    QJsonObject payload{
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

    if (timeout.isActive()) {
        timeout.stop();
    } else {
        reply->abort();
        reply->deleteLater();
        if (error) {
            *error = QObject::tr("AI 请求超时，已回退到本地规则解析");
        }
        return {};
    }

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

    const QJsonDocument response = QJsonDocument::fromJson(body);
    const QJsonArray choices = response.object().value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        if (error) {
            *error = QObject::tr("AI 返回为空，已回退到本地规则解析");
        }
        return {};
    }

    const QString content = choices.first().toObject()
        .value(QStringLiteral("message")).toObject()
        .value(QStringLiteral("content")).toString();
    const QJsonDocument contentDoc = QJsonDocument::fromJson(content.toUtf8());
    if (!contentDoc.isObject()) {
        if (error) {
            *error = QObject::tr("AI 没有返回结构化 JSON，已回退到本地规则解析");
        }
        return {};
    }
    return contentDoc.object();
}

QVariantMap normalizedTaskDraft(const QJsonObject& object)
{
    const int priority = qBound(0, object.value(QStringLiteral("priority")).toInt(1), 2);
    const int estimatedMinutes = qMax(30, object.value(QStringLiteral("estimatedMinutes")).toInt(60));
    return {
        {QStringLiteral("title"), object.value(QStringLiteral("title")).toString()},
        {QStringLiteral("notes"), object.value(QStringLiteral("notes")).toString(QObject::tr("由 AI 规划助手解析创建"))},
        {QStringLiteral("deadline"), object.value(QStringLiteral("deadline")).toString()},
        {QStringLiteral("estimatedMinutes"), estimatedMinutes},
        {QStringLiteral("priority"), priority},
        {QStringLiteral("categoryName"), object.value(QStringLiteral("categoryName")).toString(QObject::tr("学习"))},
        {QStringLiteral("preferredStudyTime"), object.value(QStringLiteral("preferredStudyTime")).toString(QStringLiteral("evening"))},
        {QStringLiteral("explanation"), object.value(QStringLiteral("explanation")).toString()},
        {QStringLiteral("source"), QStringLiteral("deepseek")}
    };
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
            "You are Chrona's AI planning parser. Extract a study task from Chinese or English input. "
            "Return strict JSON only with keys: title, notes, deadline, estimatedMinutes, priority, "
            "categoryName, preferredStudyTime, explanation. deadline format must be yyyy-MM-dd HH:mm. "
            "priority: 0 low, 1 medium, 2 high. preferredStudyTime: morning, afternoon, evening, or any. "
            "Do not include markdown."
        );
        const QString userPrompt = QStringLiteral("Today is %1. User input: %2")
            .arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")), input);

        QString error;
        const QJsonObject object = callDeepSeekJson(endpoint, model, apiKey, systemPrompt, userPrompt, &error);
        if (object.isEmpty()) {
            return AIParseResult{false, {}, error, QStringLiteral("deepseek")};
        }

        QVariantMap draft = normalizedTaskDraft(object);
        if (draft.value(QStringLiteral("title")).toString().trimmed().isEmpty() || draft.value(QStringLiteral("deadline")).toString().trimmed().isEmpty()) {
            return AIParseResult{false, {}, QObject::tr("AI 结果缺少任务标题或截止时间，已回退到本地规则解析"), QStringLiteral("deepseek")};
        }
        return AIParseResult{true, draft, QObject::tr("AI 已生成任务草稿，请确认后加入日程"), QStringLiteral("deepseek")};
    });
}

QFuture<AISuggestionResult> DeepSeekProvider::suggestScheduleChanges(const ScheduleContext& context)
{
    Q_UNUSED(context)
    return QtConcurrent::run([] {
        return AISuggestionResult{false, {}, QObject::tr("调度解释将在下一阶段接入"), QStringLiteral("deepseek")};
    });
}
