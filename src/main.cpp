#include <QApplication>
#include <QMessageBox>
#include <windows.h>
#include <shellapi.h>
#include <QThreadPool>
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
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication a(argc, argv);
    a.setApplicationName("ArcMeta");
    a.setOrganizationName("ArcMetaTeam");

    if (!isRunningAsAdmin()) {
        QMessageBox::critical(nullptr, "权限不足", 
            "ArcMeta 需要管理员权限以读取 MFT 数据及加速索引。\n请尝试“以管理员身份运行”。");
    }

    std::wstring dbPath = L"arcmeta.db";
    if (!ArcMeta::Database::instance().init(dbPath)) {
        QMessageBox::critical(nullptr, "错误", "无法初始化数据库，程序即将退出。");
        return -1;
    }

    // 启动异步初始化中控
    ArcMeta::CoreController::instance().startSystem();

    ArcMeta::MainWindow w;
    w.show();

    int ret = a.exec();

    // 2026-03-xx 架构修复：优雅退出逻辑
    // 1. 停止同步队列
    ArcMeta::SyncQueue::instance().stop();
    // 2. 强制等待并清空全局线程池中的异步图标加载任务，防止退出时访问已销毁的对象
    QThreadPool::globalInstance()->waitForDone();

    return ret;
}
