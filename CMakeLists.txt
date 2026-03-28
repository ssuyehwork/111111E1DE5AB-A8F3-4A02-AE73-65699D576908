cmake_minimum_required(VERSION 3.20)
project(ArcMeta)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 强制 MSVC 编译器及选项 (文档红线: /W4 /O2)
if(MSVC)
    add_compile_options(/W4 /O2 /utf-8)
endif()

# Qt 6 基础组件
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg Sql)

# 自动生成 MOC/UIC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

# 搜集源文件
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.h"
)

# 可执行文件
add_executable(ArcMeta ${SOURCES})

# 链接库 (文档红线列表: ntdll, ole32, bcrypt + QtSql)
target_link_libraries(ArcMeta PRIVATE
    Qt6::Widgets
    Qt6::Svg
    Qt6::Sql
    ntdll
    ole32
    bcrypt
)

# 声明管理员权限 Manifest
if(MSVC)
    set(MANIFEST_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ArcMeta.manifest")
    set_target_properties(ArcMeta PROPERTIES LINK_FLAGS "/MANIFEST:EMBED /MANIFESTINPUT:\"${MANIFEST_FILE}\"")
endif()
