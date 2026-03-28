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
#include "ui/StringUtils.h"

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

#ifdef RAPID_NOTES_TARGET
#include "core/ClipboardMonitor.h"
#include "core/HttpServer.h"
#endif

#ifdef RAPID_MANAGER_TARGET
#include "ui/MainWindow.h"
#endif

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setOrganizationName("ArcMetaProject");
    a.setWindowIcon(QIcon(":/app_icon.png"));

    // 清理临时目录
    cleanTempDir();

#ifdef RAPID_MANAGER_TARGET
    a.setApplicationName("ArcMeta");
    QString serverName = "ArcMeta_SingleInstance_Server";
    QString dbFileName = "arcmeta.db";
    a.setQuitOnLastWindowClosed(true);
#else
    a.setApplicationName("RapidNotes");
    QString serverName = "RapidNotes_SingleInstance_Server";
    QString dbFileName = "inspiration.db";
    a.setQuitOnLastWindowClosed(false);
#endif

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

#ifdef RAPID_MANAGER_TARGET
    MainWindow mainWin;
    mainWin.setObjectName("ArcMetaMainWindow");
    mainWin.setWindowTitle("ArcMeta");
    mainWin.show();
#else
    HttpServer::instance().start(23333);

    QObject::connect(&ClipboardMonitor::instance(), &ClipboardMonitor::newContentDetected, 
        [](const QString& content, const QString& type, const QByteArray& data, const QString& sourceApp, const QString& sourceTitle){
        Q_UNUSED(content); Q_UNUSED(type); Q_UNUSED(data); Q_UNUSED(sourceApp); Q_UNUSED(sourceTitle);
    });
#endif

    int result = a.exec();

    // 退出前再次清理临时目录
    cleanTempDir();

    DatabaseManager::instance().closeAndPack();
    return result;
}
