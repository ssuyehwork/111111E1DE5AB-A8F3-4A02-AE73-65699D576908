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
if(MSVC)
    add_compile_options(/W4 /O2 /utf-8)
endif()

# Qt 6 基础组件
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg Sql Concurrent)

# 2026-03-xx 按照用户要求：禁用 Vulkan 探测以消除日志报错，并清理构建路径矛盾
set(QT_NO_VULKAN ON)

# 路径严谨性核查：若检测到 VS 默认的 out 路径，主动输出警告提示
if(CMAKE_BINARY_DIR MATCHES "/out/")
    message(WARNING "⚠️ 检测到非法构建路径 [out]！请在 VS 工具栏切换至 [x64-release] 预设以符合《源码纯净》规范。")
endif()

# 2026-03-xx 按照用户要求：将可执行文件直接输出到指定项目子目录 ArcMeta 下，解决 \out 文件夹查找不便的问题
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/ArcMeta")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/ArcMeta")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/ArcMeta")

# 自动生成 MOC/UIC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 搜集源文件
# 2026-03-xx 按照用户要求：使用递归搜集源码，并包含资源文件与清单资源文件以确保编译完整性
# 2026-04-12 物理优化：回归标准 GLOB 模式，由 AUTOMOC 自动处理依赖，消除 LNK2019 外部符号冲突
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.h"
    "ArcMeta.rc"
    "resources/resources.qrc"
)

# 修复：将 .rc 资源文件加入源码列表，确保图标和清单正确嵌入
list(APPEND SOURCES 
    "ArcMeta.rc"
    "resources/resources.qrc"
)

# 可执行文件 (添加 WIN32 属性以消除控制台黑框)
# 修复：移除直接引用的 "ArcMeta.manifest"，改由 ArcMeta.rc 引入以杜绝 mt.exe 冲突
add_executable(ArcMeta WIN32 ${SOURCES})

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

# 2026-03-xx 按照用户要求及《记忆碎片》规范：
# 为避免 mt.exe 清单合并冲突，通过 ArcMeta.rc 嵌入清单并禁用链接器的默认清单生成
if(MSVC)
    target_link_options(ArcMeta PRIVATE "/MANIFEST:NO")
endif()
# Force Refresh CMake: 2026-05-24 20:00 (Trigger Re-scan after removing JSON logic)
