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

int CategoryRepo::getTotalItemCount() {
    QSqlQuery q("SELECT COUNT(*) FROM items", Database::instance().getDb());
    return q.next() ? q.value(0).toInt() : 0;
}

int CategoryRepo::getTodayItemCount() {
    // 简化逻辑：通过 last_sync 近似计算
    double today = QDateTime(QDate::currentDate(), QTime(0,0)).toSecsSinceEpoch();
    QSqlQuery q(Database::instance().getDb());
    q.prepare("SELECT COUNT(*) FROM folders WHERE last_sync >= ?");
    q.addBindValue(today);
    return q.exec() && q.next() ? q.value(0).toInt() : 0;
}

int CategoryRepo::getUncategorizedCount() {
    // 未出现在关联表中的 FRN 数量
    QSqlQuery q("SELECT COUNT(frn) FROM items WHERE frn NOT IN (SELECT frn FROM category_items)", Database::instance().getDb());
    return q.next() ? q.value(0).toInt() : 0;
}

int CategoryRepo::getUntaggedCount() {
    QSqlQuery q("SELECT COUNT(*) FROM items WHERE tags = '[]' OR tags = ''", Database::instance().getDb());
    return q.next() ? q.value(0).toInt() : 0;
}

int CategoryRepo::getPinnedCount() {
    QSqlQuery q("SELECT COUNT(*) FROM items WHERE pinned = 1", Database::instance().getDb());
    return q.next() ? q.value(0).toInt() : 0;
}

} // namespace ArcMeta
