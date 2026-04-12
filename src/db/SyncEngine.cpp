#include "SyncEngine.h"
#include "Database.h"
#include "ItemRepo.h"
#include "../meta/MetadataDefs.h"
#include <windows.h>
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QThread>
#include <QFileInfo>
#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <filesystem>
#include <map>
#include <chrono>
#include <algorithm>
#include <functional>

namespace ArcMeta {

SyncEngine& SyncEngine::instance() {
    static SyncEngine inst;
    return inst;
}

void SyncEngine::runIncrementalSync() {
    qDebug() << "[Sync] 增量同步已由实时监控接管";
}

void SyncEngine::runFullScan(std::function<void(int current, int total)> onProgress) {
    qDebug() << "[Sync] >>> 开始全量 GLOB 扫描与对账 (仅扫描本地用户数据) <<<";
    
    // 2026-04-12 用户优化：禁止扫描系统目录、临时、缓存，仅扫描用户常用区域
    const std::vector<std::wstring> EXCLUDED_DIRS = {
        L"\\Windows", L"\\System32", L"\\SysWOW64",
        L"\\Program Files", L"\\Program Files (x86)",
        L"\\ProgramData", L"\\$RECYCLE.BIN", L"\\System Volume Information",
        L"\\.git", L"\\.vs", L"\\node_modules", L"\\Temp", L"\\tmp",
        L"\\AppData", L"\\Local Settings", L"\\__pycache__"
    };

    auto shouldSkipDirectory = [&](const std::wstring& path) -> bool {
        // 排除隐藏目录和系统目录
        if (path.find(L"\\.") != std::wstring::npos) return true;
        // 检查是否在排除列表中
        std::wstring upperPath = path;
        std::transform(upperPath.begin(), upperPath.end(), upperPath.begin(), ::towupper);
        for (const auto& excluded : EXCLUDED_DIRS) {
            if (upperPath.find(excluded) != std::wstring::npos) {
                return true;
            }
        }
        return false;
    };

    std::vector<std::wstring> drivesToScan;
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (drives & (1 << i)) {
            wchar_t drivePath[] = { (wchar_t)(L'A' + i), L':', L'\\', L'\0' };
            UINT driveType = GetDriveTypeW(drivePath);
            // 2026-04-12 用户优化：仅扫描本地固定驱动器（不扫描光驱、映射网络驱动器等）
            if (driveType == DRIVE_FIXED) {
                drivesToScan.push_back(drivePath);
            }
        }
    }

    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    if (!db.isOpen()) {
        qCritical() << "[Sync] 数据库连接失败，无法进行扫描";
        return;
    }

    int totalDrives = (int)drivesToScan.size();
    for (int i = 0; i < totalDrives; ++i) {
        db.transaction();
        try {
            int count = 0;
            std::wstring volLetter = drivesToScan[i].substr(0, 2);
            wchar_t root[4] = { volLetter[0], L':', L'\\', L'\0' };
            DWORD serialNumber = 0;
            std::wstring volSerial = L"UNKNOWN";
            if (GetVolumeInformationW(root, nullptr, 0, &serialNumber, nullptr, nullptr, nullptr, 0)) {
                wchar_t buf[16];
                swprintf(buf, 16, L"%08X", serialNumber);
                volSerial = buf;
            }

            // 2026-04-12 用户优化：仅扫描深度 6 级，避免递归过深
            int maxDepth = 6;

            try {
                for (auto it = std::filesystem::recursive_directory_iterator(
                         drivesToScan[i],
                         std::filesystem::directory_options::skip_permission_denied | 
                         std::filesystem::directory_options::follow_directory_symlink);
                     it != std::filesystem::recursive_directory_iterator();
                     ++it) {
                    
                    // 检查深度限制
                    int depth = 0;
                    std::wstring pathStr = it->path().wstring();
                    for (auto& c : pathStr) {
                        if (c == L'\\' || c == L'/') depth++;
                    }
                    if (depth > maxDepth) {
                        it.disable_recursion_pending();
                        continue;
                    }

                    // 早期跳过排除目录
                    if (shouldSkipDirectory(it->path().wstring())) {
                        it.disable_recursion_pending();
                        continue;
                    }

                    const auto& entry = *it;
                    std::wstring fullPath = entry.path().wstring();
                    std::wstring parentPath = entry.path().parent_path().wstring();

                    // 2026-04-12 用户优化：避免频繁调用 CreateFileW，仅对确实需要的文件调用
                    // 使用文件系统信息直接计算伪 FRN（减轻系统负担）
                    ULARGE_INTEGER fileSize;
                    fileSize.QuadPart = 0;
                    auto ftime = std::filesystem::last_write_time(entry);
                    auto stime = std::chrono::duration_cast<std::chrono::milliseconds>(ftime.time_since_epoch()).count();

                    try {
                        fileSize.QuadPart = std::filesystem::file_size(entry);
                    } catch (...) {
                        continue;  // 无法访问的文件跳过
                    }

                    // 2026-04-12 用户优化：生成稳定的伪 FRN（基于卷序列+路径 hash）
                    // 避免每次都调用 CreateFileW
                    std::hash<std::wstring> hasher;
                    size_t pathHash = hasher(fullPath);
                    unsigned __int64 pseudoFrn = (static_cast<unsigned __int64>(serialNumber) << 32) | 
                                                 ((pathHash & 0xFFFFFFFF) ? 0xFFFFFFFF : 1);
                    std::wstring frnStr = QString::number(pseudoFrn, 16).prepend("0x").toStdWString();

                    ItemRepo::saveBasicInfo(volSerial, frnStr, fullPath, parentPath, entry.is_directory(), (double)stime, (double)fileSize.QuadPart);
                    count++;

                    // 2026-04-12 用户优化：降低事务提交频率，减少 I/O 活动
                    if (count % 1000 == 0) {
                        db.commit();
                        db.transaction();
                        qDebug() << "[Sync] 已处理 " << count << " 個文件";
                    }
                }
            } catch (const std::exception& e) {
                qWarning() << "[Sync] 扫描异常:" << e.what();
            }

            db.commit();
            qDebug() << "[Sync] 驱动器 " << drivesToScan[i].c_str() << " 扫描完成，共 " << count << " 个条目";
            if (onProgress) onProgress(i + 1, totalDrives);
        } catch (...) {
            db.commit();
        }
    }

    rebuildTagStats();
    qDebug() << "[Sync] >>> 全量对账完成 <<<";
}

void SyncEngine::rebuildTagStats() {
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    if (!db.isOpen()) {
        qCritical() << "[Sync] 标签统计重建失败：数据库连接无效";
        return;
    }
    
    if (!db.transaction()) {
        qCritical() << "[Sync] 无法启动事务:" << db.lastError().text();
        return;
    }
    
    QSqlQuery qDelete(db);
    if (!qDelete.exec("DELETE FROM tags")) {
        qWarning() << "[Sync] 删除旧标签失败:" << qDelete.lastError().text();
    }
    
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
    for (auto it = tagCounts.begin(); it != tagCounts.end(); ++it) {
        QSqlQuery ins(db);
        ins.prepare("INSERT INTO tags (tag, item_count) VALUES (?, ?)");
        ins.addBindValue(QString::fromStdString(it->first));
        ins.addBindValue(it->second);
        ins.exec();
    }
    db.commit();
}

void SyncEngine::scanDirectory(const std::filesystem::path& root, std::vector<std::wstring>& metaFiles) {
    Q_UNUSED(root); Q_UNUSED(metaFiles);
}

} // namespace ArcMeta
