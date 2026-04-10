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

namespace ArcMeta {

MetadataManager& MetadataManager::instance() {
    static MetadataManager inst;
    return inst;
}

MetadataManager::MetadataManager(QObject* parent) : QObject(parent) {}

void MetadataManager::initFromDatabase() {
    // 2026-03-xx 架构重构：采用“双缓冲”加载策略。
    // 先在本地容器加载数据，仅在最终交换时持有写锁，彻底消除主线程查询时的长时间阻塞（死锁）。
    std::unordered_map<std::wstring, RuntimeMeta> tempCache;

    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    if (!db.isOpen()) return;

    QSqlQuery query(db);
    // 仅载入有元数据的项，减少内存占用
    query.exec("SELECT path, rating, color, tags, pinned, encrypted FROM items WHERE rating > 0 OR color != '' OR tags != '' OR pinned = 1 OR encrypted = 1");
    
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

        tempCache[path] = std::move(meta);
    }

    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache = std::move(tempCache);
    }

    // 加载完成后，触发全局刷新信号，补全 UI 界面
    emit metaChanged(L"__RELOAD_ALL__");
}

RuntimeMeta MetadataManager::getMeta(const std::wstring& path) {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_cache.find(path);
    if (it != m_cache.end()) return it->second;
    return RuntimeMeta(); // 返回默认空元数据
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

void MetadataManager::setEncrypted(const std::wstring& path, bool encrypted) {
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache[path].encrypted = encrypted;
    }
    emit metaChanged(path);
    persistAsync(path);
}

void MetadataManager::persistAsync(const std::wstring& path) {
    // 异步链式持久化逻辑
    // 2026-03-xx 按照编译器建议：使用 QThreadPool::start 替代 QtConcurrent::run 以消除返回值丢弃警告
    QThreadPool::globalInstance()->start([this, path]() {
        RuntimeMeta meta = getMeta(path);
        QFileInfo info(QString::fromStdWString(path));
        std::wstring parentDir = info.absolutePath().toStdWString();
        std::wstring fileName = info.fileName().toStdWString();

        // 1. 同步到 JSON 文件
        AmMetaJson json(parentDir);
        json.load();
        auto& itemMeta = json.items()[fileName];
        itemMeta.rating = meta.rating;
        itemMeta.color = meta.color;
        itemMeta.pinned = meta.pinned;
        itemMeta.encrypted = meta.encrypted;
        itemMeta.tags.clear();
        for (const auto& t : meta.tags) itemMeta.tags.push_back(t.toStdWString());
        json.save();

        // 2. 触发 SyncQueue 同步到 DB (SyncQueue 会调用 ItemRepo)
        SyncQueue::instance().enqueue(parentDir);
    });
}

} // namespace ArcMeta
