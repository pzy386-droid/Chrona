cmake_minimum_required(VERSION 3.21)

project(Chrona VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Quick Sql Concurrent Network LinguistTools)

qt_standard_project_setup(REQUIRES 6.6)

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(Chrona
    src/main.cpp
    src/app/AppController.cpp
    src/app/AppController.h
    src/app/LocaleService.cpp
    src/app/LocaleService.h
    src/app/ThemeService.cpp
    src/app/ThemeService.h
    src/app/ScheduleService.cpp
    src/app/ScheduleService.h
    src/app/TimelineModel.cpp
    src/app/TimelineModel.h
    src/app/TaskListModel.cpp
    src/app/TaskListModel.h
    src/core/models/ScheduleTypes.h
    src/core/models/StudyFrame.h
    src/core/models/Task.h
    src/core/models/DeadlineReminder.h
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
    src/database/BackupService.cpp
    src/database/BackupService.h
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
    src/ai/providers/DeepSeekProvider.cpp
    src/ai/providers/DeepSeekProvider.h
)

set_source_files_properties(src/ui/qml/components/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(Chrona
    URI Chrona
    VERSION 1.0
    QML_FILES
        src/ui/qml/Main.qml
        src/ui/qml/pages/CalendarPage.qml
        src/ui/qml/pages/TimelinePage.qml
        src/ui/qml/pages/MonthPage.qml
        src/ui/qml/pages/DeadlinePage.qml
        src/ui/qml/pages/LoginPage.qml
        src/ui/qml/panels/Sidebar.qml
        src/ui/qml/panels/TaskDetailPanel.qml
        src/ui/qml/panels/CurrentFocusPanel.qml
        src/ui/qml/components/IconButton.qml
        src/ui/qml/components/LanguageToggle.qml
        src/ui/qml/components/Theme.qml
        src/ui/qml/components/ThemeToggle.qml
        src/ui/qml/components/PriorityPill.qml
        src/ui/qml/components/QuickAddBar.qml
        src/ui/qml/components/SegmentedControl.qml
        src/ui/qml/components/TimeBlock.qml
        src/ui/qml/components/MonthWheelPicker.qml
        src/ui/qml/components/TimeWheelPicker.qml
        src/ui/qml/components/TimelineView.qml
        src/ui/qml/panels/DailyPlanOverlay.qml
        src/ui/qml/panels/EveningReviewOverlay.qml
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
    Qt6::Network
)

if(WIN32)
    set_target_properties(Chrona PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)
    qt_add_executable(ChronaDatabaseTests
        tests/DatabaseConsistencyTests.cpp
        src/database/BackupService.cpp
        src/database/BackupService.h
        src/database/Migrations.cpp
        src/database/Migrations.h
        src/database/Repositories.cpp
        src/database/Repositories.h
    )

    target_include_directories(ChronaDatabaseTests PRIVATE src)
    target_link_libraries(ChronaDatabaseTests PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
    add_test(NAME ChronaDatabaseTests COMMAND ChronaDatabaseTests)

    qt_add_executable(ChronaNLPTests
        tests/NLPTaskParserTests.cpp
        src/managers/NLPTaskParser.cpp
        src/managers/NLPTaskParser.h
    )
    target_include_directories(ChronaNLPTests PRIVATE src)
    target_link_libraries(ChronaNLPTests PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME ChronaNLPTests COMMAND ChronaNLPTests)

    qt_add_executable(TaskSchedulerTest
        tests/TaskSchedulerTest.cpp
        src/core/scheduler/CapacityAnalyzer.cpp
        src/core/scheduler/CapacityAnalyzer.h
        src/core/scheduler/PriorityEvaluator.cpp
        src/core/scheduler/PriorityEvaluator.h
        src/core/scheduler/TaskScheduler.cpp
        src/core/scheduler/TaskScheduler.h
        src/core/scheduler/TimeBlockGenerator.cpp
        src/core/scheduler/TimeBlockGenerator.h
    )
    target_include_directories(TaskSchedulerTest PRIVATE src)
    target_link_libraries(TaskSchedulerTest PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME TaskSchedulerTest COMMAND TaskSchedulerTest)
endif()
