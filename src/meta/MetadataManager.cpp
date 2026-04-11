#include "MetadataManager.h"
#include "../db/Database.h"
#include "../db/ItemRepo.h"
#include "AmMetaJson.h"
#include "SyncQueue.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QtConcurrent>
#include <QThreadPool>
#include <QDir>
#include <QDebug>
#include <mutex>
#include <shared_mutex>
#include <cwchar>
#include <windows.h>

namespace ArcMeta {

/**
 * @brief 内部辅助：获取路径所在磁盘的卷序列号
 */
static std::wstring getVolumeSerialNumber(const std::wstring& path) {
    if (path.length() < 2 || path[1] != L':') return L"UNKNOWN";
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
 * @brief 内部辅助：建立卷序列号到当前盘符的映射
 */
static std::unordered_map<std::wstring, std::wstring> getVolumeToDriveMap() {
    std::unordered_map<std::wstring, std::wstring> map;
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (drives & (1 << i)) {
            std::wstring drive = std::wstring(1, L'A' + i) + L":\\";
            std::wstring serial = getVolumeSerialNumber(drive);
            if (serial != L"UNKNOWN") {
                map[serial] = drive;
            }
        }
    }
    return map;
}

/**
 * @brief 内部辅助：获取相对路径（如 G:\Folder -> \Folder）
 */
static std::wstring getRelativePath(const std::wstring& fullPath) {
    if (fullPath.length() >= 3 && fullPath[1] == L':' && fullPath[2] == L'\\') {
        return fullPath.substr(2);
    }
    return fullPath;
}

/**
 * @brief 内部辅助：标准化路径，确保 G: 统一为 G:\，并处理斜杠一致性
 */
static std::wstring normalizePath(const std::wstring& path) {
    QString qp = QDir::toNativeSeparators(QDir::cleanPath(QString::fromStdWString(path)));
    if (qp.length() == 2 && qp.endsWith(':')) {
        qp += '\\';
    }
    return qp.toStdWString();
}

MetadataManager& MetadataManager::instance() {
    static MetadataManager inst;
    return inst;
}

MetadataManager::MetadataManager(QObject* parent) : QObject(parent) {}

void MetadataManager::initFromDatabase() {
    qDebug() << "[MetadataManager] 开始从数据库加载内存镜像(硬件卷映射模式)...";
    std::unordered_map<std::wstring, RuntimeMeta> tempCache;
    auto volToDriveMap = getVolumeToDriveMap();

    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    if (!db.isOpen()) {
        qCritical() << "[MetadataManager] 数据库未打开，加载失败";
        return;
    }

    auto loadTable = [&](const char* sql, int& count) {
        QSqlQuery query(db);
        if (!query.exec(sql)) {
            qCritical() << "[MetadataManager] 执行查询失败:" << query.lastError().text();
            return;
        }
        while (query.next()) {
            std::wstring dbVol = query.value(0).toString().toStdWString();
            std::wstring dbPath = query.value(1).toString().toStdWString();
            
            std::wstring finalPath = dbPath;
            if (volToDriveMap.count(dbVol)) {
                QString base = QString::fromStdWString(volToDriveMap[dbVol]);
                if (base.endsWith('\\')) base.chop(1);
                finalPath = (base + QString::fromStdWString(getRelativePath(dbPath))).toStdWString();
            }

            RuntimeMeta meta;
            meta.rating = query.value(2).toInt();
            meta.color = query.value(3).toString().toStdWString();
            QJsonDocument doc = QJsonDocument::fromJson(query.value(4).toByteArray());
            if (doc.isArray()) {
                for (const auto& v : doc.array()) meta.tags << v.toString();
            }
            meta.pinned = query.value(5).toInt() != 0;
            meta.encrypted = query.value(6).toInt() != 0;
            meta.note = query.value(7).toString().toStdWString();
            tempCache[finalPath] = std::move(meta);
            count++;
        }
    };

    int itemCount = 0;
    loadTable("SELECT volume, path, rating, color, tags, pinned, encrypted, note FROM items WHERE rating > 0 OR color != '' OR tags != '' OR pinned = 1 OR encrypted = 1 OR note != ''", itemCount);
    qDebug() << "[MetadataManager] 已从 items 表加载" << itemCount << "条记录";

    int folderCount = 0;
    loadTable("SELECT volume, path, rating, color, tags, pinned, encrypted, note FROM folders WHERE rating > 0 OR color != '' OR tags != '' OR pinned = 1 OR encrypted = 1 OR note != ''", folderCount);
    qDebug() << "[MetadataManager] 已从 folders 表加载" << folderCount << "条记录";

    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache = std::move(tempCache);
    }
    qDebug() << "[MetadataManager] 内存镜像初始化完成";
    
    emit metaChanged(L"__RELOAD_ALL__");
}

RuntimeMeta MetadataManager::getMeta(const std::wstring& path) {
    std::wstring nPath = normalizePath(path);
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_cache.find(nPath);
    if (it != m_cache.end()) return it->second;
    return RuntimeMeta(); // 返回默认空元数据
}

