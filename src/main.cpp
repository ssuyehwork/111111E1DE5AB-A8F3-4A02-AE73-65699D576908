#include <QApplication>
#include <QMessageBox>
#include <windows.h>
#include <shellapi.h>
#include "ui/MainWindow.h"
#include "db/Database.h"
#include "db/SyncEngine.h"
#include "meta/SyncQueue.h"
#include "mft/MftReader.h"
#include "core/CoreController.h"

/**
 * @brief 检查当前进程是否具有管理员权限
 */
bool isRunningAsAdmin() {
    BOOL fRet = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            fRet = elevation.TokenIsElevated;
        }
    }
    if (hToken) CloseHandle(hToken);
    return fRet;
}

int main(int argc, char *argv[]) {
    // 设置高 DPI 支持 (Qt 6 默认开启，此处显式设置)
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication a(argc, argv);
    a.setApplicationName("ArcMeta");
    a.setOrganizationName("ArcMetaTeam");

    // 1. 权限检查逻辑
    if (!isRunningAsAdmin()) {
        QMessageBox::critical(nullptr, "权限不足", 
            "ArcMeta 需要管理员权限以读取 MFT 数据及加速索引。\n请尝试“以管理员身份运行”。");
        // 文档规定：无权限时执行降级方案，但启动基础 UI 仍需进行
    }

    // 2. 初始化数据库 (仅核心表结构，必须同步完成)
    std::wstring dbPath = L"arcmeta.db";
    if (!ArcMeta::Database::instance().init(dbPath)) {
        QMessageBox::critical(nullptr, "错误", "无法初始化数据库，程序即将退出。");
        return -1;
    }

    // 3. 2026-03-xx 按照用户要求重构启动时效：
    // 初始化完毕后再打开窗口。先实例化窗口（此时默认隐藏），然后通过信号槽联动。
    ArcMeta::MainWindow w;

    // 核心握手连接：当后台初始化（MFT 扫描、元数据载入、增量同步）全部完成后，触发窗口显示
    QObject::connect(&ArcMeta::CoreController::instance(), &ArcMeta::CoreController::initializationFinished,
                     &w, &ArcMeta::MainWindow::show);

    // 启动异步初始化中控
    ArcMeta::CoreController::instance().startSystem();

    int ret = a.exec();

    // 6. 优雅退出：刷空队列并停止线程
    ArcMeta::SyncQueue::instance().stop();

    return ret;
}
