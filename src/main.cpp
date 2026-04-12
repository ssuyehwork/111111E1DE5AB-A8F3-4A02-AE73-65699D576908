#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include "ui/MainWindow.h"
#include "ui/LoadingWindow.h"
#include "db/Database.h"
#include "db/SyncEngine.h"
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
    // 0. 初始化 COM 环境：支持后续调用 IShellItemImageFactory 提取专业格式缩略图
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

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

    // 2. 初始化数据库 (仅核心表结构，必须同步完成)
    std::wstring dbPath = L"arcmeta.db";
    if (!ArcMeta::Database::instance().init(dbPath)) {
        QMessageBox::critical(nullptr, "错误", "无法初始化数据库，程序即将退出。");
        return -1;
    }

    // 3. 创建加载窗口并显示
    // 2026-04-12 按照用户要求：在启动时显示加载动画，提升用户体验
    ArcMeta::LoadingWindow* loadingWindow = new ArcMeta::LoadingWindow();
    loadingWindow->show();
    qDebug() << "[Main] 加载窗口已显示";

    // 4. 连接信号：更新加载窗口的进度文本
    QObject::connect(&ArcMeta::CoreController::instance(), &ArcMeta::CoreController::statusTextChanged,
                     loadingWindow, &ArcMeta::LoadingWindow::updateStatus, Qt::QueuedConnection);

    // 5. 建立"延迟构造"联动逻辑：
    // 2026-03-xx 物理修复：为了彻底杜绝启动期间的死锁与"未响应"，改为在初始化彻底完成后再构造 MainWindow。
    // 这确保了 UI 线程在数据库高峰期保持绝对空闲，不产生任何锁竞争。
    ArcMeta::MainWindow* w = nullptr;
    QObject::connect(&ArcMeta::CoreController::instance(), &ArcMeta::CoreController::initializationFinished, &a, [&w, loadingWindow]() {
        qDebug() << "[Main] 后台就绪，通知加载窗口关闭...";
        loadingWindow->onInitializationFinished();
    }, Qt::QueuedConnection);

    // 6. 在加载窗口关闭后创建并显示主窗口
    QObject::connect(loadingWindow, &ArcMeta::LoadingWindow::finished, &a, [&w]() {
        qDebug() << "[Main] 加载窗口已关闭，正在构造 MainWindow...";
        w = new ArcMeta::MainWindow();
        w->show();
        qDebug() << "[Main] MainWindow 已显示";
    }, Qt::QueuedConnection);

    // 7. 启动异步初始化中控
    ArcMeta::CoreController::instance().startSystem();

    int ret = a.exec();

    return ret;
}
