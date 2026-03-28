#include "FavoritesRepo.h"
#include <QString>
#include <QDateTime>

namespace ArcMeta {

bool FavoritesRepo::add(const Favorite& fav) {
    try {
        auto sqlite = Database::instance().sqlite();
        SQLite::Statement query(*sqlite, "INSERT OR REPLACE INTO favorites (path, type, name, sort_order, added_at) VALUES (?, ?, ?, ?, ?)");
        query.bind(1, QString::fromStdWString(fav.path).toStdString());
        query.bind(2, QString::fromStdWString(fav.type).toStdString());
        query.bind(3, QString::fromStdWString(fav.name).toStdString());
        query.bind(4, fav.sortOrder);
        query.bind(5, (double)QDateTime::currentMSecsSinceEpoch());
        return query.exec() > 0;
    } catch (...) {
        return false;
    }
}

bool FavoritesRepo::remove(const std::wstring& path) {
    try {
        auto sqlite = Database::instance().sqlite();
        SQLite::Statement query(*sqlite, "DELETE FROM favorites WHERE path = ?");
        query.bind(1, QString::fromStdWString(path).toStdString());
        return query.exec() > 0;
    } catch (...) {
        return false;
    }
}

std::vector<Favorite> FavoritesRepo::getAll() {
    std::vector<Favorite> results;
    try {
        auto sqlite = Database::instance().sqlite();
        SQLite::Statement query(*sqlite, "SELECT path, type, name, sort_order FROM favorites ORDER BY sort_order ASC");
        while (query.executeStep()) {
            Favorite fav;
            fav.path = QString::fromStdString(query.getColumn(0).getText()).toStdWString();
            fav.type = QString::fromStdString(query.getColumn(1).getText()).toStdWString();
            fav.name = QString::fromStdString(query.getColumn(2).getText()).toStdWString();
            fav.sortOrder = query.getColumn(3).getInt();
            results.push_back(fav);
        }
    } catch (...) {}
    return results;
}

} // namespace ArcMeta
