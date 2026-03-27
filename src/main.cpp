#include <QApplication>
#include "db/Database.h"
#include "meta/AmMetaJson.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 初始化数据库
    ArcMeta::Database::instance().init();

    // 后续将初始化主窗口

    return a.exec();
}
