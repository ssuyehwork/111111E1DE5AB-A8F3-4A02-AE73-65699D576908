#include "SyncQueue.h"
#include "AmMetaJson.h"
#include <windows.h>
#include <QThread>
#include <vector>
#include <algorithm>
#include <QString>
#include <QDateTime>
#include <QDebug>

namespace ArcMeta {

SyncQueue& SyncQueue::instance() {
    static SyncQueue inst;
    return inst;
}

SyncQueue::SyncQueue() {}

SyncQueue::~SyncQueue() {
    stop();
}

void SyncQueue::start() {
    if (m_running) return;
    m_running = true;
    m_thread = std::thread(&SyncQueue::workerThread, this);
}

void SyncQueue::stop() {
    if (!m_running) return;
    m_running = false;
    m_cv.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void SyncQueue::enqueue(const std::wstring& folderPath) {
    // 2026-05-22 按照用户要求：废除数据库。
    // SyncQueue 原本职责是异步将 JSON 同步到数据库。
    // 在纯 JSON 模式下，如果后续有耗时的 JSON 批量操作可以放在这里。
    // 目前暂时将其逻辑设为空。
    Q_UNUSED(folderPath);
}

void SyncQueue::flush() {
    // 2026-05-22 按照用户要求：废除数据库。
}

void SyncQueue::workerThread() {
    while (m_running) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait_for(lock, std::chrono::milliseconds(2000), [this] {
            return !m_running;
        });
        
        if (!m_running) break;
    }
}

bool SyncQueue::processBatch() {
    // 2026-05-22 按照用户要求：废除数据库，不再需要处理同步批次。
    return false;
}

} // namespace ArcMeta
