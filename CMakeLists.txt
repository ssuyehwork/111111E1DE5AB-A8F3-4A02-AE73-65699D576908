cmake_minimum_required(VERSION 3.20)
project(ArcMeta)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 修复：优先探测 QT_DIR 环境变量，若无则使用文档红线默认路径
if(DEFINED ENV{QT_DIR})
    set(CMAKE_PREFIX_PATH "$ENV{QT_DIR}")
else()
    set(CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64")
endif()

# 强制 MSVC 编译器及选项 (文档红线: /W4 /O2)
# 修复：/O2 与 /RTC1 (Debug 默认) 冲突，改用生成器表达式
if(MSVC)
    add_compile_options(/W4 /utf-8)
    add_compile_options("$<$<NOT:$<CONFIG:Debug>>:/O2>")
endif()

# Qt 6 基础组件
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg Sql Concurrent)

# 优化输出目录，按照用户要求生成到项目根目录下的 bin 文件夹中 (修复 mt.exe 文件夹命名冲突)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# 自动生成 MOC/UIC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

# 搜集源文件 (GLOB_RECURSE 已包含 src 目录下所有 cpp/h，无需重复手动列举)
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.h"
)

# 可执行文件 (添加 WIN32 属性以消除控制台黑框)
# 修复：清单文件从源码列表移除，改由下方的 target_link_options 嵌入以彻底解决 mt.exe 冲突
add_executable(ArcMeta WIN32 ${SOURCES} "resources/resources.qrc")

# 链接库 (文档红线列表: ntdll, ole32, bcrypt + QtSql)
target_link_libraries(ArcMeta PRIVATE
    Qt6::Widgets
    Qt6::Svg
    Qt6::Sql
    Qt6::Concurrent
    ntdll
    ole32
    bcrypt
)

# --- 深度修复 mt.exe 报错与路径冲突逻辑 ---

# 1. 禁用增量链接：解决 Release/RelWithDebInfo 模式下 mt.exe 合并 ilk 导致的锁定错误
set_target_properties(ArcMeta PROPERTIES LINK_FLAGS "/INCREMENTAL:NO")

# 2. 强制定向输出：不仅是运行时，PDB 等调试符号也一并重定向至 bin，防止路径残留
set_target_properties(ArcMeta PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    PDB_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

# 3. 链接器直接嵌入清单：通过链接期参数嵌入 ArcMeta.manifest，彻底跳过不稳定的 mt.exe 独立步骤
if(MSVC)
    target_link_options(ArcMeta PRIVATE "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/ArcMeta.manifest")
endif()
