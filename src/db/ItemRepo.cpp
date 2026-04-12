#include "ItemRepo.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QStringList>
#include "../meta/MetadataDefs.h"

namespace ArcMeta {

bool ItemRepo::save(const std::wstring& parentPath, const std::wstring& name, const ItemMeta& meta) {
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    
    std::wstring fullPath = parentPath;
    if (!fullPath.empty() && fullPath.back() != L'\\' && fullPath.back() != L'/') fullPath += L'\\';
    fullPath += name;

    std::wstring volume = meta.volume;
    std::wstring frn = meta.frn;

    if (volume.empty() || frn.empty()) {
        QSqlQuery check(db);
        check.prepare("SELECT volume, frn FROM items WHERE path = ?");
        check.addBindValue(QString::fromStdWString(fullPath));
        if (check.exec() && check.next()) {
            if (volume.empty()) volume = check.value(0).toString().toStdWString();
            if (frn.empty()) frn = check.value(1).toString().toStdWString();
        }
    }

    if (volume.empty() || frn.empty()) {
        volume = L"VIRTUAL";
        frn = QString(QCryptographicHash::hash(QString::fromStdWString(fullPath).toUtf8(), QCryptographicHash::Md5).toHex()).toStdWString();
    }

    QSqlQuery q(db);
    q.prepare(R"sql(
        INSERT OR REPLACE INTO items 
        (volume, frn, path, parent_path, type, rating, color, tags, pinned, note, 
         encrypted, encrypt_salt, encrypt_iv, encrypt_verify_hash, original_name) 
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )sql");

    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
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

    return q.exec();
}

bool ItemRepo::saveBasicInfo(const std::wstring& volume, const std::wstring& frn, const std::wstring& path, const std::wstring& parentPath, bool isDir, double mtime, double size) {
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    QSqlQuery q(db);

    q.prepare(R"sql(
        INSERT OR IGNORE INTO items (volume, frn, path, parent_path, type)
        VALUES (?, ?, ?, ?, ?)
    )sql");
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    q.addBindValue(QString::fromStdWString(path));
    q.addBindValue(QString::fromStdWString(parentPath));
    q.addBindValue(isDir ? "folder" : "file");
    q.exec();

    QSqlQuery u(db);
    u.prepare("UPDATE items SET path = ?, parent_path = ?, mtime = ?, deleted = 0 WHERE volume = ? AND frn = ?");
    u.addBindValue(QString::fromStdWString(path));
    u.addBindValue(QString::fromStdWString(parentPath));
    u.addBindValue(mtime);
    u.addBindValue(QString::fromStdWString(volume));
    u.addBindValue(QString::fromStdWString(frn));
    Q_UNUSED(size);
    return u.exec();
}

bool ItemRepo::markAsDeleted(const std::wstring& volume, const std::wstring& frn) {
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    QSqlQuery q(db);
    q.prepare("UPDATE items SET deleted = 1 WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    return q.exec();
}

bool ItemRepo::removeByFrn(const std::wstring& volume, const std::wstring& frn) {
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    QSqlQuery q(db);
    q.prepare("DELETE FROM items WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    return q.exec();
}

std::wstring ItemRepo::getPathByFrn(const std::wstring& volume, const std::wstring& frn) {
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    QSqlQuery q(db);
    q.prepare("SELECT path FROM items WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    if (q.exec() && q.next()) {
        return q.value(0).toString().toStdWString();
    }
    return L"";
}

bool ItemRepo::updatePath(const std::wstring& volume, const std::wstring& frn, const std::wstring& newPath, const std::wstring& newParentPath) {
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    QSqlQuery q(db);
    q.prepare("UPDATE items SET path = ?, parent_path = ? WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(newPath));
    q.addBindValue(QString::fromStdWString(newParentPath));
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    return q.exec();
}

QStringList ItemRepo::searchByKeyword(const QString& keyword, const QString& parentPath) {
    if (keyword.isEmpty()) return {};
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    QSqlQuery q(db);

    if (parentPath.isEmpty()) {
        q.prepare("SELECT path FROM items WHERE path LIKE ? AND deleted = 0 LIMIT 300");
        q.addBindValue("%" + keyword + "%");
    } else {
        q.prepare("SELECT path FROM items WHERE parent_path = ? AND path LIKE ? AND deleted = 0 LIMIT 300");
        q.addBindValue(parentPath);
        q.addBindValue("%" + keyword + "%");
    }

    QStringList results;
    if (q.exec()) {
        while (q.next()) {
            results << q.value(0).toString();
        }
    }
    return results;
}

} // namespace ArcMeta
