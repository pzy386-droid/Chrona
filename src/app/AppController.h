#pragma once

#include "app/LocaleService.h"
#include "app/ThemeService.h"
#include "app/ScheduleService.h"
#include "ai/AIService.h"
#include "database/Database.h"

#include <memory>

class QQmlApplicationEngine;

class AppController {
public:
    bool initialize(QQmlApplicationEngine* engine);

private:
    Database m_database;
    std::unique_ptr<LocaleService> m_localeService;
    std::unique_ptr<ThemeService> m_themeService;
    std::unique_ptr<ScheduleService> m_scheduleService;
    std::unique_ptr<AIService> m_aiService;
};
