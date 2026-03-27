#include "FavoritesRepo.h"
#include "Database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

namespace ArcMeta {

bool FavoritesRepo::add(const FavoriteItem& item) {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO favorites (path, type, name, sort_order, added_at) VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(item.path);
    query.addBindValue(item.type);
    query.addBindValue(item.name);
    query.addBindValue(item.sortOrder);
    query.addBindValue(QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0);
    return query.exec();
}

bool FavoritesRepo::remove(const QString& path) {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare("DELETE FROM favorites WHERE path = ?");
    query.addBindValue(path);
    return query.exec();
}

QList<FavoriteItem> FavoritesRepo::getAll() {
    QList<FavoriteItem> list;
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query("SELECT path, type, name, sort_order, added_at FROM favorites ORDER BY sort_order ASC", db);

    while (query.next()) {
        FavoriteItem item;
        item.path = query.value(0).toString();
        item.type = query.value(1).toString();
        item.name = query.value(2).toString();
        item.sortOrder = query.value(3).toInt();
        item.addedAt = query.value(4).toDouble();
        list.append(item);
    }
    return list;
}

} // namespace ArcMeta
