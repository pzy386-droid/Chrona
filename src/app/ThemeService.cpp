#include "app/ThemeService.h"

#include <utility>

ThemeService::ThemeService(SettingsRepository settings, QObject* parent)
    : QObject(parent)
    , m_settings(std::move(settings))
{
}

bool ThemeService::dark() const
{
    return m_dark;
}

void ThemeService::load()
{
    setDark(m_settings.value(QStringLiteral("theme"), QStringLiteral("dark")) != QStringLiteral("light"));
}

void ThemeService::setDark(bool dark)
{
    if (m_dark == dark) {
        return;
    }
    m_dark = dark;
    m_settings.setValue(QStringLiteral("theme"), dark ? QStringLiteral("dark") : QStringLiteral("light"));
    emit darkChanged();
}

void ThemeService::toggle()
{
    setDark(!m_dark);
}
