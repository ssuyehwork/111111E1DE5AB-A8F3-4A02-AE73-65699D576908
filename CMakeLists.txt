cmake_minimum_required(VERSION 3.20)

project(ArcMeta VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2026-03-27 按照宪法：强制锁定 MSVC 2022 编译环境
if(NOT MSVC)
    message(FATAL_ERROR "ArcMeta 仅支持使用 Visual Studio 2022 (MSVC) 构建。")
endif()

add_compile_options(/W4 /O2 /utf-8 /MP)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Svg SvgWidgets)

set(SOURCES
    src/mft/MftReader.cpp
    src/mft/MftReader.h
    src/ui/IconHelper.cpp
    src/ui/IconHelper.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/ContentPanel.cpp
    src/ui/ContentPanel.h
    src/ui/NavPanel.cpp
    src/ui/NavPanel.h
    src/main.cpp
)

set(LIBS
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Svg
    Qt6::SvgWidgets
    ntdll.lib
    ole32.lib
    bcrypt.lib
    shell32.lib
    user32.lib
)

add_executable(ArcMeta WIN32 ${SOURCES})
target_link_libraries(ArcMeta PRIVATE ${LIBS})
target_include_directories(ArcMeta PRIVATE src)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
