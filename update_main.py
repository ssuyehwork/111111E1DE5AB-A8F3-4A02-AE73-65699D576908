import re

main_content = """#include <QApplication>
#include <QMessageBox>
#include "ui/MainWindow.h"
#include "core/DatabaseManager.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setApplicationName("RapidNotes");

    if (!DatabaseManager::instance().init()) {
        QMessageBox::critical(nullptr, "Error", "Could not initialize database");
        return -1;
    }

    MainWindow w;
    w.show();
    return a.exec();
}
"""

with open("src/main.cpp", "w", encoding="utf-8") as f:
    f.write(main_content)
