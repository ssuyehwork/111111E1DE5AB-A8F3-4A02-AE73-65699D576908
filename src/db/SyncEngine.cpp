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

static qint64 getPathMtime(const std::filesystem::path& p) {
    try {
        auto ftime = std::filesystem::last_write_time(p);
        return std::chrono::duration_cast<std::chrono::milliseconds>(ftime.time_since_epoch()).count();
    } catch (...) {
        return 0;
    }
}

void SyncEngine::scanDirInternal(const std::wstring& path, int depth, ScanContext& ctx) {
    if (depth > ctx.maxDepth || ctx.shouldSkip(path)) {
        return;
    }

    std::filesystem::path fsPath(path);
    qint64 diskMtime = getPathMtime(fsPath);
    qint64 cachedMtime = Database::instance().queryFolderCache(path);

    bool needsScan = (diskMtime == 0 || diskMtime != cachedMtime);

    if (!needsScan) {
        ctx.skipDirCount++;
    } else {
        ctx.scanDirCount++;
    }

    QSqlDatabase db = Database::instance().getThreadDatabase();

    try {
        for (const auto& entry : std::filesystem::directory_iterator(fsPath, 
                 std::filesystem::directory_options::skip_permission_denied)) {
            
            std::wstring fullPath = entry.path().wstring();
            bool isDir = entry.is_directory();

            if (needsScan) {
                // 执行对账：仅在 mtime 不一致时处理当前目录下的文件/文件夹
                std::wstring parentPath = entry.path().parent_path().wstring();
                
                ULARGE_INTEGER fileSize;
                fileSize.QuadPart = 0;
                qint64 stime = 0;

                try {
                    auto ftime = entry.last_write_time();
                    stime = std::chrono::duration_cast<std::chrono::milliseconds>(ftime.time_since_epoch()).count();
                    if (!isDir) {
                        fileSize.QuadPart = entry.file_size();
                    }
                } catch (...) {}

                // 生成伪 FRN（维持现有逻辑）
                std::hash<std::wstring> hasher;
                size_t pathHash = hasher(fullPath);
                unsigned __int64 pseudoFrn = (static_cast<unsigned __int64>(ctx.serialNumber) << 32) | 
                                             ((pathHash & 0xFFFFFFFF) ? 0xFFFFFFFF : 1);
                std::wstring frnStr = QString::number(pseudoFrn, 16).prepend("0x").toStdWString();

                ItemRepo::saveBasicInfo(ctx.volSerial, frnStr, fullPath, parentPath, isDir, (double)stime, (double)fileSize.QuadPart);
                ctx.totalFiles++;

                // 2026-04-12 按照用户要求：回归每 1000 个条目提交一次事务的鲁棒逻辑
                if (ctx.totalFiles % 1000 == 0) {
                    db.commit();
                    db.transaction();
                    qDebug() << "[Sync] 已处理 " << ctx.totalFiles << " 個条目";
                }
            }

            // 核心剪枝策略：mtime 不冒泡，无论父目录是否变化，必须递归检查子目录
            if (isDir) {
                scanDirInternal(fullPath, depth + 1, ctx);
            }
        }

        // 扫描完成后更新缓存
        if (needsScan) {
            Database::instance().upsertFolderCache(path, diskMtime);
        }

    } catch (const std::exception& e) {
        // 2026-04-12 按照用户要求：修正 wstring 直接输出到 qDebug 的错误
        qWarning() << "[Sync] 访问目录异常:" << QString::fromStdWString(path) << " -> " << e.what();
    }
}

void SyncEngine::runFullScan(std::function<void(int current, int total)> onProgress) {
    auto startTime = std::chrono::steady_clock::now();
    qDebug() << "[Sync] >>> 开始增量剪枝扫描与对账 <<<";
    
    const std::vector<std::wstring> EXCLUDED_DIRS = {
        L"\\Windows", L"\\System32", L"\\SysWOW64",
        L"\\Program Files", L"\\Program Files (x86)",
        L"\\ProgramData", L"\\$RECYCLE.BIN", L"\\System Volume Information",
        L"\\.git", L"\\.vs", L"\\node_modules", L"\\Temp", L"\\tmp",
        L"\\AppData", L"\\Local Settings", L"\\__pycache__"
    };

    auto shouldSkipDirectory = [&](const std::wstring& path) -> bool {
        if (path.find(L"\\.") != std::wstring::npos) return true;
        std::wstring upperPath = path;
        std::transform(upperPath.begin(), upperPath.end(), upperPath.begin(), ::towupper);
        for (const auto& excluded : EXCLUDED_DIRS) {
            if (upperPath.find(excluded) != std::wstring::npos) return true;
        }
        return false;
    };

    std::vector<std::wstring> drivesToScan;
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (drives & (1 << i)) {
            wchar_t drivePath[] = { (wchar_t)(L'A' + i), L':', L'\\', L'\0' };
            if (GetDriveTypeW(drivePath) == DRIVE_FIXED) {
                drivesToScan.push_back(drivePath);
            }
        }
    }

    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    if (!db.isOpen()) {
        qCritical() << "[Sync] 数据库连接失败";
        return;
    }

    ScanContext ctx;
    ctx.maxDepth = 6;
    ctx.shouldSkip = shouldSkipDirectory;

    int totalDrives = (int)drivesToScan.size();
    for (int i = 0; i < totalDrives; ++i) {
        db.transaction();
        
        std::wstring volLetter = drivesToScan[i].substr(0, 2);
        wchar_t root[4] = { volLetter[0], L':', L'\\', L'\0' };
        ctx.serialNumber = 0;
        ctx.volSerial = L"UNKNOWN";
        // 2026-04-12 关键修复：修正 GetVolumeInformationW 的 8 个完整参数传递
        if (GetVolumeInformationW(root, nullptr, 0, (LPDWORD)&ctx.serialNumber, nullptr, nullptr, nullptr, 0)) {
            wchar_t buf[16];
            // 2026-04-12 关键修复：使用 swprintf_s 确保缓冲区安全且参数配平
            swprintf_s(buf, 16, L"%08X", ctx.serialNumber);
            ctx.volSerial = buf;
        }

        // 2026-04-12 关键修复：明确调用类成员函数 scanDirInternal
        scanDirInternal(drivesToScan[i], 0, ctx);

        db.commit();
        if (onProgress) onProgress(i + 1, totalDrives);
    }

    rebuildTagStats();
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    qDebug() << "[Sync] >>> 全量对账完成 <<<";
    qDebug() << "[Sync] 统计报告:";
    qDebug() << "  - 扫描目录数:" << ctx.scanDirCount;
    qDebug() << "  - 跳过目录数:" << ctx.skipDirCount;
    qDebug() << "  - 处理条目总数:" << ctx.totalFiles;
    qDebug() << "  - 总耗时:" << duration << "ms";
}

void SyncEngine::rebuildTagStats() {
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    if (!db.isOpen()) return;
    
    if (!db.transaction()) return;
    
    QSqlQuery qDelete(db);
    qDelete.exec("DELETE FROM tags");
    
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
    for (auto const& [tag, count] : tagCounts) {
        QSqlQuery ins(db);
        ins.prepare("INSERT INTO tags (tag, item_count) VALUES (?, ?)");
        ins.addBindValue(QString::fromStdString(tag));
        ins.addBindValue(count);
        ins.exec();
    }
    db.commit();
}

void SyncEngine::scanDirectory(const std::filesystem::path& root, std::vector<std::wstring>& metaFiles) {
    Q_UNUSED(root); Q_UNUSED(metaFiles);
}

} // namespace ArcMeta
