#include "MetadataManager.h"
#include "AmMetaJson.h"
#include "SyncQueue.h"
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
    // 2026-05-22 按照用户要求：彻底废除数据库，此函数现在仅打印日志并重置缓存
    qDebug() << "[MetadataManager] 数据库已废除，跳过初始化。";
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache.clear();
    }
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
    // 2026-04-12 按照用户最新铁律：支持虚拟路径 computer://
    std::wstring nDirPath = (dirPath == L"computer://") ? dirPath : normalizePath(dirPath);
    qDebug() << "[MetadataManager] 预加载目录元数据(纯 JSON 模式):" << QString::fromStdWString(nDirPath);
    
    QThreadPool::globalInstance()->start([this, nDirPath]() {
        AmMetaJson json(nDirPath);
        
        // 2026-04-10 按照用户铁律：访问即生成
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
                QString fullPath;
                if (nDirPath == L"computer://") {
                    fullPath = QString::fromStdWString(pair.first);
                    if (!fullPath.endsWith('\\')) fullPath += '\\';
                } else {
                    fullPath = QDir(QString::fromStdWString(nDirPath)).filePath(QString::fromStdWString(pair.first));
                }
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
        
        std::wstring parentDir;
        std::wstring fileName;

        if (info.isRoot() || (qPath.length() <= 3 && qPath.endsWith(":\\"))) {
            parentDir = L"computer://";
            QString driveName = qPath;
            if (driveName.endsWith('\\')) driveName.chop(1);
            fileName = driveName.toStdWString();
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
            // 2026-05-22 按照用户要求：废除数据库，无需向 SyncQueue 入队（SyncQueue 目前也已去数据库化）
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
    });
}

} // namespace ArcMeta
