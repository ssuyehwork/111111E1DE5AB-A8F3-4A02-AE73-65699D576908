#include <QApplication>
#include "db/Database.h"
#include "meta/AmMetaJson.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 初始化数据库
    ArcMeta::Database::instance().init();

    // 初始化主窗口
    ArcMeta::MainWindow w;
    w.show();

    return a.exec();
}
