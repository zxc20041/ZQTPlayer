cmake_minimum_required(VERSION 3.16)

project(ZQTPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Quick QuickControls2 LinguistTools)

qt_standard_project_setup()

# Use ':/qt/qml/' as the default resource prefix so the engine can discover QML modules
qt_policy(SET QTP0001 NEW)

# Avoid clash between QML module dir "ZQTPlayer/" and the executable on Linux
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

qt_add_executable(ZQTPlayer
    main.cpp
    pages/HomeController.cpp
    pages/HomeController.h
    pages/SettingsController.cpp
    pages/SettingsController.h
    pages/StyleManager.cpp
    pages/StyleManager.h
    pages/LanguageManager.cpp
    pages/LanguageManager.h
    pages/ThemeManager.cpp
    pages/ThemeManager.h
)

qt_add_qml_module(ZQTPlayer
    URI ZQTPlayer
    VERSION 1.0
    QML_FILES
        Main.qml
        pages/Home.qml
        pages/Settings.qml
    SOURCES
        pages/HomeController.h
        pages/HomeController.cpp
        pages/SettingsController.h
        pages/SettingsController.cpp
        pages/StyleManager.h
        pages/StyleManager.cpp
        pages/LanguageManager.h
        pages/LanguageManager.cpp
        pages/ThemeManager.h
        pages/ThemeManager.cpp
)

set_target_properties(ZQTPlayer PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_link_libraries(ZQTPlayer
    PRIVATE Qt6::Quick Qt6::QuickControls2
)

target_include_directories(ZQTPlayer
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pages
)

# Compile .ts → .qm and embed as Qt resources (accessible via :/i18n/)
qt_add_translations(ZQTPlayer
    TS_FILES
        i18n/app_en_US.ts
        i18n/app_zh_CN.ts
)
