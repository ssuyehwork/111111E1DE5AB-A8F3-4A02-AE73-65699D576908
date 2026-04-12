#pragma once

#include <string>
#include <vector>
#include <QMap>
#include <QString>

namespace ArcMeta {

/**
 * @brief 2026-05-22 按照用户要求：废除数据库。
 * 提供最小化的 Stub 数据结构，以解决 UI 层的编译依赖。
 */
struct Category {
    int id = 0;
    int parentId = 0;
    std::wstring name;
    std::wstring color;
    std::vector<std::wstring> presetTags;
    int sortOrder = 0;
    bool pinned = false;
    bool encrypted = false;
    std::wstring encryptHint;
};

class CategoryRepo {
public:
    static bool add(Category&) { return false; }
    static bool update(const Category&) { return false; }
    static bool remove(int) { return false; }
    static bool reorder(int, bool) { return false; }
    static bool reorderAll(bool) { return false; }
    static std::vector<Category> getAll() { return {}; }
    static std::vector<std::pair<int, int>> getCounts() { return {}; }
    static int getUniqueItemCount() { return 0; }
    static int getUncategorizedItemCount() { return 0; }
    static QMap<QString, int> getSystemCounts() { return {}; }
    static std::vector<std::wstring> getItemPathsInCategory(int) { return {}; }
    static bool addItemToCategory(int, const std::wstring&) { return false; }
    static bool removeItemFromCategory(int, const std::wstring&) { return false; }
};

class ItemRepo {
public:
    static QStringList searchByKeyword(const QString&, const QString&) { return {}; }
};

} // namespace ArcMeta
