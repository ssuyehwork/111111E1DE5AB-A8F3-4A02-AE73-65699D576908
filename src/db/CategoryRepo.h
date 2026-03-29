#pragma once

#include "Database.h"
#include <string>
#include <vector>
#include <QSqlDatabase>

namespace ArcMeta {

struct Category {
    int id = 0;
    int parentId = 0;
    std::wstring name;
    std::wstring color;
    std::vector<std::wstring> presetTags;
    int sortOrder = 0;
    bool pinned = false;
    bool encrypted = false;
};

/**
 * @brief 分类持久层
 */
class CategoryRepo {
public:
    static bool add(Category& cat, QSqlDatabase db = QSqlDatabase::database());
    static bool update(const Category& cat, QSqlDatabase db = QSqlDatabase::database());
    static bool remove(int id, QSqlDatabase db = QSqlDatabase::database());
    static std::vector<Category> getAll(QSqlDatabase db = QSqlDatabase::database());

    // 条目关联逻辑
    static bool addItemToCategory(int categoryId, const std::wstring& itemPath, QSqlDatabase db = QSqlDatabase::database());
    static bool removeItemFromCategory(int categoryId, const std::wstring& itemPath, QSqlDatabase db = QSqlDatabase::database());
};

} // namespace ArcMeta
