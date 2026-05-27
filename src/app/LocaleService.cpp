#include "app/LocaleService.h"

#include <QCoreApplication>
#include <utility>

LocaleService::LocaleService(SettingsRepository settings, QObject* parent)
    : QObject(parent)
    , m_settings(std::move(settings))
{
}

QString LocaleService::locale() const
{
    return m_locale;
}

void LocaleService::load()
{
    setLocale(m_settings.value(QStringLiteral("locale"), QStringLiteral("zh_CN")));
}

void LocaleService::setLocale(const QString& locale)
{
    const QString normalized = locale == QStringLiteral("en_US") ? QStringLiteral("en_US") : QStringLiteral("zh_CN");
    if (m_locale == normalized) {
        return;
    }

    installTranslator(normalized);
    m_locale = normalized;
    m_settings.setValue(QStringLiteral("locale"), normalized);
    emit localeChanged();
}

bool LocaleService::installTranslator(const QString& locale)
{
    QCoreApplication::removeTranslator(&m_translator);
    if (locale == QStringLiteral("zh_CN")) {
        return true;
    }

    if (m_translator.load(QStringLiteral(":/i18n/chrona_%1.qm").arg(locale))) {
        QCoreApplication::installTranslator(&m_translator);
        return true;
    }
    return false;
}
