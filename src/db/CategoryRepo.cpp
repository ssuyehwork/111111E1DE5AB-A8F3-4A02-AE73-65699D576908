#include "CategoryRepo.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QString>
#include <QDateTime>

namespace ArcMeta {

bool CategoryRepo::add(Category& cat) {
    try {
        auto sqlite = Database::instance().sqlite();
        SQLite::Statement query(*sqlite, "INSERT INTO categories (parent_id, name, color, preset_tags, sort_order, pinned, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)");

        query.bind(1, cat.parentId);
        query.bind(2, QString::fromStdWString(cat.name).toStdString());
        query.bind(3, QString::fromStdWString(cat.color).toStdString());

        QJsonArray tagsArr;
        for (const auto& t : cat.presetTags) tagsArr.append(QString::fromStdWString(t));
        query.bind(4, QJsonDocument(tagsArr).toJson(QJsonDocument::Compact).toStdString());

        query.bind(5, cat.sortOrder);
        query.bind(6, cat.pinned ? 1 : 0);
        query.bind(7, (double)QDateTime::currentMSecsSinceEpoch());

        if (query.exec() > 0) {
            cat.id = (int)sqlite->getLastInsertRowid();
            return true;
        }
    } catch (...) {}
    return false;
}

bool CategoryRepo::addItemToCategory(int categoryId, const std::wstring& itemPath) {
    try {
        auto sqlite = Database::instance().sqlite();
        SQLite::Statement query(*sqlite, "INSERT OR IGNORE INTO category_items (category_id, item_path, added_at) VALUES (?, ?, ?)");
        query.bind(1, categoryId);
        query.bind(2, QString::fromStdWString(itemPath).toStdString());
        query.bind(3, (double)QDateTime::currentMSecsSinceEpoch());
        return query.exec() > 0;
    } catch (...) {
        return false;
    }
}

std::vector<Category> CategoryRepo::getAll() {
    std::vector<Category> results;
    try {
        auto sqlite = Database::instance().sqlite();
        SQLite::Statement query(*sqlite, "SELECT id, parent_id, name, color, preset_tags, sort_order, pinned FROM categories ORDER BY sort_order ASC");

        while (query.executeStep()) {
            Category cat;
            cat.id = query.getColumn(0).getInt();
            cat.parentId = query.getColumn(1).getInt();
            cat.name = QString::fromStdString(query.getColumn(2).getText()).toStdWString();
            cat.color = QString::fromStdString(query.getColumn(3).getText()).toStdWString();

            QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(query.getColumn(4).getText()));
            if (doc.isArray()) {
                for (const auto& v : doc.array()) cat.presetTags.push_back(v.toString().toStdWString());
            }

            cat.sortOrder = query.getColumn(5).getInt();
            cat.pinned = query.getColumn(6).getInt() != 0;
            results.push_back(cat);
        }
    } catch (...) {}
    return results;
}

bool CategoryRepo::remove(int id) {
    try {
        auto sqlite = Database::instance().sqlite();
        sqlite->exec("DELETE FROM category_items WHERE category_id = " + std::to_string(id));
        SQLite::Statement query(*sqlite, "DELETE FROM categories WHERE id = ?");
        query.bind(1, id);
        return query.exec() > 0;
    } catch (...) {
        return false;
    }
}

} // namespace ArcMeta
