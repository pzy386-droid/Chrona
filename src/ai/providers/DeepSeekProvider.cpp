#include "ai/providers/DeepSeekProvider.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
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
            *error = QObject::tr("Chrona AI 请求超时，已回退到本地规则");
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
            *error = QObject::tr("Chrona AI 请求失败：%1").arg(networkErrorText);
        }
        return {};
    }

    const QJsonObject response = QJsonDocument::fromJson(body).object();
    const QJsonArray choices = response.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        if (error) {
            *error = QObject::tr("Chrona AI 返回为空，已回退到本地规则");
        }
        return {};
    }

    const QString content = choices.first().toObject()
        .value(QStringLiteral("message")).toObject()
        .value(QStringLiteral("content")).toString();
    const QJsonDocument contentDoc = QJsonDocument::fromJson(content.toUtf8());
    if (!contentDoc.isObject()) {
        if (error) {
            *error = QObject::tr("Chrona AI 未返回有效 JSON，已回退到本地规则");
        }
        return {};
    }
    return contentDoc.object();
}

QVariantMap normalizedTaskDraft(const QJsonObject& object)
{
    const QJsonValue priorityValue = object.value(QStringLiteral("priority"));
    int priority = 1;
    if (priorityValue.isDouble()) {
        priority = priorityValue.toInt(1);
    } else {
        const QString priorityText = priorityValue.toString().trimmed().toLower();
        if (priorityText == QStringLiteral("high") || priorityText == QStringLiteral("高")) {
            priority = 2;
        } else if (priorityText == QStringLiteral("low") || priorityText == QStringLiteral("低")) {
            priority = 0;
        }
    }
    priority = qBound(0, priority, 2);

    const int estimatedMinutes = qBound(30, object.value(QStringLiteral("estimatedMinutes")).toInt(60), 1440);
    QString preferred = object.value(QStringLiteral("preferredStudyTime")).toString(QStringLiteral("evening")).trimmed().toLower();
    if (preferred != QStringLiteral("morning") && preferred != QStringLiteral("afternoon")
        && preferred != QStringLiteral("evening") && preferred != QStringLiteral("any")) {
        preferred = QStringLiteral("evening");
    }

    const bool hasTimeAnchor = object.value(QStringLiteral("hasTimeAnchor")).toBool(false);
    const QString scheduledStart = object.value(QStringLiteral("scheduledStart")).toString();
    const QString scheduledEnd = object.value(QStringLiteral("scheduledEnd")).toString();
    const QString planningMode = object.value(QStringLiteral("planningMode")).toString(
        hasTimeAnchor ? QStringLiteral("direct_time_block") : QStringLiteral("task_deadline"));

    return {
        {QStringLiteral("title"), object.value(QStringLiteral("title")).toString().trimmed()},
        {QStringLiteral("notes"), object.value(QStringLiteral("notes")).toString(QObject::tr("由 Chrona AI 解析创建"))},
        {QStringLiteral("deadline"), object.value(QStringLiteral("deadline")).toString().trimmed()},
        {QStringLiteral("estimatedMinutes"), estimatedMinutes},
        {QStringLiteral("priority"), priority},
        {QStringLiteral("categoryName"), object.value(QStringLiteral("categoryName")).toString(QObject::tr("学习"))},
        {QStringLiteral("preferredStudyTime"), preferred},
        {QStringLiteral("hasTimeAnchor"), hasTimeAnchor},
        {QStringLiteral("scheduledStart"), scheduledStart},
        {QStringLiteral("scheduledEnd"), scheduledEnd},
        {QStringLiteral("planningMode"), planningMode},
        {QStringLiteral("confidence"), object.value(QStringLiteral("confidence")).toDouble(0.0)},
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

QString DeepSeekProvider::providerName() const
{
    return QStringLiteral("deepseek");
}

bool DeepSeekProvider::isConfigured() const
{
    return !m_apiKey.trimmed().isEmpty();
}

bool DeepSeekProvider::setApiKey(const QString& apiKey)
{
    m_apiKey = apiKey.trimmed();
    return true;
}

QFuture<AIParseResult> DeepSeekProvider::parseNaturalLanguageTask(const QString& input)
{
    const QString apiKey = m_apiKey;
    const QString endpoint = m_endpoint;
    const QString model = m_model;
    return QtConcurrent::run([apiKey, endpoint, model, input] {
        if (apiKey.trimmed().isEmpty()) {
            return AIParseResult{false, {}, QObject::tr("尚未连接 Chrona AI，已使用本地规则解析"), QStringLiteral("deepseek")};
        }

        const QString systemPrompt = QStringLiteral(
            "You are Chrona AI, the natural-language planner inside an AI-native study scheduling desktop app. "
            "The user message is a compact JSON object with currentLocalDateTime and input. "
            "You must understand Chinese and English natural language semantically, not by keyword matching. "
            "Return strict JSON only, no markdown. Required keys: "
            "title:string, notes:string, deadline:string, estimatedMinutes:number, priority:number, "
            "categoryName:string, preferredStudyTime:string, hasTimeAnchor:boolean, scheduledStart:string, "
            "scheduledEnd:string, planningMode:string, confidence:number, explanation:string. "
            "deadline, scheduledStart and scheduledEnd must be local time in format yyyy-MM-dd HH:mm. "
            "planningMode must be one of: direct_time_block, task_deadline, schedule_suggestion. "
            "If the user intends to do something at a natural time, set planningMode=direct_time_block, "
            "hasTimeAnchor=true, scheduledStart to your best reasonable anchor, scheduledEnd=scheduledStart+estimatedMinutes, "
            "and deadline=scheduledEnd. If the user only gives a due date or submission deadline, set "
            "planningMode=task_deadline, hasTimeAnchor=false, scheduledStart='', scheduledEnd=''. "
            "Chinese time rules: 明天 means current date + 1 day; 后天 means +2 days; 下周三 means the next Wednesday; "
            "中午 means 12:00; 下午两点 means 14:00; 晚上 means evening; 上午 means morning. "
            "Examples: '明天中午吃饭' => direct_time_block, start tomorrow 12:00, duration 60 minutes, categoryName='生活'. "
            "'下周三交数据库实验报告' => task_deadline, deadline next Wednesday 23:59, no direct time block. "
            "'后天早上早读' => direct_time_block, start current date + 2 days at 08:00, duration 60 minutes. "
            "If the input only says noon/afternoon/evening without a minute, infer a reasonable anchor time: "
            "noon=12:00, afternoon=14:00, evening=20:00, morning=08:00. "
            "priority must be an integer only: 0 low, 1 medium, or 2 high. "
            "preferredStudyTime must be morning, afternoon, evening, or any. "
            "Do not create multiple tasks unless the input explicitly contains multiple independent tasks. "
            "Do not claim database writes.");

        const QJsonObject userPayload{
            {QStringLiteral("currentLocalDateTime"), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))},
            {QStringLiteral("input"), input}
        };
        const QString userPrompt = QString::fromUtf8(QJsonDocument(userPayload).toJson(QJsonDocument::Compact));

        QString error;
        const QJsonObject object = callDeepSeekJson(endpoint, model, apiKey, systemPrompt, userPrompt, &error);
        if (object.isEmpty()) {
            return AIParseResult{false, {}, error, QStringLiteral("deepseek")};
        }
        const QVariantList drafts = normalizedTaskDrafts(object);
        if (drafts.isEmpty()) {
            return AIParseResult{false, {}, QObject::tr("Chrona AI 结果缺少任务标题或截止时间，已使用本地规则解析"), QStringLiteral("deepseek")};
        }

        const QString message = object.value(QStringLiteral("message")).toString(
            QObject::tr("Chrona AI 已生成任务草稿，请确认后加入日程"));
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
            return AISuggestionResult{false, {}, QObject::tr("未配置 Chrona AI Key，使用本地建议"), QStringLiteral("deepseek")};
        }

        const QString systemPrompt = QStringLiteral(
            "You are Chrona AI, a concise study schedule advisor. The deterministic local scheduler is authoritative. "
            "Never claim to have changed data. Return strict JSON only with keys title, summary, riskLevel, currentFocus, "
            "reasons, suggestions, providerLabel. Keep all Chinese copy short and product-like. reasons is a string array "
            "with at most 3 items. suggestions is an array with at most 3 items; each item has title, description, impact, "
            "actionType, taskId, proposedDeadline. Allowed actionType values are reschedule, extend_deadline, review_task, focus_buffer. "
            "For effortLevel=2, explain why a one-hour buffer before and after the selected block reduces context switching. "
            "If deadlineType=1 or a block is locked, treat it as immovable and recommend a different time instead of moving it. "
            "Use only supplied facts; do not invent tasks, events, or times. No markdown.");
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
            return AISuggestionResult{false, {}, QObject::tr("Chrona AI 建议缺少摘要，使用本地建议"), QStringLiteral("deepseek")};
        }
        suggestion.insert(QStringLiteral("provider"), QStringLiteral("deepseek"));
        suggestion.insert(QStringLiteral("model"), model);
        return AISuggestionResult{true, suggestion, summary, QStringLiteral("deepseek")};
    });
}
