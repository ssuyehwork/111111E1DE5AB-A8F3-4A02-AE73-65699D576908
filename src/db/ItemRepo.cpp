#include "ItemRepo.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QString>

namespace ArcMeta {

bool ItemRepo::save(const std::wstring& parentPath, const std::wstring& name, const ItemMeta& meta) {
    try {
        auto sqlite = Database::instance().sqlite();
        SQLite::Statement query(*sqlite, R"sql(
            INSERT OR REPLACE INTO items
            (volume, frn, path, parent_path, type, rating, color, tags, pinned, note,
             encrypted, encrypt_salt, encrypt_iv, encrypt_verify_hash, original_name)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )sql");

        std::wstring fullPath = parentPath;
        if (!fullPath.empty() && fullPath.back() != L'\\' && fullPath.back() != L'/') fullPath += L'\\';
        fullPath += name;

        query.bind(1, QString::fromStdWString(meta.volume).toStdString());
        query.bind(2, QString::fromStdWString(meta.frn).toStdString());
        query.bind(3, QString::fromStdWString(fullPath).toStdString());
        query.bind(4, QString::fromStdWString(parentPath).toStdString());
        query.bind(5, QString::fromStdWString(meta.type).toStdString());
        query.bind(6, meta.rating);
        query.bind(7, QString::fromStdWString(meta.color).toStdString());

        QJsonArray tagsArr;
        for (const auto& t : meta.tags) tagsArr.append(QString::fromStdWString(t));
        query.bind(8, QJsonDocument(tagsArr).toJson(QJsonDocument::Compact).toStdString());

        query.bind(9, meta.pinned ? 1 : 0);
        query.bind(10, QString::fromStdWString(meta.note).toStdString());
        query.bind(11, meta.encrypted ? 1 : 0);
        query.bind(12, meta.encryptSalt);
        query.bind(13, meta.encryptIv);
        query.bind(14, meta.encryptVerifyHash);
        query.bind(15, QString::fromStdWString(meta.originalName).toStdString());

        return query.exec() > 0;
    } catch (...) {
        return false;
    }
}

bool ItemRepo::markAsDeleted(const std::wstring& volume, const std::wstring& frn) {
    try {
        auto sqlite = Database::instance().sqlite();
        SQLite::Statement query(*sqlite, "UPDATE items SET deleted = 1 WHERE volume = ? AND frn = ?");
        query.bind(1, QString::fromStdWString(volume).toStdString());
        query.bind(2, QString::fromStdWString(frn).toStdString());
        return query.exec() > 0;
    } catch (...) {
        return false;
    }
}

bool ItemRepo::removeByFrn(const std::wstring& volume, const std::wstring& frn) {
    try {
        auto sqlite = Database::instance().sqlite();
        SQLite::Statement query(*sqlite, "DELETE FROM items WHERE volume = ? AND frn = ?");
        query.bind(1, QString::fromStdWString(volume).toStdString());
        query.bind(2, QString::fromStdWString(frn).toStdString());
        return query.exec() > 0;
    } catch (...) {
        return false;
    }
}

} // namespace ArcMeta
