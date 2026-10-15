cmake_minimum_required(VERSION 3.16)
project(KanDu VERSION 1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(KanDu WIN32
    src/main.cpp
    src/icons.cpp
    src/settings.cpp
    src/imageview.cpp
    src/titlebar.cpp
    src/mainwindow.cpp
    src/settingsdialog.cpp
    src/aboutdialog.cpp
    src/defaultapp.cpp
    src/wicdecode.cpp
    src/singleton.cpp
    resources/app.qrc
    resources/app.rc
)

target_include_directories(KanDu PRIVATE src)
target_link_libraries(KanDu PRIVATE Qt6::Widgets ole32 windowscodecs)
set_target_properties(KanDu PROPERTIES WIN32_EXECUTABLE TRUE)

# 自动化自测（含测试钩子，独立于正式版）
find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
add_executable(kandu_selftest
    tests/selftest.cpp
    src/icons.cpp
    src/settings.cpp
    src/imageview.cpp
    src/titlebar.cpp
    src/mainwindow.cpp
    src/settingsdialog.cpp
    src/aboutdialog.cpp
    src/defaultapp.cpp
    src/wicdecode.cpp
    src/singleton.cpp
    resources/app.qrc
)
target_include_directories(kandu_selftest PRIVATE src)
target_compile_definitions(kandu_selftest PRIVATE KANDU_SELFTEST)
target_link_libraries(kandu_selftest PRIVATE Qt6::Widgets Qt6::Test ole32 windowscodecs)
