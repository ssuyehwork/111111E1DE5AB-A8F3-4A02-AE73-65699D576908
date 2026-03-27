#include <QApplication>
#include "db/Database.h"
#include "meta/AmMetaJson.h"
#include "ui/MainWindow.h"
#include "mft/MftReader.h"
#include "mft/UsnWatcher.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 1. 初始化数据库
    ArcMeta::Database::instance().init();

    // 2. 启动底层文件索引引擎 (单卷演示：C盘)
    static ArcMeta::FileIndex globalIndex;
    ArcMeta::MftReader reader;
    if (reader.scanVolume("C:")) {
        globalIndex = reader.getIndex();

        // 3. 启动实时监听
        ArcMeta::UsnWatcher* watcher = new ArcMeta::UsnWatcher(globalIndex, "C:", &a);
        watcher->start();
    }

    // 4. 初始化主窗口
    ArcMeta::MainWindow w;
    w.show();

    // 5. 生命周期管理：程序退出前强制刷新元数据队列，确保数据真值完整落地
    QObject::connect(&a, &QCoreApplication::aboutToQuit, []() {
        ArcMeta::SyncQueue::instance().flush();
        ArcMeta::Database::instance().close();
    });

    return a.exec();
}
