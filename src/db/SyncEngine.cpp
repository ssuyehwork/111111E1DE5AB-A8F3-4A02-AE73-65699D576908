#include "SyncEngine.h"
#include "Database.h"
#include "../meta/AmMetaJson.h"
#include <QDirIterator>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

namespace ArcMeta {

SyncEngine::SyncEngine(QObject* parent) : QObject(parent) {}

void SyncEngine::fullScan(const QString& rootPath, std::function<void(int current, int total)> progressCallback) {
    QDirIterator it(rootPath, QStringList() << ".am_meta.json", QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);

    QStringList metaFiles;
    while (it.hasNext()) {
        metaFiles.append(it.next());
    }

    int total = metaFiles.size();
    int current = 0;

    QSqlDatabase db = Database::instance().getDb();
    db.transaction();

    for (const QString& filePath : metaFiles) {
        QString dirPath = QFileInfo(filePath).absolutePath();
        syncDirectory(dirPath);

        current++;
        if (progressCallback) {
            progressCallback(current, total);
        }
    }

    db.commit();
    rebuildTagIndex();
}

bool SyncEngine::syncDirectory(const QString& dirPath) {
    AmMeta meta = AmMetaJson::load(dirPath);
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);

    // 1. 更新 folders 表
    query.prepare(
        "INSERT OR REPLACE INTO folders (path, rating, color, tags, pinned, note, sort_by, sort_order, last_sync) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"
    );
    query.addBindValue(dirPath);
    query.addBindValue(meta.folder.rating);
    query.addBindValue(meta.folder.color);
    query.addBindValue(QJsonDocument(QJsonArray::fromStringList(meta.folder.tags)).toJson(QJsonDocument::Compact));
    query.addBindValue(meta.folder.pinned ? 1 : 0);
    query.addBindValue(meta.folder.note);
    query.addBindValue(meta.folder.sortBy);
    query.addBindValue(meta.folder.sortOrder);
    query.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());

    if (!query.exec()) {
        qDebug() << "SyncEngine: 更新 folders 失败 -" << query.lastError().text();
        return false;
    }

    // 2. 更新 items 表 (基于当前文件夹下的项目)
    // 首先删除该路径下旧的所有 item 记录（简单处理一致性）
    QSqlQuery delQuery(db);
    delQuery.prepare("DELETE FROM items WHERE parent_path = ?");
    delQuery.addBindValue(dirPath);
    delQuery.exec();

    for (auto it = meta.items.begin(); it != meta.items.end(); ++it) {
        const QString& fileName = it.key();
        const ItemMetadata& item = it.value();
        QString fullItemPath = QDir(dirPath).filePath(fileName);

        query.prepare(
            "INSERT OR REPLACE INTO items (path, frn, type, rating, color, tags, pinned, note, encrypted, encrypt_salt, encrypt_verify_hash, original_name, parent_path) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        );
        query.addBindValue(fullItemPath);
        query.addBindValue(static_cast<qint64>(item.frn));
        query.addBindValue(item.type);
        query.addBindValue(item.rating);
        query.addBindValue(item.color);
        query.addBindValue(QJsonDocument(QJsonArray::fromStringList(item.tags)).toJson(QJsonDocument::Compact));
        query.addBindValue(item.pinned ? 1 : 0);
        query.addBindValue(item.note);
        query.addBindValue(item.encrypted ? 1 : 0);
        query.addBindValue(item.encryptSalt);
        query.addBindValue(item.encryptVerifyHash);
        query.addBindValue(item.originalName);
        query.addBindValue(dirPath);

        if (!query.exec()) {
            qDebug() << "SyncEngine: 更新 items 失败 -" << query.lastError().text();
        }
    }

    return true;
}

void SyncEngine::onSyncRequested(const QStringList& dirPaths) {
    QSqlDatabase db = Database::instance().getDb();
    db.transaction();

    for (const QString& path : dirPaths) {
        syncDirectory(path);
    }

    db.commit();
    rebuildTagIndex();
}

void SyncEngine::rebuildTagIndex() {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);

    // 清空旧标签统计
    query.exec("DELETE FROM tags");

    // 聚合统计（从 items 表）
    query.exec("SELECT tags FROM items WHERE tags != '[]'");
    QMap<QString, int> tagCounts;
    while (query.next()) {
        QJsonDocument doc = QJsonDocument::fromJson(query.value(0).toByteArray());
        QJsonArray arr = doc.array();
        for (const auto& val : arr) {
            tagCounts[val.toString()]++;
        }
    }

    // 写入 tags 表
    db.transaction();
    for (auto it = tagCounts.begin(); it != tagCounts.end(); ++it) {
        QSqlQuery insert(db);
        insert.prepare("INSERT INTO tags (tag, item_count) VALUES (?, ?)");
        insert.addBindValue(it.key());
        insert.addBindValue(it.value());
        insert.exec();
    }
    db.commit();
}

} // namespace ArcMeta
