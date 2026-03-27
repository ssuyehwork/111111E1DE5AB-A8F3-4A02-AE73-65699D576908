#include "FolderRepo.h"
#include "Database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>

namespace ArcMeta {

bool FolderRepo::upsert(const FolderInfo& info) {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare(
        "INSERT OR REPLACE INTO folders (path, rating, color, tags, pinned, note, sort_by, sort_order, last_sync) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"
    );
    query.addBindValue(info.path);
    query.addBindValue(info.rating);
    query.addBindValue(info.color);
    query.addBindValue(QJsonDocument(QJsonArray::fromStringList(info.tags)).toJson(QJsonDocument::Compact));
    query.addBindValue(info.pinned ? 1 : 0);
    query.addBindValue(info.note);
    query.addBindValue(info.sortBy);
    query.addBindValue(info.sortOrder);
    query.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());
    return query.exec();
}

FolderInfo FolderRepo::getByPath(const QString& path) {
    FolderInfo info;
    info.path = path;
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare("SELECT rating, color, tags, pinned, note, sort_by, sort_order FROM folders WHERE path = ?");
    query.addBindValue(path);

    if (query.exec() && query.next()) {
        info.rating = query.value(0).toInt();
        info.color = query.value(1).toString();
        QJsonDocument doc = QJsonDocument::fromJson(query.value(2).toByteArray());
        for (const auto& val : doc.array()) info.tags.append(val.toString());
        info.pinned = query.value(3).toInt() == 1;
        info.note = query.value(4).toString();
        info.sortBy = query.value(5).toString();
        info.sortOrder = query.value(6).toString();
    }
    return info;
}

bool FolderRepo::removeByPath(const QString& path) {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare("DELETE FROM folders WHERE path = ?");
    query.addBindValue(path);
    return query.exec();
}

} // namespace ArcMeta
