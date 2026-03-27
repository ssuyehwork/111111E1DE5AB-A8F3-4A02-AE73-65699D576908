#ifndef AM_META_JSON_H
#define AM_META_JSON_H

#include <QString>
#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <windows.h>

namespace ArcMeta {

struct ItemMetadata {
    QString type;       // "file" | "folder"
    int rating = 0;
    QString color;      // red | orange | green ...
    QStringList tags;
    bool pinned = false;
    QString note;
    bool encrypted = false;
    QString encryptSalt;
    QString encryptVerifyHash;
    QString originalName;
    unsigned long long frn = 0;

    QJsonObject toJson() const;
    static ItemMetadata fromJson(const QJsonObject& obj);
};

struct FolderMetadata {
    QString sortBy = "name";
    QString sortOrder = "asc";
    int rating = 0;
    QString color;
    QStringList tags;
    bool pinned = false;
    QString note;

    QJsonObject toJson() const;
    static FolderMetadata fromJson(const QJsonObject& obj);
};

struct AmMeta {
    QString version = "1";
    FolderMetadata folder;
    QMap<QString, ItemMetadata> items;

    QJsonObject toJson() const;
    static AmMeta fromJson(const QJsonObject& obj);
};

class AmMetaJson {
public:
    static bool save(const QString& dirPath, const AmMeta& meta);
    static AmMeta load(const QString& dirPath);

private:
    static const QString META_FILENAME;
    static const QString TMP_FILENAME;
};

} // namespace ArcMeta

#endif // AM_META_JSON_H
