#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char *argv[]) {
    // 2026-03-27 按照文档：本项目强制由 Visual Studio 2022 编译
    QApplication a(argc, argv);

    ArcMeta::MainWindow w;
    w.show();

    return a.exec();
}
