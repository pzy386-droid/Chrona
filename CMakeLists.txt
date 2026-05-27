cmake_minimum_required(VERSION 3.21)

project(Chrona VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Quick Sql Concurrent LinguistTools)

qt_standard_project_setup(REQUIRES 6.6)

qt_add_executable(Chrona
    src/main.cpp
    src/app/AppController.cpp
    src/app/AppController.h
    src/app/LocaleService.cpp
    src/app/LocaleService.h
    src/app/ScheduleService.cpp
    src/app/ScheduleService.h
    src/app/TimelineModel.cpp
    src/app/TimelineModel.h
    src/app/TaskListModel.cpp
    src/app/TaskListModel.h
    src/core/models/ScheduleTypes.h
    src/core/models/Task.h
    src/core/models/CalendarEvent.h
    src/core/models/TimeBlock.cpp
    src/core/models/TimeBlock.h
    src/core/scheduler/CapacityAnalyzer.cpp
    src/core/scheduler/CapacityAnalyzer.h
    src/core/scheduler/ConflictResolver.cpp
    src/core/scheduler/ConflictResolver.h
    src/core/scheduler/PriorityEvaluator.cpp
    src/core/scheduler/PriorityEvaluator.h
    src/core/scheduler/TaskScheduler.cpp
    src/core/scheduler/TaskScheduler.h
    src/core/scheduler/TimeBlockGenerator.cpp
    src/core/scheduler/TimeBlockGenerator.h
    src/database/Database.cpp
    src/database/Database.h
    src/database/Migrations.cpp
    src/database/Migrations.h
    src/database/Repositories.cpp
    src/database/Repositories.h
    src/managers/NLPTaskParser.cpp
    src/managers/NLPTaskParser.h
    src/managers/OCRManager.cpp
    src/managers/OCRManager.h
    src/managers/Scheduler.cpp
    src/managers/Scheduler.h
    src/ai/AIService.h
    src/ai/NoopAIService.cpp
    src/ai/NoopAIService.h
    src/ai/providers/DeepSeekProvider.h
)

qt_add_qml_module(Chrona
    URI Chrona
    VERSION 1.0
    QML_FILES
        src/ui/qml/Main.qml
        src/ui/qml/pages/CalendarPage.qml
        src/ui/qml/pages/TimelinePage.qml
        src/ui/qml/panels/Sidebar.qml
        src/ui/qml/panels/TaskDetailPanel.qml
        src/ui/qml/panels/CurrentFocusPanel.qml
        src/ui/qml/components/IconButton.qml
        src/ui/qml/components/LanguageToggle.qml
        src/ui/qml/components/PriorityPill.qml
        src/ui/qml/components/QuickAddBar.qml
        src/ui/qml/components/SegmentedControl.qml
        src/ui/qml/components/TimeBlock.qml
        src/ui/qml/components/TimelineView.qml
)

qt_add_translations(Chrona
    TS_FILES
        src/i18n/chrona_en_US.ts
        src/i18n/chrona_zh_CN.ts
    RESOURCE_PREFIX "/i18n"
)

target_include_directories(Chrona PRIVATE src)

target_link_libraries(Chrona PRIVATE
    Qt6::Quick
    Qt6::Sql
    Qt6::Concurrent
)

if(WIN32)
    set_target_properties(Chrona PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
