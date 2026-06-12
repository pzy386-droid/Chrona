#include "app/AppController.h"

#include "ai/providers/DeepSeekProvider.h"
#include "database/Migrations.h"

#include <QProcessEnvironment>
#include <QQmlApplicationEngine>
#include <QQmlContext>

bool AppController::initialize(QQmlApplicationEngine* engine)
{
    if (!m_database.open()) {
        return false;
    }
    QSqlDatabase db = m_database.connection();
    if (!Migrations::run(db)) {
        return false;
    }
    SettingsRepository settings(db);
    if (settings.value(QStringLiteral("demoModeEnabled"), QStringLiteral("false")) == QStringLiteral("true") && !Migrations::seed(db)) {
        return false;
    }

    m_localeService = std::make_unique<LocaleService>(SettingsRepository(db));
    const QString storedDeepSeekKey = settings.value(QStringLiteral("deepseekApiKey"));
    const QString deepSeekKey = storedDeepSeekKey.trimmed().isEmpty()
        ? QProcessEnvironment::systemEnvironment().value(QStringLiteral("DEEPSEEK_API_KEY"))
        : storedDeepSeekKey;
    m_aiService = std::make_unique<DeepSeekProvider>(deepSeekKey);
    m_scheduleService = std::make_unique<ScheduleService>(
        TaskRepository(db), CalendarRepository(db), TimeBlockRepository(db),
        StudyFrameRepository(db), SettingsRepository(db), BackupService(db), m_aiService.get());

    engine->rootContext()->setContextProperty(QStringLiteral("LocaleService"), m_localeService.get());
    engine->rootContext()->setContextProperty(QStringLiteral("ScheduleService"), m_scheduleService.get());

    QObject::connect(m_localeService.get(), &LocaleService::localeChanged, engine, &QQmlApplicationEngine::retranslate);

    m_localeService->load();
    m_scheduleService->reschedule();
    return true;
}
