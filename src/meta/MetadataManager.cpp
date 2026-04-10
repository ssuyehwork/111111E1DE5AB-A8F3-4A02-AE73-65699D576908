#include "MetadataManager.h"
#include "../db/Database.h"
#include "../db/ItemRepo.h"
#include "AmMetaJson.h"
#include "SyncQueue.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QtConcurrent>
#include <QThreadPool>
#include <QDir>
#include "AmMetaJson.h"

namespace ArcMeta {

MetadataManager& MetadataManager::instance() {
    static MetadataManager inst;
    return inst;
}

MetadataManager::MetadataManager(QObject* parent) : QObject(parent) {}

void MetadataManager::initFromDatabase() {
    qDebug() << "[MetadataManager] 开始从数据库加载内存镜像...";
    // 2026-03-xx 架构重构：采用“双缓冲”加载策略。
    // 先在本地容器加载数据，仅在最终交换时持有写锁，彻底消除主线程查询时的长时间阻塞（死锁）。
    std::unordered_map<std::wstring, RuntimeMeta> tempCache;

    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    if (!db.isOpen()) {
        qCritical() << "[MetadataManager] 数据库未打开，加载失败";
        return;
    }

    QSqlQuery query(db);
    // 1. 加载条目元数据
    query.exec("SELECT path, rating, color, tags, pinned, encrypted, note FROM items WHERE rating > 0 OR color != '' OR tags != '' OR pinned = 1 OR encrypted = 1 OR note != ''");
    int itemCount = 0;
    while (query.next()) {
        std::wstring path = query.value(0).toString().toStdWString();
        RuntimeMeta meta;
        meta.rating = query.value(1).toInt();
        meta.color = query.value(2).toString().toStdWString();
        QJsonDocument doc = QJsonDocument::fromJson(query.value(3).toByteArray());
        if (doc.isArray()) {
            for (const auto& v : doc.array()) meta.tags << v.toString();
        }
        meta.pinned = query.value(4).toInt() != 0;
        meta.encrypted = query.value(5).toInt() != 0;
        meta.note = query.value(6).toString().toStdWString();
        tempCache[path] = std::move(meta);
        itemCount++;
    }
    qDebug() << "[MetadataManager] 已从 items 表加载" << itemCount << "条记录";

    // 2. 加载文件夹元数据（修复启动后文件夹星级等消失的关键补丁）
    query.exec("SELECT path, rating, color, tags, pinned, encrypted, note FROM folders WHERE rating > 0 OR color != '' OR tags != '' OR pinned = 1 OR encrypted = 1 OR note != ''");
    int folderCount = 0;
    while (query.next()) {
        std::wstring path = query.value(0).toString().toStdWString();
        RuntimeMeta meta;
        meta.rating = query.value(1).toInt();
        meta.color = query.value(2).toString().toStdWString();
        QJsonDocument doc = QJsonDocument::fromJson(query.value(3).toByteArray());
        if (doc.isArray()) {
            for (const auto& v : doc.array()) meta.tags << v.toString();
        }
        meta.pinned = query.value(4).toInt() != 0;
        meta.encrypted = query.value(5).toInt() != 0;
        meta.note = query.value(6).toString().toStdWString();
        tempCache[path] = std::move(meta);
        folderCount++;
    }
    qDebug() << "[MetadataManager] 已从 folders 表加载" << folderCount << "条记录";

    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache = std::move(tempCache);
    }
    qDebug() << "[MetadataManager] 内存镜像初始化完成";
    
    // 加载完成后，触发全局刷新信号，补全 UI 界面
    emit metaChanged(L"__RELOAD_ALL__");
}

RuntimeMeta MetadataManager::getMeta(const std::wstring& path) {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_cache.find(path);
    if (it != m_cache.end()) return it->second;
    return RuntimeMeta(); // 返回默认空元数据
}

