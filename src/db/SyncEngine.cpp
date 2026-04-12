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

namespace ArcMeta {

SyncEngine& SyncEngine::instance() {
    static SyncEngine inst;
    return inst;
}

void SyncEngine::runIncrementalSync() {
    qDebug() << "[Sync] 增量同步已由实时监控接管";
}

void SyncEngine::runFullScan(std::function<void(int current, int total)> onProgress) {
    qDebug() << "[Sync] >>> 开始全量 GLOB 扫描与对账 <<<";
    std::vector<std::wstring> drivesToScan;
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (drives & (1 << i)) {
            wchar_t drivePath[] = { (wchar_t)(L'A' + i), L':', L'\\', L'\0' };
            if (GetDriveTypeW(drivePath) == DRIVE_FIXED || GetDriveTypeW(drivePath) == DRIVE_REMOVABLE) {
                drivesToScan.push_back(drivePath);
            }
        }
    }
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    if (!db.isOpen()) return;
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
            for (const auto& entry : std::filesystem::recursive_directory_iterator(drivesToScan[i], std::filesystem::directory_options::skip_permission_denied)) {
                std::wstring fullPath = entry.path().wstring();
                std::wstring parentPath = entry.path().parent_path().wstring();
                if (fullPath.find(L"\\.") != std::wstring::npos || fullPath.find(L"$RECYCLE.BIN") != std::wstring::npos) continue;
                HANDLE hFile = CreateFileW(fullPath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
                std::wstring frnStr = L"";
                if (hFile != INVALID_HANDLE_VALUE) {
                    BY_HANDLE_FILE_INFORMATION fileInfo;
                    if (GetFileInformationByHandle(hFile, &fileInfo)) {
                        unsigned __int64 frn = ((unsigned __int64)fileInfo.nFileIndexHigh << 32) | (unsigned __int64)fileInfo.nFileIndexLow;
                        frnStr = QString::number(frn, 16).prepend("0x").toStdWString();
                    }
                    CloseHandle(hFile);
                }
                if (frnStr.empty()) continue;
                auto ftime = std::filesystem::last_write_time(entry);
                auto stime = std::chrono::duration_cast<std::chrono::milliseconds>(ftime.time_since_epoch()).count();
                ItemRepo::saveBasicInfo(volSerial, frnStr, fullPath, parentPath, entry.is_directory(), (double)stime, (double)entry.file_size());
                count++;
                if (count % 500 == 0) { db.commit(); db.transaction(); }
            }
        } catch (...) {}
        db.commit();
        if (onProgress) onProgress(i + 1, totalDrives);
    }
    rebuildTagStats();
}

void SyncEngine::rebuildTagStats() {
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    if (!db.isOpen()) return;
    db.transaction();
    QSqlQuery qDelete(db); qDelete.exec("DELETE FROM tags");
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
