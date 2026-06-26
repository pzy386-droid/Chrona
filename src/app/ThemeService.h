#pragma once

#include "database/Repositories.h"

#include <QObject>

class ThemeService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY darkChanged)

public:
    explicit ThemeService(SettingsRepository settings, QObject* parent = nullptr);

    bool dark() const;
    Q_INVOKABLE void setDark(bool dark);
    Q_INVOKABLE void toggle();
    void load();

signals:
    void darkChanged();

private:
    SettingsRepository m_settings;
    bool m_dark = true;
};
