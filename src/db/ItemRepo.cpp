#include "ItemRepo.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>

namespace ArcMeta {

bool ItemRepo::save(const std::wstring& parentPath, const std::wstring& name, const ItemMeta& meta) {
    QSqlQuery q;
    q.prepare(R"sql(
        INSERT OR REPLACE INTO items
        (volume, frn, path, parent_path, type, rating, color, tags, pinned, note,
         encrypted, encrypt_salt, encrypt_iv, encrypt_verify_hash, original_name)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
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

    return q.exec();
}

bool ItemRepo::markAsDeleted(const std::wstring& volume, const std::wstring& frn) {
    QSqlQuery q;
    q.prepare("UPDATE items SET deleted = 1 WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    return q.exec();
}

bool ItemRepo::removeByFrn(const std::wstring& volume, const std::wstring& frn) {
    QSqlQuery q;
    q.prepare("DELETE FROM items WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    return q.exec();
}

std::wstring ItemRepo::getPathByFrn(const std::wstring& volume, const std::wstring& frn) {
    QSqlQuery q;
    q.prepare("SELECT path FROM items WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    if (q.exec() && q.next()) {
        return q.value(0).toString().toStdWString();
    }
    return L"";
}

bool ItemRepo::updatePath(const std::wstring& volume, const std::wstring& frn, const std::wstring& newPath) {
    QSqlQuery q;
    q.prepare("UPDATE items SET path = ? WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(newPath));
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    return q.exec();
}

} // namespace ArcMeta
