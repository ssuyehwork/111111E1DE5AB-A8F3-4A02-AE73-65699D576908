#include "MetadataManager.h"
#include "../db/Database.h"
#include "../db/ItemRepo.h"
#include "../db/FolderRepo.h"
#include "MetadataDefs.h"
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
#include <QTimer>
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
 * @brief 内部辅助：获取相对路径
 */
static std::wstring getRelativePath(const std::wstring& fullPath) {
    if (fullPath.length() >= 3 && fullPath[1] == L':' && fullPath[2] == L'\\') {
        return fullPath.substr(2);
    }
    return fullPath;
}

/**
 * @brief 内部辅助：标准化路径
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
    qDebug() << "[MetadataManager] 开始从数据库加载内存镜像...";
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
    int folderCount = 0;
    loadTable("SELECT volume, path, rating, color, tags, pinned, encrypted, note FROM folders WHERE rating > 0 OR color != '' OR tags != '' OR pinned = 1 OR encrypted = 1 OR note != ''", folderCount);

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
    return RuntimeMeta();
}

void MetadataManager::prefetchDirectory(const std::wstring& dirPath) {
    std::wstring nDirPath = (dirPath == L"computer://") ? dirPath : normalizePath(dirPath);
    qDebug() << "[MetadataManager] 预加载目录元数据(中心化模式):" << QString::fromStdWString(nDirPath);
}

void MetadataManager::setRating(const std::wstring& path, int rating) {
    std::wstring nPath = normalizePath(path);
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[nPath].rating = rating;
    }
    emit metaChanged(nPath);
    debouncePersist(nPath);
}

void MetadataManager::setColor(const std::wstring& path, const std::wstring& color) {
    std::wstring nPath = normalizePath(path);
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[nPath].color = color;
    }
    emit metaChanged(nPath);
    debouncePersist(nPath);
}

void MetadataManager::setPinned(const std::wstring& path, bool pinned) {
    std::wstring nPath = normalizePath(path);
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[nPath].pinned = pinned;
    }
    emit metaChanged(nPath);
    debouncePersist(nPath);
}

void MetadataManager::setTags(const std::wstring& path, const QStringList& tags) {
    std::wstring nPath = normalizePath(path);
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[nPath].tags = tags;
    }
    emit metaChanged(nPath);
    debouncePersist(nPath);
}

void MetadataManager::setNote(const std::wstring& path, const std::wstring& note) {
    std::wstring nPath = normalizePath(path);
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[nPath].note = note;
    }
    emit metaChanged(nPath);
    debouncePersist(nPath);
}

void MetadataManager::setEncrypted(const std::wstring& path, bool encrypted) {
    std::wstring nPath = normalizePath(path);
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[nPath].encrypted = encrypted;
    }
    emit metaChanged(nPath);
    debouncePersist(nPath);
}

void MetadataManager::debouncePersist(const std::wstring& nPath) {
    QMetaObject::invokeMethod(this, [this, nPath]() {
        if (!m_debounceTimers.count(nPath)) {
            QTimer* t = new QTimer(this);
            t->setSingleShot(true);
            t->setInterval(2000);
            connect(t, &QTimer::timeout, [this, nPath]() {
                persistAsync(nPath);
                m_debounceTimers[nPath]->deleteLater();
                m_debounceTimers.erase(nPath);
            });
            m_debounceTimers[nPath] = t;
        }
        m_debounceTimers[nPath]->start();
    }, Qt::QueuedConnection);
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
        std::wstring volSerial = getVolumeSerialNumber(path);

        if (info.isDir()) {
            FolderMeta fMeta;
            fMeta.rating = meta.rating;
            fMeta.color = meta.color;
            fMeta.pinned = meta.pinned;
            fMeta.note = meta.note;
            fMeta.encrypted = meta.encrypted;
            for (const auto& t : meta.tags) fMeta.tags.push_back(t.toStdWString());
            FolderRepo::save(volSerial, getRelativePath(path), fMeta);
        }

        ItemMeta iMeta;
        iMeta.volume = volSerial;
        iMeta.type = info.isDir() ? L"folder" : L"file";
        iMeta.rating = meta.rating;
        iMeta.color = meta.color;
        iMeta.pinned = meta.pinned;
        iMeta.encrypted = meta.encrypted;
        iMeta.note = meta.note;
        for (const auto& t : meta.tags) iMeta.tags.push_back(t.toStdWString());

        std::wstring parentDir = QDir::toNativeSeparators(info.absolutePath()).toStdWString();
        std::wstring fileName = info.fileName().toStdWString();

        if (info.isRoot() || (qPath.length() <= 3 && qPath.endsWith(":\\"))) {
            parentDir = L"computer://";
            QString driveName = qPath;
            if (driveName.endsWith('\\')) driveName.chop(1);
            fileName = driveName.toStdWString();
        }

        ItemRepo::save(parentDir, fileName, iMeta);
    });
}

} // namespace ArcMeta
