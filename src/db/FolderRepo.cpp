#include "FolderRepo.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QString>

namespace ArcMeta {

bool FolderRepo::save(const std::wstring& path, const FolderMeta& meta) {
    try {
        auto sqlite = Database::instance().sqlite();
        SQLite::Statement query(*sqlite, "INSERT OR REPLACE INTO folders (path, rating, color, tags, pinned, note, sort_by, sort_order) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

        query.bind(1, QString::fromStdWString(path).toStdString());
        query.bind(2, meta.rating);
        query.bind(3, QString::fromStdWString(meta.color).toStdString());

        // 关键：标签存储为 JSON 数组字符串
        QJsonArray tagsArr;
        for (const auto& t : meta.tags) tagsArr.append(QString::fromStdWString(t));
        query.bind(4, QJsonDocument(tagsArr).toJson(QJsonDocument::Compact).toStdString());

        query.bind(5, meta.pinned ? 1 : 0);
        query.bind(6, QString::fromStdWString(meta.note).toStdString());
        query.bind(7, QString::fromStdWString(meta.sortBy).toStdString());
        query.bind(8, QString::fromStdWString(meta.sortOrder).toStdString());

        return query.exec() > 0;
    } catch (...) {
        return false;
    }
}

bool FolderRepo::get(const std::wstring& path, FolderMeta& meta) {
    try {
        auto sqlite = Database::instance().sqlite();
        SQLite::Statement query(*sqlite, "SELECT rating, color, tags, pinned, note, sort_by, sort_order FROM folders WHERE path = ?");
        query.bind(1, QString::fromStdWString(path).toStdString());

        if (query.executeStep()) {
            meta.rating = query.getColumn(0).getInt();
            meta.color = QString::fromStdString(query.getColumn(1).getText()).toStdWString();

            QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(query.getColumn(2).getText()));
            meta.tags.clear();
            if (doc.isArray()) {
                for (const auto& v : doc.array()) meta.tags.push_back(v.toString().toStdWString());
            }

            meta.pinned = query.getColumn(3).getInt() != 0;
            meta.note = QString::fromStdString(query.getColumn(4).getText()).toStdWString();
            meta.sortBy = QString::fromStdString(query.getColumn(5).getText()).toStdWString();
            meta.sortOrder = QString::fromStdString(query.getColumn(6).getText()).toStdWString();
            return true;
        }
    } catch (...) {}
    return false;
}

bool FolderRepo::remove(const std::wstring& path) {
    try {
        auto sqlite = Database::instance().sqlite();
        SQLite::Statement query(*sqlite, "DELETE FROM folders WHERE path = ?");
        query.bind(1, QString::fromStdWString(path).toStdString());
        return query.exec() > 0;
    } catch (...) {
        return false;
    }
}

} // namespace ArcMeta
