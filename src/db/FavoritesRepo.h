#pragma once

#include "Database.h"
#include <string>
#include <vector>
#include <QSqlDatabase>

namespace ArcMeta {

struct Favorite {
    std::wstring path;
    std::wstring type;
    std::wstring name;
    int sortOrder = 0;
};

/**
 * @brief 收藏夹持久层
 */
class FavoritesRepo {
public:
    static bool add(const Favorite& fav, QSqlDatabase db = QSqlDatabase::database());
    static bool remove(const std::wstring& path, QSqlDatabase db = QSqlDatabase::database());
    static std::vector<Favorite> getAll(QSqlDatabase db = QSqlDatabase::database());
};

} // namespace ArcMeta
