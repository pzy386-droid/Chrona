#pragma once

#include "database/Repositories.h"

#include <QObject>
#include <QTranslator>

class LocaleService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString locale READ locale NOTIFY localeChanged)

public:
    explicit LocaleService(SettingsRepository settings, QObject* parent = nullptr);

    QString locale() const;
    Q_INVOKABLE void setLocale(const QString& locale);
    void load();

signals:
    void localeChanged();

private:
    bool installTranslator(const QString& locale);

    SettingsRepository m_settings;
    QString m_locale = QStringLiteral("zh_CN");
    QTranslator m_translator;
};
