#ifndef ITEM_REPO_H
#define ITEM_REPO_H

#include <QString>
#include <windows.h>

namespace ArcMeta {

class ItemRepo {
public:
    // 更新文件路径（当捕获到重命名/移动事件时）
    static bool updatePath(DWORDLONG frn, const QString& newPath, const QString& newParentPath);

    // 删除记录
    static bool removeByFrn(DWORDLONG frn);

    // 级联删除：删除该路径下的所有子项记录
    static bool removeByParentPath(const QString& parentPath);
};

} // namespace ArcMeta

#endif // ITEM_REPO_H
