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

# 搜集源文件
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.h"
)
# 修复：显式包含新添加的文件，防止 CMake 缓存导致 GLOB 滞后
list(APPEND SOURCES 
    "src/ui/BreadcrumbBar.h"
    "src/ui/BreadcrumbBar.cpp"
    "src/ui/CategoryModel.cpp"
    "src/ui/CategoryModel.h"
    "src/ui/CategoryDelegate.h"
    "src/ui/DropTreeView.cpp"
    "src/ui/DropTreeView.h"
    "src/core/CoreController.h"
    "src/core/CoreController.cpp"
    "src/meta/MetadataManager.h"
    "src/meta/MetadataManager.cpp"
    "src/ui/FramelessDialog.h"
    "src/ui/FramelessDialog.cpp"
    "src/ui/DropListView.h"
    "src/ui/DropListView.cpp"
    "src/ui/TreeItemDelegate.h"
    "src/ui/CategoryLockDialog.h"
    "src/ui/CategoryLockDialog.cpp"
    "src/ui/CategorySetPasswordDialog.h"
    "src/ui/CategorySetPasswordDialog.cpp"
)

# 可执行文件 (添加 WIN32 属性以消除控制台黑框)
# 修复：使用绝对路径显式引用 Manifest，防止嵌入工具定位失败
add_executable(ArcMeta WIN32 ${SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/ArcMeta.manifest" "resources/resources.qrc")

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

# 声明管理员权限 Manifest (现代 CMake 方式通过直接将 ArcMeta.manifest 加入 add_executable 处理)