void MetadataManager::prefetchDirectory(const std::wstring& dirPath) {
    std::wstring nDirPath = normalizePath(dirPath);
    qDebug() << "[MetadataManager] 预加载目录元数据(访问即生成):" << QString::fromStdWString(nDirPath);
    
    QThreadPool::globalInstance()->start([this, nDirPath]() {
        AmMetaJson json(nDirPath);
        
        // 2026-04-10 按照用户铁律：访问即生成
        // 强制调用 save()。如果文件不存在，它会创建一个包含默认 folder 节点的初始文件。
        json.load();
        json.save();

        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            
            // 1. 加载文件夹自身的元数据 (folder 节点)
            RuntimeMeta fMeta;
            fMeta.rating = json.folder().rating;
            fMeta.color = json.folder().color;
            fMeta.pinned = json.folder().pinned;
            fMeta.note = json.folder().note;
            for (const auto& t : json.folder().tags) fMeta.tags << QString::fromStdWString(t);
            m_cache[nDirPath] = std::move(fMeta);

            // 2. 加载子项元数据 (items 节点)
            for (auto& pair : json.items()) {
                QString fullPath = QDir(QString::fromStdWString(nDirPath)).filePath(QString::fromStdWString(pair.first));
                std::wstring wFullPath = normalizePath(fullPath.toStdWString());
                
                RuntimeMeta meta;
                meta.rating = pair.second.rating;
                meta.color = pair.second.color;
                meta.pinned = pair.second.pinned;
                meta.encrypted = pair.second.encrypted;
                meta.note = pair.second.note;
                for (const auto& t : pair.second.tags) meta.tags << QString::fromStdWString(t);
                
                m_cache[wFullPath] = std::move(meta);
            }
        }
    });
}

void MetadataManager::setRating(const std::wstring& path, int rating) {
    std::wstring nPath = normalizePath(path);
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[nPath].rating = rating;
    }
    emit metaChanged(nPath);
    persistAsync(nPath);
}

void MetadataManager::setColor(const std::wstring& path, const std::wstring& color) {
    std::wstring nPath = normalizePath(path);
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[nPath].color = color;
    }
    emit metaChanged(nPath);
    persistAsync(nPath);
}

void MetadataManager::setPinned(const std::wstring& path, bool pinned) {
    std::wstring nPath = normalizePath(path);
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[nPath].pinned = pinned;
    }
    emit metaChanged(nPath);
    persistAsync(nPath);
}

void MetadataManager::setTags(const std::wstring& path, const QStringList& tags) {
    std::wstring nPath = normalizePath(path);
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[nPath].tags = tags;
    }
    emit metaChanged(nPath);
    persistAsync(nPath);
}

void MetadataManager::setNote(const std::wstring& path, const std::wstring& note) {
    std::wstring nPath = normalizePath(path);
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[nPath].note = note;
    }
    emit metaChanged(nPath);
    persistAsync(nPath);
}

void MetadataManager::setEncrypted(const std::wstring& path, bool encrypted) {
    std::wstring nPath = normalizePath(path);
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[nPath].encrypted = encrypted;
    }
    emit metaChanged(nPath);
    persistAsync(nPath);
}

void MetadataManager::renameItem(const std::wstring& oldPath, const std::wstring& newPath) {
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_cache.find(oldPath);
        if (it != m_cache.end()) {
            m_cache[newPath] = std::move(it->second);
            m_cache.erase(it);
        }
    }
    emit metaChanged(newPath);
}

void MetadataManager::persistAsync(const std::wstring& path) {
    QThreadPool::globalInstance()->start([this, path]() {
        RuntimeMeta meta = getMeta(path);
        
        QString qPath = QDir::cleanPath(QString::fromStdWString(path));
        qPath = QDir::toNativeSeparators(qPath);
        QFileInfo info(qPath);
        
        std::wstring parentDir;
        std::wstring fileName;

        if (info.isRoot() || (qPath.length() <= 3 && qPath.endsWith(":\\"))) {
            parentDir = qPath.toStdWString();
            fileName = L""; 
        } else {
            parentDir = QDir::toNativeSeparators(info.absolutePath()).toStdWString();
            fileName = info.fileName().toStdWString();
        }

        std::wstring volSerial = getVolumeSerialNumber(path);

        AmMetaJson json(parentDir);
        json.load();

        if (info.isDir() && !fileName.empty()) {
            AmMetaJson selfJson(path);
            selfJson.load();
            auto& fMeta = selfJson.folder();
            fMeta.rating = meta.rating;
            fMeta.color = meta.color;
            fMeta.pinned = meta.pinned;
            fMeta.note = meta.note;
            fMeta.encrypted = meta.encrypted;
            fMeta.tags.clear();
            for (const auto& t : meta.tags) fMeta.tags.push_back(t.toStdWString());
            selfJson.save();
            SyncQueue::instance().enqueue(path);
        }

        if (fileName.empty()) {
            auto& fMeta = json.folder();
            fMeta.rating = meta.rating;
            fMeta.color = meta.color;
            fMeta.pinned = meta.pinned;
            fMeta.note = meta.note;
            fMeta.encrypted = meta.encrypted;
            fMeta.tags.clear();
            for (const auto& t : meta.tags) fMeta.tags.push_back(t.toStdWString());
            json.save();
            SyncQueue::instance().enqueue(parentDir);
            return;
        }

        auto& itemMeta = json.items()[fileName];
        itemMeta.volume = volSerial; 
        if (info.isDir()) {
            itemMeta.type = L"folder";
        } else {
            itemMeta.type = L"file";
        }
        itemMeta.rating = meta.rating;
        itemMeta.color = meta.color;
        itemMeta.pinned = meta.pinned;
        itemMeta.encrypted = meta.encrypted;
        itemMeta.note = meta.note;
        itemMeta.tags.clear();
        for (const auto& t : meta.tags) itemMeta.tags.push_back(t.toStdWString());
        json.save();

        SyncQueue::instance().enqueue(parentDir);
    });
}

} // namespace ArcMeta
