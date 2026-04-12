#include "SyncEngine.h"
#include "Database.h"
#include <windows.h>
#include <QDateTime>
#include <QThread>
#include <QFileInfo>
#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QJsonDocument>
#include <QJsonArray>
#include <filesystem>
#include <map>

namespace ArcMeta {

SyncEngine& SyncEngine::instance() {
    static SyncEngine inst;
    return inst;
}

/**
 * @brief 增量同步
 */
void SyncEngine::runIncrementalSync() {
    // 2026-05-24 按照用户要求：彻底移除 JSON 逻辑。
    // 在中心化数据库模式下，增量同步不再依赖 .am_meta.json 的 mtime。
    // 该功能目前已在 MetadataManager 的实时持久化中完成。
    qDebug() << "[Sync] 增量扫描已跳过（中心化模式无需扫描 JSON）";
}

/**
 * @brief 全量扫描
 * 2026-05-24 按照用户要求：彻底移除基于 JSON 的扫描逻辑。
 */
void SyncEngine::runFullScan(std::function<void(int current, int total)> onProgress) {
    // 在纯中心化模式下，全量扫描逻辑需要重构为“同步文件索引”。
    // 考虑到移除 JSON 的首要目标，此处先行禁用原有的 JSON 搜集逻辑。
    qDebug() << "[Sync] 全量扫描已跳过（中心化模式不再搜集 JSON 文件）";
    if (onProgress) onProgress(100, 100);
}

/**
 * @brief 标签聚合统计逻辑
 * 2026-03-xx 物理修复：聚合任务属于重度 IO 操作，强制要求在后台线程独立连接中运行事务。
 */
void SyncEngine::rebuildTagStats() {
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    if (!db.isOpen()) return;

    db.transaction();
    
    QSqlQuery qDelete(db);
    qDelete.exec("DELETE FROM tags");
    
    // 聚合 items 表中的标签
    QSqlQuery query("SELECT tags FROM items WHERE tags != ''", db);
    std::map<std::string, int> tagCounts;
    while (query.next()) {
        QByteArray jsonData = query.value(0).toByteArray();
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (doc.isArray()) {
            for (const auto& val : doc.array()) {
                QString t = val.toString();
                if (!t.isEmpty()) tagCounts[t.toStdString()]++;
            }
        }
    }

    for (const auto& [tag, count] : tagCounts) {
        QSqlQuery ins(db);
        ins.prepare("INSERT INTO tags (tag, item_count) VALUES (?, ?)");
        ins.addBindValue(QString::fromStdString(tag));
        ins.addBindValue(count);
        ins.exec();
    }

    db.commit();
}

/**
 * @brief 递归扫描目录
 */
void SyncEngine::scanDirectory(const std::filesystem::path& root, std::vector<std::wstring>& metaFiles) {
    // 已废弃
    Q_UNUSED(root);
    Q_UNUSED(metaFiles);
}

} // namespace ArcMeta
