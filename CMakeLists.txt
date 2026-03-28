cmake_minimum_required(VERSION 3.16)

project(ArcMeta VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(/utf-8)
add_compile_options(/MP)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-DNOMINMAX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT Qt6_DIR AND NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Network Concurrent Svg)

# 禁用所有调试日志
add_definitions(-DQT_NO_DEBUG_OUTPUT)
add_definitions(-DQT_NO_INFO_OUTPUT)
add_definitions(-DQT_NO_WARNING_OUTPUT)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)

include_directories(src)

# 基于当前磁盘实际物理文件重建源代码列表
set(SOURCES
    src/main.cpp
    src/core/DatabaseManager.cpp
    src/core/DatabaseManager.h
    src/core/FileStorageHelper.h
    src/core/HttpServer.h
    src/core/MessageCaptureHandler.h
    src/core/ClipboardMonitor.h
    src/meta/AmMetaJson.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/HeaderBar.cpp
    src/ui/Editor.cpp
    src/ui/Editor.h
    src/ui/MetadataPanel.cpp
    src/ui/MetadataPanel.h
    src/ui/FilterPanel.cpp
    src/ui/FilterPanel.h
    src/ui/FileItemDelegate.cpp
    src/ui/StringUtils.h
    src/ui/ToolTipOverlay.h
    src/ui/HeaderBar.h
    src/ui/IconHelper.h
    src/ui/TagCapsule.h
    src/ui/FlowLayout.h
    src/ui/ClickableLineEdit.h
    resources/app.manifest
    SvgIcons.h
)

add_executable(ArcMeta ${SOURCES})

# 强制管理端宏定义
target_compile_definitions(ArcMeta PRIVATE RAPID_MANAGER_TARGET)

target_link_libraries(ArcMeta PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::Network Qt6::Concurrent Qt6::Svg
    ntdll ole32 bcrypt user32 shell32 psapi dwmapi windowsapp
)

if(WIN32)
    set_target_properties(ArcMeta PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