void MetadataManager::prefetchDirectory(const std::wstring& dirPath) {
    qDebug() << "[MetadataManager] 预加载目录元数据:" << QString::fromStdWString(dirPath);
    // 2026-03-xx 性能预判：在后台预读指定文件夹的 JSON 元数据，消除点击时的微小顿挫
    // 修复：由于 MetadataManager 是单例，this 始终有效，但为保持代码严谨性仍进行常规检查
    QThreadPool::globalInstance()->start([this, dirPath]() {
        AmMetaJson json(dirPath);
        if (json.load()) {
            qDebug() << "[MetadataManager] 加载 JSON 成功:" << QString::fromStdWString(json.getMetaFilePath());
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            QString qDir = QDir::cleanPath(QString::fromStdWString(dirPath));
            qDir = QDir::toNativeSeparators(qDir);
            if (qDir.length() == 2 && qDir.endsWith(':')) qDir += '\\';
            
            // 1. 加载文件夹自身的元数据 (folder 节点)
            RuntimeMeta fMeta;
            fMeta.rating = json.folder().rating;
            fMeta.color = json.folder().color;
            fMeta.pinned = json.folder().pinned;
            fMeta.note = json.folder().note;
            for (const auto& t : json.folder().tags) fMeta.tags << QString::fromStdWString(t);
            m_cache[qDir.toStdWString()] = std::move(fMeta);

            // 2. 加载子项元数据 (items 节点)
            for (auto& pair : json.items()) {
                // 修复：使用 QDir::filePath 拼接路径，解决根目录双斜杠 Bug
                QString fullPath = QDir(qDir).filePath(QString::fromStdWString(pair.first));
                std::wstring wFullPath = QDir::toNativeSeparators(QDir::cleanPath(fullPath)).toStdWString();
                
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
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[path].rating = rating;
    }
    emit metaChanged(path);
    persistAsync(path);
}

void MetadataManager::setColor(const std::wstring& path, const std::wstring& color) {
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[path].color = color;
    }
    emit metaChanged(path);
    persistAsync(path);
}

void MetadataManager::setPinned(const std::wstring& path, bool pinned) {
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[path].pinned = pinned;
    }
    emit metaChanged(path);
    persistAsync(path);
}

void MetadataManager::setTags(const std::wstring& path, const QStringList& tags) {
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[path].tags = tags;
    }
    emit metaChanged(path);
    persistAsync(path);
}

void MetadataManager::setNote(const std::wstring& path, const std::wstring& note) {
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[path].note = note;
    }
    emit metaChanged(path);
    persistAsync(path);
}

void MetadataManager::setEncrypted(const std::wstring& path, bool encrypted) {
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[path].encrypted = encrypted;
    }
    emit metaChanged(path);
    persistAsync(path);
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
    qDebug() << "[MetadataManager] 请求异步持久化:" << QString::fromStdWString(path);
    // 异步链式持久化逻辑
    // 2026-03-xx 按照编译器建议：使用 QThreadPool::start 替代 QtConcurrent::run 以消除返回值丢弃警告
    QThreadPool::globalInstance()->start([this, path]() {
        RuntimeMeta meta = getMeta(path);
        
        // 2026-03-xx 修复文件夹记录丢失：针对根目录文件夹进行路径健壮性处理
        QString qPath = QDir::cleanPath(QString::fromStdWString(path));
        qPath = QDir::toNativeSeparators(qPath);
        qDebug() << "[MetadataManager] 正在持久化路径:" << qPath << "星级:" << meta.rating << "颜色:" << QString::fromStdWString(meta.color);
        QFileInfo info(qPath);
        
        std::wstring parentDir;
        std::wstring fileName;

        if (info.isRoot() || (qPath.length() <= 3 && qPath.endsWith(":\\"))) {
            // 如果是根目录本身（如 G:\），其没有父目录，元数据存放在自身下
            parentDir = qPath.toStdWString();
            fileName = L""; // 根目录级别的元数据通常存放在 folder 节点
        } else {
            parentDir = QDir::toNativeSeparators(info.absolutePath()).toStdWString();
            fileName = info.fileName().toStdWString();
        }

        // 1. 同步到 JSON 文件
        AmMetaJson json(parentDir);
        json.load();

        if (info.isDir()) {
            // 如果是文件夹，更新其自身的 .am_meta.json (folder 节点)
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

            // 2026-04-10 关键修复：文件夹自身也需要加入同步队列，以更新数据库的 folders 表
            SyncQueue::instance().enqueue(path);
        }

        if (fileName.empty()) {
            // 针对根目录的情况，更新并保存当前的 json（即自身的 json）
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

        // 更新父目录 JSON 中的条目记录
        auto& itemMeta = json.items()[fileName];
        // 2026-04-10 修复：显式修正类型，防止文件夹被误认为文件
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

        // 2. 触发 SyncQueue 同步到 DB (SyncQueue 会调用 ItemRepo)
        SyncQueue::instance().enqueue(parentDir);
    });
}

} // namespace ArcMeta
