#include "ItemRepo.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>

namespace ArcMeta {

bool ItemRepo::save(const std::wstring& parentPath, const std::wstring& name, const ItemMeta& meta, QSqlDatabase db) {
    QSqlQuery q(db);
    q.prepare(R"sql(
        INSERT OR REPLACE INTO items 
        (volume, frn, path, parent_path, type, rating, color, tags, pinned, note, 
         encrypted, encrypt_salt, encrypt_iv, encrypt_verify_hash, original_name, size, mtime, ctime)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )sql");

    std::wstring fullPath = parentPath;
    if (!fullPath.empty() && fullPath.back() != L'\\' && fullPath.back() != L'/') fullPath += L'\\';
    fullPath += name;

    q.addBindValue(QString::fromStdWString(meta.volume));
    q.addBindValue(QString::fromStdWString(meta.frn));
    q.addBindValue(QString::fromStdWString(fullPath));
    q.addBindValue(QString::fromStdWString(parentPath));
    q.addBindValue(QString::fromStdWString(meta.type));
    q.addBindValue(meta.rating);
    q.addBindValue(QString::fromStdWString(meta.color));

    QJsonArray tagsArr;
    for (const auto& t : meta.tags) tagsArr.append(QString::fromStdWString(t));
    q.addBindValue(QJsonDocument(tagsArr).toJson(QJsonDocument::Compact));

    q.addBindValue(meta.pinned ? 1 : 0);
    q.addBindValue(QString::fromStdWString(meta.note));
    q.addBindValue(meta.encrypted ? 1 : 0);
    q.addBindValue(QString::fromStdString(meta.encryptSalt));
    q.addBindValue(QString::fromStdString(meta.encryptIv));
    q.addBindValue(QString::fromStdString(meta.encryptVerifyHash));
    q.addBindValue(QString::fromStdWString(meta.originalName));
    q.addBindValue((qlonglong)meta.size);
    q.addBindValue(meta.mtime);
    q.addBindValue(meta.ctime);

    return q.exec();
}

bool ItemRepo::markAsDeleted(const std::wstring& volume, const std::wstring& frn, QSqlDatabase db) {
    QSqlQuery q(db);
    q.prepare("UPDATE items SET deleted = 1 WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    return q.exec();
}

bool ItemRepo::removeByFrn(const std::wstring& volume, const std::wstring& frn, QSqlDatabase db) {
    QSqlQuery q(db);
    q.prepare("DELETE FROM items WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    return q.exec();
}

std::wstring ItemRepo::getPathByFrn(const std::wstring& volume, const std::wstring& frn, QSqlDatabase db) {
    QSqlQuery q(db);
    q.prepare("SELECT path FROM items WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    if (q.exec() && q.next()) {
        return q.value(0).toString().toStdWString();
    }
    return L"";
}

bool ItemRepo::updatePath(const std::wstring& volume, const std::wstring& frn, const std::wstring& newPath, const std::wstring& newParentPath, QSqlDatabase db) {
    QSqlQuery q(db);
    q.prepare("UPDATE items SET path = ?, parent_path = ? WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(newPath));
    q.addBindValue(QString::fromStdWString(newParentPath));
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    return q.exec();
}

std::map<std::wstring, ItemMeta> ItemRepo::getMetadataBatch(const std::wstring& parentPath, QSqlDatabase db) {
    std::map<std::wstring, ItemMeta> results;
    QSqlQuery q(db);
    // 关键红线：利用 idx_items_parent 索引执行 O(log N) 范围扫描，并一次性获取所有显示属性 (Zero-IO)
    q.prepare("SELECT path, rating, color, tags, pinned, encrypted, type, size, mtime, ctime FROM items WHERE parent_path = ?");
    q.addBindValue(QString::fromStdWString(parentPath));

    if (q.exec()) {
        while (q.next()) {
            QString fullPath = q.value(0).toString();
            QString name = QFileInfo(fullPath).fileName();

            ItemMeta meta;
            meta.rating = q.value(1).toInt();
            meta.color = q.value(2).toString().toStdWString();

            QJsonDocument doc = QJsonDocument::fromJson(q.value(3).toByteArray());
            if (doc.isArray()) {
                for (const auto& v : doc.array()) meta.tags.push_back(v.toString().toStdWString());
            }

            meta.pinned = q.value(4).toBool();
            meta.encrypted = q.value(5).toBool();
            meta.type = q.value(6).toString().toStdWString();
            meta.size = q.value(7).toLongLong();
            meta.mtime = q.value(8).toDouble();
            meta.ctime = q.value(9).toDouble();

            results[name.toStdWString()] = meta;
        }
    }
    return results;
}

} // namespace ArcMeta
