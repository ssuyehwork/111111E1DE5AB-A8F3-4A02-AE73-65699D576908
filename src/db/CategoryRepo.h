#ifndef CATEGORY_REPO_H
#define CATEGORY_REPO_H

#include <QString>
#include <QList>
#include <QStringList>
#include <QMap>

namespace ArcMeta {

struct Category {
    int id = -1;
    int parentId = 0;
    QString name;
    QString color;
    QStringList presetTags;
    int sortOrder = 0;
    int pinned = 0;
};

class CategoryRepo {
public:
    static int add(const Category& category);
    static bool remove(int id);
    static bool update(const Category& category);
    static QList<Category> getAll();

    // 统计功能
    static int getTotalItemCount();
    static int getTodayItemCount();
    static int getUncategorizedCount();
    static int getUntaggedCount();
    static int getPinnedCount();
};

} // namespace ArcMeta

#endif // CATEGORY_REPO_H
