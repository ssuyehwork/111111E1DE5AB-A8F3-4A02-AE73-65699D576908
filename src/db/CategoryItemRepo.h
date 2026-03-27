#ifndef CATEGORY_ITEM_REPO_H
#define CATEGORY_ITEM_REPO_H

#include <vector>
#include <windows.h>
#include <QStringList>

namespace ArcMeta {

class CategoryItemRepo {
public:
    // 将条目（通过 FRN）关联到分类
    static bool addItem(int categoryId, DWORDLONG frn);

    // 从分类中移除条目
    static bool removeItem(int categoryId, DWORDLONG frn);

    // 获取分类下的所有 FRN
    static std::vector<DWORDLONG> getItems(int categoryId);

    // 获取条目所属的所有分类 ID
    static std::vector<int> getCategoriesForItem(DWORDLONG frn);

    // 清理某条目的所有分类关联
    static bool clearItem(DWORDLONG frn);
};

} // namespace ArcMeta

#endif // CATEGORY_ITEM_REPO_H
