cmake_minimum_required(VERSION 3.20)

project(ArcMeta VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 强制要求 MSVC 2022
if(NOT MSVC)
    message(FATAL_ERROR "ArcMeta 只能由 Visual Studio 2022 (MSVC) 进行编译。")
endif()

# 设置 Qt 安装路径（根据 ArcMeta.md 要求）
set(CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64")

# 查找 Qt 组件
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent Svg)

# 编译选项：/W4 (四级警告), /O2 (速度优化)
add_compile_options(/W4 /O2 /utf-8)

# 链接库要求：ntdll, ole32, bcrypt.lib
set(EXTERNAL_LIBS ntdll ole32 bcrypt.lib)

# 启用自动特性
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 包含路径
include_directories(src)

# 收集源文件（目前仅包含基础骨架）
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.h"
)

# 资源文件（预留）
# set(RESOURCES resources/resources.qrc)

# 定义可执行文件
add_executable(ArcMeta WIN32 ${SOURCES})

# 链接库
target_link_libraries(ArcMeta PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Svg
    ${EXTERNAL_LIBS}
)

# 应用 Manifest 以获取管理员权限
set(MANIFEST_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/ArcMeta.manifest")
if(EXISTS ${MANIFEST_FILE})
    target_link_options(ArcMeta PRIVATE "/MANIFEST:EMBED" "/MANIFESTINPUT:${MANIFEST_FILE}")
endif()
