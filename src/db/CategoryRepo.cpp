#include "CategoryRepo.h"
#include "Database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVariant>
#include <QDateTime>

namespace ArcMeta {

int CategoryRepo::add(const Category& category) {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO categories (parent_id, name, color, preset_tags, sort_order, pinned, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"
    );
    query.addBindValue(category.parentId);
    query.addBindValue(category.name);
    query.addBindValue(category.color);
    query.addBindValue(QJsonDocument(QJsonArray::fromStringList(category.presetTags)).toJson(QJsonDocument::Compact));
    query.addBindValue(category.sortOrder);
    query.addBindValue(category.pinned);
    query.addBindValue(QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0);

    if (query.exec()) {
        return query.lastInsertId().toInt();
    }
    return -1;
}

bool CategoryRepo::remove(int id) {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    // 同时清理关联项（虽然文档说变为未分类，实际逻辑可在业务层处理，此处先删除分类表记录）
    query.prepare("DELETE FROM categories WHERE id = ?");
    query.addBindValue(id);
    return query.exec();
}

QList<Category> CategoryRepo::getAll() {
    QList<Category> list;
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query("SELECT id, parent_id, name, color, preset_tags, sort_order, pinned FROM categories ORDER BY sort_order ASC", db);

    while (query.next()) {
        Category c;
        c.id = query.value(0).toInt();
        c.parentId = query.value(1).toInt();
        c.name = query.value(2).toString();
        c.color = query.value(3).toString();

        QJsonDocument doc = QJsonDocument::fromJson(query.value(4).toByteArray());
        for (const auto& val : doc.array()) {
            c.presetTags.append(val.toString());
        }

        c.sortOrder = query.value(5).toInt();
        c.pinned = query.value(6).toInt();
        list.append(c);
    }
    return list;
}

bool CategoryRepo::update(const Category& category) {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare(
        "UPDATE categories SET parent_id = ?, name = ?, color = ?, preset_tags = ?, sort_order = ?, pinned = ? WHERE id = ?"
    );
    query.addBindValue(category.parentId);
    query.addBindValue(category.name);
    query.addBindValue(category.color);
    query.addBindValue(QJsonDocument(QJsonArray::fromStringList(category.presetTags)).toJson(QJsonDocument::Compact));
    query.addBindValue(category.sortOrder);
    query.addBindValue(category.pinned);
    query.addBindValue(category.id);
    return query.exec();
}

} // namespace ArcMeta
