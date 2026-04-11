#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <windows.h>
#include <shellapi.h>
#include "ui/MainWindow.h"
#include "db/Database.h"
#include "db/SyncEngine.h"
#include "meta/SyncQueue.h"
#include "core/CoreController.h"

/**
 * @brief 自定义日志处理程序，将 qDebug 消息重定向至本地 .log 文件
 * 2026-03-xx 按照用户要求：在手动运行 .exe 时，通过日志文件排查初始化挂起或信号丢失问题。
 */
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    Q_UNUSED(context); // 显式消除未引用参数警告以满足编译严苛性要求
    QFile logFile("arcmeta_debug.log");
    // 采用追加模式，并确保每次写入都刷新到磁盘
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream textStream(&logFile);
        QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
        
        // 映射消息类型
        QString level;
        switch (type) {
            case QtDebugMsg:    level = "DEBUG";    break;
            case QtInfoMsg:     level = "INFO ";    break;
            case QtWarningMsg:  level = "WARN ";    break;
            case QtCriticalMsg: level = "CRIT ";    break;
            case QtFatalMsg:    level = "FATAL";    break;
        }

        textStream << QString("[%1][%2] %3").arg(timeStr, level, msg) << Qt::endl;
        logFile.close();
    }
}

int main(int argc, char *argv[]) {
    // 1. 安装自定义日志处理器：确保从程序启动的第一秒开始就能捕获所有调试信息
    qInstallMessageHandler(customMessageHandler);
    qDebug() << "================ ArcMeta 启动加载 (MFT 已移除) ==================";

    // 设置高 DPI 支持 (Qt 6 默认开启，此处显式设置)
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication a(argc, argv);

    // 2026-03-xx 按照用户要求：设置全局应用图标，确保任务栏及窗口显示 Logo
    a.setWindowIcon(QIcon(":/app_icon.png"));

    a.setApplicationName("ArcMeta");
    a.setOrganizationName("ArcMetaTeam");

    // 2. 线程亲和性预热：在主线程显式初始化单例，防止后台线程抢占导致 QTimer 失效或闪退
    ArcMeta::MetadataManager::instance();
    ArcMeta::CoreController::instance();

    // 3. 初始化数据库 (仅核心表结构，必须同步完成)
    std::wstring dbPath = L"arcmeta.db";
    if (!ArcMeta::Database::instance().init(dbPath)) {
        QMessageBox::critical(nullptr, "错误", "无法初始化数据库，程序即将退出。");
        return -1;
    }

    // 3. 秒开重构：
    // 2026-05-20 性能优化：主窗口使用指针分配，配合 deleteLater 确保退出清理时的稳定性，防止栈对象销毁竞态闪退。
    auto* w = new ArcMeta::MainWindow();
    w->show();

    // 4. 启动异步初始化中控
    ArcMeta::CoreController::instance().startSystem();

    int ret = a.exec();

    // 5. 优雅退出清理
    // 先停止同步队列防止后台 IO 异常
    ArcMeta::SyncQueue::instance().stop();
    // 显式销毁窗口
    w->deleteLater();

    return ret;
}
