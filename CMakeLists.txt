cmake_minimum_required(VERSION 3.20)

project(ArcMeta VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 强制要求 MSVC 2022
if(NOT MSVC)
    message(FATAL_ERROR "ArcMeta 只能由 Visual Studio 2022 (MSVC) 进行编译。")
endif()

# 尝试自动查找 Qt 路径
# 如果未指定 CMAKE_PREFIX_PATH，用户可以通过 -DQT_DIR=... 传入，或设置环境变量
if(NOT CMAKE_PREFIX_PATH)
    set(QT_SEARCH_PATHS
        "C:/Qt/6.10.2/msvc2022_64"
        "C:/Qt/6.8.0/msvc2022_64"
        "C:/Qt/6.7.2/msvc2022_64"
        "D:/Qt/6.10.2/msvc2022_64"
        "$ENV{QT_DIR}"
    )
    foreach(PATH ${QT_SEARCH_PATHS})
        if(EXISTS "${PATH}")
            set(CMAKE_PREFIX_PATH "${PATH}")
            break()
        endif()
    endforeach()
endif()

# 查找 Qt 组件
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent Svg Sql)

# 编译选项：/W4 (四级警告), /O2 (速度优化), /utf-8 (统一编码)
add_compile_options(/W4 /O2 /utf-8)

# 链接库要求：ntdll, ole32, bcrypt.lib, shell32.lib
set(EXTERNAL_LIBS ntdll ole32 bcrypt.lib shell32.lib)

# 启用自动特性
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 包含路径
include_directories(src)

# 收集源文件
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.h"
)

# 定义可执行文件
add_executable(ArcMeta WIN32 ${SOURCES})

# 链接库
target_link_libraries(ArcMeta PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Svg
    Qt6::Sql
    ${EXTERNAL_LIBS}
)

# 应用 Manifest 以获取管理员权限（如果存在）
set(MANIFEST_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/ArcMeta.manifest")
if(EXISTS ${MANIFEST_FILE})
    target_link_options(ArcMeta PRIVATE "/MANIFEST:EMBED" "/MANIFESTINPUT:${MANIFEST_FILE}")
endif()

# 安装配置说明（输出到控制台）
message(STATUS "-----------------------------------------------------------")
message(STATUS "ArcMeta 构建配置已生成。")
message(STATUS "使用的 Qt 路径: ${CMAKE_PREFIX_PATH}")
message(STATUS "请确保安装了 MSVC 2022 且 Qt 版本为 6.x。")
message(STATUS "-----------------------------------------------------------")
