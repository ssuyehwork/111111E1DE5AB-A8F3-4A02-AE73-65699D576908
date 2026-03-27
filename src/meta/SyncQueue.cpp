#include "SyncQueue.h"
#include <QCoreApplication>
#include <QDebug>

namespace ArcMeta {

SyncQueue& SyncQueue::instance() {
    static SyncQueue inst;
    return inst;
}

SyncQueue::SyncQueue(QObject* parent) : QObject(parent) {
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    connect(m_debounceTimer, &QTimer::timeout, this, &SyncQueue::processQueue);
}

void SyncQueue::enqueue(const QString& dirPath) {
    QMutexLocker locker(&m_mutex);
    m_pendingPaths.insert(dirPath);

    // 如果定时器没在跑，就启动它；如果在跑，就自动延长了防抖时间
    if (!m_debounceTimer->isActive()) {
        m_debounceTimer->start(DEBOUNCE_INTERVAL_MS);
    }
}

void SyncQueue::flush() {
    m_debounceTimer->stop();
    processQueue();
}

void SyncQueue::processQueue() {
    QStringList pathsToSync;
    {
        QMutexLocker locker(&m_mutex);
        if (m_pendingPaths.isEmpty()) return;
        pathsToSync = m_pendingPaths.values();
        m_pendingPaths.clear();
    }

    qDebug() << "SyncQueue: 发起批量同步请求，路径数量:" << pathsToSync.size();
    emit syncRequested(pathsToSync);
}

} // namespace ArcMeta
