#include "SyncQueue.h"
#include "AmMetaJson.h"
#include "../db/Database.h"
#include "../db/FolderRepo.h"
#include <windows.h>
#include <QSqlDatabase>
#include <QSqlQuery>
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
 * @brief 内部辅助：获取路径所在磁盘的卷序列号
 */
static std::wstring getVolumeSerial(const std::wstring& path) {
    if (path.length() < 2) return L"UNKNOWN";
    wchar_t root[4] = { path[0], L':', L'\\', L'\0' };
    DWORD serialNumber = 0;
    if (GetVolumeInformationW(root, nullptr, 0, &serialNumber, nullptr, nullptr, nullptr, 0)) {
        wchar_t buf[16];
        swprintf(buf, 16, L"%08X", serialNumber);
        return buf;
    }
    return L"UNKNOWN";
}

/**
 * @brief 核心业务逻辑：从 JSON 同步数据到 SQLite 事务
 */
bool SyncQueue::processBatch() {
    std::vector<std::wstring> batch;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pendingPaths.empty()) return false;
        batch.assign(m_pendingPaths.begin(), m_pendingPaths.end());
        m_pendingPaths.clear();
    }

    if (batch.empty()) return false;

    // 2026-03-xx 统一使用 getThreadDatabase 机制，修复后台同步线程的数据库连接警告。
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();

    if (!db.isOpen()) return false;

    try {
        // 2026-05-20 性能优化：微事务分片处理，每 50 个路径提交一次事务
        const size_t chunkSize = 50;
        for (size_t i = 0; i < batch.size(); i += chunkSize) {
            db.transaction();
            
            size_t end = std::min(i + chunkSize, batch.size());
            for (size_t j = i; j < end; ++j) {
                const auto& path = batch[j];
                qDebug() << "[SyncQueue] 正在从 JSON 同步目录到数据库:" << QString::fromStdWString(path);
                AmMetaJson meta(path);
                if (!meta.load()) {
                    qWarning() << "[SyncQueue] 加载 JSON 失败:" << QString::fromStdWString(path);
                    continue;
                }

                std::wstring volSerial = getVolumeSerial(path);
                std::wstring relPath = path;
                if (path.length() >= 3 && path[1] == L':' && path[2] == L'\\') {
                    relPath = path.substr(2);
                }

                // 1. 同步文件夹
                FolderRepo::save(volSerial, relPath, meta.folder());

                // 2. 同步子条目
                for (const auto& [name, iMeta] : meta.items()) {
                    ItemRepo::save(path, name, iMeta);
                }
            }

            db.commit();

            // 关键优化点：分片间隙主动让出写锁，确保主线程交互优先
            if (end < batch.size()) {
                QThread::msleep(5);
            }
        }
        return true;
    } catch (...) {
        if (db.isOpen()) db.rollback();
        return false;
    }
}

} // namespace ArcMeta
