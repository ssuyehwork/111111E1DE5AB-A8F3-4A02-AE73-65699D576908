#ifndef FAVORITES_REPO_H
#define FAVORITES_REPO_H

#include <QString>
#include <QList>
#include <QDateTime>

namespace ArcMeta {

struct FavoriteItem {
    QString path;
    QString type;
    QString name;
    int sortOrder = 0;
    double addedAt = 0;
};

class FavoritesRepo {
public:
    static bool add(const FavoriteItem& item);
    static bool remove(const QString& path);
    static QList<FavoriteItem> getAll();
};

} // namespace ArcMeta

#endif // FAVORITES_REPO_H
