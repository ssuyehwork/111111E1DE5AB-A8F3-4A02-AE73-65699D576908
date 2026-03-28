#include "SyncQueue.h"
#include "AmMetaJson.h"
#include "../db/Database.h"
#include "../db/FolderRepo.h"
#include "../db/ItemRepo.h"
#include <vector>
#include <QString>
#include <QDateTime>

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
    // 程序退出前执行最后的强制同步
    flush();
}

void SyncQueue::enqueue(const std::wstring& folderPath) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingPaths.insert(folderPath); // set 自动去重
    }
    m_cv.notify_one();
}

void SyncQueue::flush() {
    while (true) {
        if (!processBatch()) break;
    }
}

void SyncQueue::workerThread() {
    while (m_running) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(2000), [this] {
                return !m_running || !m_pendingPaths.empty();
            });
        }

        if (!m_running && m_pendingPaths.empty()) break;

        // 批量处理当前队列中的路径
        processBatch();
    }
}

/**
 * @brief 核心业务逻辑：从 JSON 同步数据到 SQLite 事务
 */
bool SyncQueue::processBatch() {
    std::vector<std::wstring> batch;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pendingPaths.empty()) return false;

        // 一次性取出当前所有待处理路径进行批处理
        batch.assign(m_pendingPaths.begin(), m_pendingPaths.end());
        m_pendingPaths.clear();
    }

    if (batch.empty()) return false;

    try {
        // 关键红线：使用事务批量提交
        auto transaction = Database::instance().createTransaction();
        auto sqlite = Database::instance().sqlite();

        for (const auto& path : batch) {
            AmMetaJson meta(path);
            if (!meta.load()) continue;

            // 1. 使用 Repository 同步文件夹
            FolderRepo::save(path, meta.folder());

            // 2. 使用 Repository 同步所有条目
            for (const auto& [name, iMeta] : meta.items()) {
                ItemRepo::save(path, name, iMeta);
            }
        }

        transaction->commit();
        return true;
    } catch (const std::exception& e) {
        // 事务失败会自动回滚 (RAII Transaction)
        return false;
    }
}

} // namespace ArcMeta
