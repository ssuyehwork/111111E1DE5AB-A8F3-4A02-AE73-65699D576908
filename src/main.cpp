#include <QSettings>
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QThreadPool>
#include <QPointer>
#include <functional>

#include "core/DatabaseManager.h"
#include "ui/ToolTipOverlay.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

void cleanTempDir() {
    QString tempPath = QDir::tempPath() + "/amtemp/";
    QDir dir(tempPath);
    if (dir.exists()) {
        dir.removeRecursively();
    }
    dir.mkpath(".");
}

#include "ui/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setOrganizationName("ArcMetaProject");
    a.setWindowIcon(QIcon(":/app_icon.png"));

    // 清理临时目录
    cleanTempDir();

    a.setApplicationName("ArcMeta");
    QString serverName = "ArcMeta_SingleInstance_Server";
    QString dbFileName = "arcmeta.db";
    a.setQuitOnLastWindowClosed(true);

    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(500)) return 0;

    QLocalServer::removeServer(serverName);
    QLocalServer server;
    server.listen(serverName);

    QFile styleFile(":/qss/dark_style.qss");
    if (styleFile.open(QFile::ReadOnly)) a.setStyleSheet(styleFile.readAll());

    QString dbPath = QCoreApplication::applicationDirPath() + "/" + dbFileName;
    if (!DatabaseManager::instance().init(dbPath)) {
        QMessageBox::critical(nullptr, "启动失败", "无法加载加密数据库外壳。");
        return -1;
    }

    MainWindow mainWin;
    mainWin.setObjectName("ArcMetaMainWindow");
    mainWin.setWindowTitle("ArcMeta");
    mainWin.show();

    int result = a.exec();

    // 退出前再次清理临时目录
    cleanTempDir();

    DatabaseManager::instance().closeAndPack();
    return result;
}
