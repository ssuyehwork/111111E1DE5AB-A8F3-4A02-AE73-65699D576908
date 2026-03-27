#include <QApplication>
#include "db/Database.h"
#include "meta/AmMetaJson.h"
#include "ui/MainWindow.h"
#include "mft/MftReader.h"
#include "mft/UsnWatcher.h"
#include "meta/SyncQueue.h"
#include "db/SyncEngine.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 1. 初始化数据库
    if (!ArcMeta::Database::instance().init()) {
        return -1;
    }

    // 2. 初始化同步引擎并建立联动
    ArcMeta::SyncEngine* engine = new ArcMeta::SyncEngine(&a);
    QObject::connect(&ArcMeta::SyncQueue::instance(), &ArcMeta::SyncQueue::syncRequested,
                     engine, &ArcMeta::SyncEngine::onSyncRequested);

    // 3. 启动底层文件索引引擎 (单卷演示：C盘)
    static ArcMeta::FileIndex globalIndex;
    ArcMeta::MftReader reader;

    // 4. 初始化主窗口 (传入索引引用)
    ArcMeta::MainWindow w(globalIndex);

    if (reader.scanVolume("C:")) {
        globalIndex = reader.getIndex();

        // 5. 启动实时监听
        ArcMeta::UsnWatcher* watcher = new ArcMeta::UsnWatcher(globalIndex, "C:", &a);
        watcher->start();
    }

    w.show();

    // 6. 生命周期管理：程序退出前强制刷新元数据队列，确保数据真值完整落地
    QObject::connect(&a, &QCoreApplication::aboutToQuit, []() {
        ArcMeta::SyncQueue::instance().flush();
        ArcMeta::Database::instance().close();
    });

    return a.exec();
}
