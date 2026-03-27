#include "ItemRepo.h"
#include "Database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

namespace ArcMeta {

bool ItemRepo::updatePath(DWORDLONG frn, const QString& newPath, const QString& newParentPath) {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare("UPDATE items SET path = ?, parent_path = ? WHERE frn = ?");
    query.addBindValue(newPath);
    query.addBindValue(newParentPath);
    query.addBindValue(static_cast<qint64>(frn));

    if (!query.exec()) {
        qDebug() << "ItemRepo: 更新路径失败 -" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool ItemRepo::removeByFrn(DWORDLONG frn) {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare("DELETE FROM items WHERE frn = ?");
    query.addBindValue(static_cast<qint64>(frn));
    return query.exec();
}

bool ItemRepo::removeByParentPath(const QString& parentPath) {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare("DELETE FROM items WHERE parent_path = ?");
    query.addBindValue(parentPath);
    return query.exec();
}

} // namespace ArcMeta
