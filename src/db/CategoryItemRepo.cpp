#include "CategoryItemRepo.h"
#include "Database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

namespace ArcMeta {

bool CategoryItemRepo::addItem(int categoryId, DWORDLONG frn) {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare("INSERT OR IGNORE INTO category_items (category_id, frn) VALUES (?, ?)");
    query.addBindValue(categoryId);
    query.addBindValue(static_cast<qulonglong>(frn));
    return query.exec();
}

bool CategoryItemRepo::removeItem(int categoryId, DWORDLONG frn) {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare("DELETE FROM category_items WHERE category_id = ? AND frn = ?");
    query.addBindValue(categoryId);
    query.addBindValue(static_cast<qulonglong>(frn));
    return query.exec();
}

std::vector<DWORDLONG> CategoryItemRepo::getItems(int categoryId) {
    std::vector<DWORDLONG> results;
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare("SELECT frn FROM category_items WHERE category_id = ?");
    query.addBindValue(categoryId);

    if (query.exec()) {
        while (query.next()) {
            results.push_back(query.value(0).toULongLong());
        }
    }
    return results;
}

std::vector<int> CategoryItemRepo::getCategoriesForItem(DWORDLONG frn) {
    std::vector<int> results;
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare("SELECT category_id FROM category_items WHERE frn = ?");
    query.addBindValue(static_cast<qulonglong>(frn));

    if (query.exec()) {
        while (query.next()) {
            results.push_back(query.value(0).toInt());
        }
    }
    return results;
}

bool CategoryItemRepo::clearItem(DWORDLONG frn) {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare("DELETE FROM category_items WHERE frn = ?");
    query.addBindValue(static_cast<qulonglong>(frn));
    return query.exec();
}

} // namespace ArcMeta
