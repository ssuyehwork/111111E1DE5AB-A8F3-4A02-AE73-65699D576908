#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

namespace ArcMeta {

/**
 * @brief 独立日志工具类，绕过 qDebug 直接写入本地文件
 * 确保日志输出不受 Qt 消息处理器拦截，且能实时固化到磁盘。
 */
class Logger {
public:
    static void log(const QString& msg) {
        QFile file("drag_drop_debug.log");
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz ") << msg << Qt::endl;
            file.flush();
            file.close();
        }
    }
};

} // namespace ArcMeta

#endif // LOGGER_H
