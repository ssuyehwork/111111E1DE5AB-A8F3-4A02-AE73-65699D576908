#ifndef FOLDER_REPO_H
#define FOLDER_REPO_H

#include <QString>
#include <QList>

namespace ArcMeta {

struct FolderInfo {
    QString path;
    int rating = 0;
    QString color;
    QStringList tags;
    bool pinned = false;
    QString note;
    QString sortBy = "name";
    QString sortOrder = "asc";
};

class FolderRepo {
public:
    static bool upsert(const FolderInfo& info);
    static FolderInfo getByPath(const QString& path);
    static bool removeByPath(const QString& path);
};

} // namespace ArcMeta

#endif // FOLDER_REPO_H
