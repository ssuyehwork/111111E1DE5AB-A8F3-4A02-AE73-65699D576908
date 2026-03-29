#include "CategoryRepo.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>

namespace ArcMeta {

bool CategoryRepo::add(Category& cat, QSqlDatabase db) {
    QSqlQuery q(db);
    q.prepare("INSERT INTO categories (parent_id, name, color, preset_tags, sort_order, pinned, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(cat.parentId);
    q.addBindValue(QString::fromStdWString(cat.name));
    q.addBindValue(QString::fromStdWString(cat.color));

    QJsonArray tagsArr;
    for (const auto& t : cat.presetTags) tagsArr.append(QString::fromStdWString(t));
    q.addBindValue(QJsonDocument(tagsArr).toJson(QJsonDocument::Compact));

    q.addBindValue(cat.sortOrder);
    q.addBindValue(cat.pinned ? 1 : 0);
    q.addBindValue((double)QDateTime::currentMSecsSinceEpoch());

    if (q.exec()) {
        cat.id = q.lastInsertId().toInt();
        return true;
    }
    return false;
}

bool CategoryRepo::addItemToCategory(int categoryId, const std::wstring& itemPath, QSqlDatabase db) {
    QSqlQuery q(db);
    q.prepare("INSERT OR IGNORE INTO category_items (category_id, item_path, added_at) VALUES (?, ?, ?)");
    q.addBindValue(categoryId);
    q.addBindValue(QString::fromStdWString(itemPath));
    q.addBindValue((double)QDateTime::currentMSecsSinceEpoch());
    return q.exec();
}

std::vector<Category> CategoryRepo::getAll(QSqlDatabase db) {
    std::vector<Category> results;
    QSqlQuery q("SELECT id, parent_id, name, color, preset_tags, sort_order, pinned FROM categories ORDER BY sort_order ASC", db);
    while (q.next()) {
        Category cat;
        cat.id = q.value(0).toInt();
        cat.parentId = q.value(1).toInt();
        cat.name = q.value(2).toString().toStdWString();
        cat.color = q.value(3).toString().toStdWString();
        
        QJsonDocument doc = QJsonDocument::fromJson(q.value(4).toByteArray());
        if (doc.isArray()) {
            for (const auto& v : doc.array()) cat.presetTags.push_back(v.toString().toStdWString());
        }

        cat.sortOrder = q.value(5).toInt();
        cat.pinned = q.value(6).toBool();
        results.push_back(cat);
    }
    return results;
}

bool CategoryRepo::remove(int id, QSqlDatabase db) {
    QSqlQuery q1(db);
    q1.prepare("DELETE FROM category_items WHERE category_id = ?");
    q1.addBindValue(id);
    q1.exec();

    QSqlQuery q2(db);
    q2.prepare("DELETE FROM categories WHERE id = ?");
    q2.addBindValue(id);
    return q2.exec();
}

bool CategoryRepo::update(const Category& cat, QSqlDatabase db) {
    QSqlQuery q(db);
    q.prepare("UPDATE categories SET parent_id = ?, name = ?, color = ?, sort_order = ?, pinned = ? WHERE id = ?");
    q.addBindValue(cat.parentId);
    q.addBindValue(QString::fromStdWString(cat.name));
    q.addBindValue(QString::fromStdWString(cat.color));
    q.addBindValue(cat.sortOrder);
    q.addBindValue(cat.pinned ? 1 : 0);
    q.addBindValue(cat.id);
    return q.exec();
}

bool CategoryRepo::removeItemFromCategory(int categoryId, const std::wstring& itemPath, QSqlDatabase db) {
    QSqlQuery q(db);
    q.prepare("DELETE FROM category_items WHERE category_id = ? AND item_path = ?");
    q.addBindValue(categoryId);
    q.addBindValue(QString::fromStdWString(itemPath));
    return q.exec();
}

} // namespace ArcMeta
