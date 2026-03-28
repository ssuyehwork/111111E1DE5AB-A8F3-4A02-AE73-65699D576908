#include "SyncEngine.h"
#include "Database.h"
#include "../meta/SyncQueue.h"
#include <windows.h>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>

namespace ArcMeta {

SyncEngine& SyncEngine::instance() {
    static SyncEngine inst;
    return inst;
}

/**
 * @brief 增量同步：只处理 mtime > last_sync_time 的 .am_meta.json
 */
void SyncEngine::runIncrementalSync() {
    auto sqlite = Database::instance().sqlite();
    double lastSyncTime = 0;

    // 获取上次同步时间
    SQLite::Statement st(*sqlite, "SELECT value FROM sync_state WHERE key = 'last_sync_time'");
    if (st.executeStep()) {
        lastSyncTime = st.getColumn(0).getDouble();
    }

    // 执行全表增量扫描逻辑
    SQLite::Statement query(*sqlite, "SELECT path FROM folders");
    while (query.executeStep()) {
        std::wstring path = QString::fromStdString(query.getColumn(0).getText()).toStdWString();
        std::wstring jsonPath = path + L"\\.am_meta.json";

        QFileInfo info(QString::fromStdWString(jsonPath));
        if (info.exists() && info.lastModified().toMSecsSinceEpoch() > lastSyncTime) {
            SyncQueue::instance().enqueue(path);
        }
    }
}

/**
 * @brief 全量扫描：递归所有盘符搜集元数据
 */
void SyncEngine::runFullScan(std::function<void(int current, int total)> onProgress) {
    std::vector<std::wstring> metaFiles;

    // 枚举所有固定驱动器
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (drives & (1 << i)) {
            wchar_t drivePath[] = { (wchar_t)(L'A' + i), L':', L'\\', L'\0' };
            if (GetDriveTypeW(drivePath) == DRIVE_FIXED) {
                scanDirectory(std::filesystem::path(drivePath), metaFiles);
            }
        }
    }

    // 清理并重建核心表
    auto sqlite = Database::instance().sqlite();
    sqlite->exec("DELETE FROM folders; DELETE FROM items; DELETE FROM tags;");

    int total = (int)metaFiles.size();
    for (int i = 0; i < total; ++i) {
        // 提取父目录并加入队列进行解析同步
        std::wstring parentDir = std::filesystem::path(metaFiles[i]).parent_path().wstring();
        SyncQueue::instance().enqueue(parentDir);

        if (onProgress) onProgress(i + 1, total);
    }

    // 强制刷空队列
    SyncQueue::instance().flush();
    // 更新同步时间
    SQLite::Statement updateSync(*sqlite, "INSERT OR REPLACE INTO sync_state (key, value) VALUES ('last_sync_time', ?)");
    updateSync.bind(1, (double)QDateTime::currentMSecsSinceEpoch());
    updateSync.exec();

    rebuildTagStats();
}

/**
 * @brief 标签聚合统计逻辑
 */
void SyncEngine::rebuildTagStats() {
    auto sqlite = Database::instance().sqlite();
    auto transaction = Database::instance().createTransaction();

    sqlite->exec("DELETE FROM tags");

    // 聚合 items 表中的标签（解析存储在数据库中的 JSON 字符串）
    SQLite::Statement query(*sqlite, "SELECT tags FROM items WHERE tags != ''");
    std::map<std::string, int> tagCounts;
    while (query.executeStep()) {
        QByteArray jsonData = QByteArray::fromStdString(query.getColumn(0).getText());
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (doc.isArray()) {
            for (const auto& val : doc.array()) {
                QString t = val.toString();
                if (!t.isEmpty()) tagCounts[t.toStdString()]++;
            }
        }
    }

    for (const auto& [tag, count] : tagCounts) {
        SQLite::Statement ins(*sqlite, "INSERT INTO tags (tag, item_count) VALUES (?, ?)");
        ins.bind(1, tag);
        ins.bind(2, count);
        ins.exec();
    }

    transaction->commit();
}

/**
 * @brief 递归扫描目录（排除系统隐藏文件夹）
 */
void SyncEngine::scanDirectory(const std::filesystem::path& root, std::vector<std::wstring>& metaFiles) {
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file() && entry.path().filename() == ".am_meta.json") {
                metaFiles.push_back(entry.path().wstring());
            }
        }
    } catch (...) {
        // 忽略访问受限目录
    }
}

} // namespace ArcMeta
