cmake_minimum_required(VERSION 3.16)

project(ZQTPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Quick)

qt_standard_project_setup()

qt_add_executable(ZQTPlayer
    main.cpp
    pages/HomeController.cpp
    pages/HomeController.h
    pages/SettingsController.cpp
    pages/SettingsController.h
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
)

set_target_properties(ZQTPlayer PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_link_libraries(ZQTPlayer
    PRIVATE Qt6::Quick
)

target_include_directories(ZQTPlayer
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pages
)
