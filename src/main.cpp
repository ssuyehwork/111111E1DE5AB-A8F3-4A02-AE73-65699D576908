#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include "db/Database.h"
#include "meta/AmMetaJson.h"
#include "ui/MainWindow.h"
#include "mft/MftReader.h"
#include "mft/UsnWatcher.h"
#include "meta/SyncQueue.h"
#include "db/SyncEngine.h"

namespace ArcMeta {
    void cleanupTempDir() {
        QString tempPath = QDir::tempPath() + "/amtemp";
        QDir dir(tempPath);
        if (dir.exists()) {
            dir.removeRecursively();
        }
    }
}

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 1. 生命周期管理：启动时清理临时文件
    ArcMeta::cleanupTempDir();

    // 2. 初始化数据库
    if (!ArcMeta::Database::instance().init()) {
        return -1;
    }

    // 3. 初始化同步引擎并建立联动
    ArcMeta::SyncEngine* engine = new ArcMeta::SyncEngine(&a);
    QObject::connect(&ArcMeta::SyncQueue::instance(), &ArcMeta::SyncQueue::syncRequested,
                     engine, &ArcMeta::SyncEngine::onSyncRequested);

    // 4. 启动底层文件索引引擎 (单卷演示：C盘)
    static ArcMeta::FileIndex globalIndex;
    ArcMeta::MftReader reader;

    // 5. 初始化主窗口 (传入索引引用)
    ArcMeta::MainWindow w(globalIndex);

    if (reader.scanVolume("C:")) {
        globalIndex = reader.getIndex();

        // 6. 启动实时监听
        ArcMeta::UsnWatcher* watcher = new ArcMeta::UsnWatcher(globalIndex, "C:", &a);
        watcher->start();
    }

    w.show();

    // 7. 生命周期管理：程序退出前强制刷新元数据队列，并清理临时文件
    QObject::connect(&a, &QCoreApplication::aboutToQuit, []() {
        ArcMeta::SyncQueue::instance().flush();
        ArcMeta::Database::instance().close();
        ArcMeta::cleanupTempDir(); // 退出前二次清理
    });

    return a.exec();
}
