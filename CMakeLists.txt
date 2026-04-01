cmake_minimum_required(VERSION 3.20)
project(ArcMeta)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 优先探测 QT_DIR 环境变量
if(DEFINED ENV{QT_DIR})
    set(CMAKE_PREFIX_PATH "$ENV{QT_DIR}")
else()
    set(CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64")
endif()

# 强制 MSVC 编译器及选项
if(MSVC)
    add_compile_options(/W4 /O2 /utf-8)
endif()

# Qt 6 基础组件
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg Sql Concurrent)

# 自动生成 MOC/UIC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

# 显式声明所有源文件 (物理还原后的 RapidNotes 完整架构)
set(SOURCES
    # Core & Crypto
    "src/core/AES.h"
    "src/core/AES.cpp"
    "src/core/ClipboardMonitor.h"
    "src/core/ClipboardMonitor.cpp"
    "src/core/CoreController.h"
    "src/core/CoreController.cpp"
    "src/core/DatabaseManager.h"
    "src/core/DatabaseManager.cpp"
    "src/core/FileCryptoHelper.h"
    "src/core/FileCryptoHelper.cpp"
    "src/core/HardwareInfoHelper.h"
    "src/core/HardwareInfoHelper.cpp"

    # DB & Meta
    "src/db/Database.h"
    "src/db/Database.cpp"
    "src/db/CategoryRepo.h"
    "src/db/CategoryRepo.cpp"
    "src/db/FavoritesRepo.h"
    "src/db/FavoritesRepo.cpp"
    "src/db/FolderRepo.h"
    "src/db/FolderRepo.cpp"
    "src/db/ItemRepo.h"
    "src/db/ItemRepo.cpp"
    "src/db/SyncEngine.h"
    "src/db/SyncEngine.cpp"
    "src/meta/AmMetaJson.h"
    "src/meta/AmMetaJson.cpp"
    "src/meta/BatchRenameEngine.h"
    "src/meta/BatchRenameEngine.cpp"
    "src/meta/SyncQueue.h"
    "src/meta/SyncQueue.cpp"

    # UI Components
    "src/ui/AdvancedTagSelector.h"
    "src/ui/AdvancedTagSelector.cpp"
    "src/ui/BreadcrumbBar.h"
    "src/ui/BreadcrumbBar.cpp"
    "src/ui/CategoryDelegate.h"
    "src/ui/CategoryModel.h"
    "src/ui/CategoryModel.cpp"
    "src/ui/CategoryPanel.h"
    "src/ui/CategoryPanel.cpp"
    "src/ui/ContentPanel.h"
    "src/ui/ContentPanel.cpp"
    "src/ui/DropTreeView.h"
    "src/ui/DropTreeView.cpp"
    "src/ui/FilterPanel.h"
    "src/ui/FilterPanel.cpp"
    "src/ui/FlowLayout.h"
    "src/ui/FlowLayout.cpp"
    "src/ui/FramelessDialog.h"
    "src/ui/FramelessDialog.cpp"
    "src/ui/IconHelper.h"
    "src/ui/MainWindow.h"
    "src/ui/MainWindow.cpp"
    "src/ui/MetaPanel.h"
    "src/ui/MetaPanel.cpp"
    "src/ui/NavPanel.h"
    "src/ui/NavPanel.cpp"
    "src/ui/QuickLookWindow.h"
    "src/ui/QuickLookWindow.cpp"
    "src/ui/StringUtils.h"
    "src/ui/SvgIcons.h"
    "src/ui/ToolTipOverlay.h"
    "src/ui/ToolTipOverlay.cpp"
    "src/ui/UiHelper.h"

    # Entry Point
    "src/main.cpp"
)

# 可执行文件
add_executable(ArcMeta WIN32 ${SOURCES} "ArcMeta.manifest")

# 链接库
target_link_libraries(ArcMeta PRIVATE
    Qt6::Widgets
    Qt6::Svg
    Qt6::Sql
    Qt6::Concurrent
    ntdll
    ole32
    bcrypt
)
