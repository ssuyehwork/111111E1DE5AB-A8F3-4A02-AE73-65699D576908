#ifndef SYNC_QUEUE_H
#define SYNC_QUEUE_H

#include <QObject>
#include <QStringList>
#include <QSet>
#include <QMutex>
#include <QTimer>
#include <atomic>

namespace ArcMeta {

class SyncQueue : public QObject {
    Q_OBJECT
public:
    static SyncQueue& instance();

    // 将需要同步的文件夹路径加入队列
    void enqueue(const QString& dirPath);

    // 立即执行同步（用于程序关闭前的强制刷空）
    void flush();

signals:
    // 通知同步引擎执行具体解析和写入数据库操作
    void syncRequested(const QStringList& paths);

private:
    SyncQueue(QObject* parent = nullptr);
    ~SyncQueue() = default;
    SyncQueue(const SyncQueue&) = delete;
    SyncQueue& operator=(const SyncQueue&) = delete;

private slots:
    void processQueue();

private:
    QSet<QString> m_pendingPaths;
    QMutex m_mutex;
    QTimer* m_debounceTimer;
    const int DEBOUNCE_INTERVAL_MS = 1000;
};

} // namespace ArcMeta

#endif // SYNC_QUEUE_H
