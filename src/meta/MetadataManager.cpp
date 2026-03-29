#include "MetadataManager.h"
#include "../db/Database.h"
#include "../db/ItemRepo.h"
#include "AmMetaJson.h"
#include "SyncQueue.h"
#include <QSqlQuery>
#include <QSqlRecord>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QtConcurrent>
#include <QThreadPool>
#include <QSqlDatabase>
#include <QUuid>

namespace ArcMeta {

MetadataManager& MetadataManager::instance() {
    static MetadataManager inst;
    return inst;
}

MetadataManager::MetadataManager(QObject* parent) : QObject(parent) {}

/**
 * @brief 2026-03-xx 按照 Qt 规范修复：在后台线程建立独立的数据库连接
 */
void MetadataManager::initFromDatabase() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_cache.clear();

    // 修复：QtSql 数据库连接不跨线程。在后台线程中必须创建带有唯一连接名的私有连接。
    QString connectionName = "MetadataInit_" + QUuid::createUuid().toString();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(QString::fromStdWString(Database::instance().getDbPath()));

        if (!db.open()) return;

        QSqlQuery query(db);
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

            m_cache[path] = std::move(meta);
        }
        db.close();
    }
    // 销毁作用域后移除临时连接名
    QSqlDatabase::removeDatabase(connectionName);
}

RuntimeMeta MetadataManager::getMeta(const std::wstring& path) {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_cache.find(path);
    if (it != m_cache.end()) return it->second;
    return RuntimeMeta();
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

void MetadataManager::persistAsync(const std::wstring& path) {
    QThreadPool::globalInstance()->start([this, path]() {
        RuntimeMeta meta = getMeta(path);
        QFileInfo info(QString::fromStdWString(path));
        std::wstring parentDir = info.absolutePath().toStdWString();
        std::wstring fileName = info.fileName().toStdWString();

        AmMetaJson json(parentDir);
        json.load();
        auto& itemMeta = json.items()[fileName];
        itemMeta.rating = meta.rating;
        itemMeta.color = meta.color;
        itemMeta.pinned = meta.pinned;
        itemMeta.tags.clear();
        for (const auto& t : meta.tags) itemMeta.tags.push_back(t.toStdWString());
        json.save();

        SyncQueue::instance().enqueue(parentDir);
    });
}

} // namespace ArcMeta
