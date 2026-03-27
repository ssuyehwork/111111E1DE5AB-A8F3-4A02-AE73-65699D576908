#include "AmMetaJson.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QDebug>

namespace ArcMeta {

const QString AmMetaJson::META_FILENAME = ".am_meta.json";
const QString AmMetaJson::TMP_FILENAME = ".am_meta.json.tmp";

QJsonObject ItemMetadata::toJson() const {
    QJsonObject obj;
    obj["type"] = type;
    obj["rating"] = rating;
    obj["color"] = color;
    obj["tags"] = QJsonArray::fromStringList(tags);
    obj["pinned"] = pinned;
    obj["note"] = note;
    obj["encrypted"] = encrypted;
    if (encrypted) {
        obj["encrypt_salt"] = encryptSalt;
        obj["encrypt_verify_hash"] = encryptVerifyHash;
        obj["original_name"] = originalName;
    }
    obj["frn"] = static_cast<qint64>(frn);
    return obj;
}

ItemMetadata ItemMetadata::fromJson(const QJsonObject& obj) {
    ItemMetadata meta;
    meta.type = obj["type"].toString();
    meta.rating = obj["rating"].toInt();
    meta.color = obj["color"].toString();

    QJsonArray tagsArray = obj["tags"].toArray();
    for (const auto& val : tagsArray) {
        meta.tags.append(val.toString());
    }

    meta.pinned = obj["pinned"].toBool();
    meta.note = obj["note"].toString();
    meta.encrypted = obj["encrypted"].toBool();
    meta.encryptSalt = obj["encrypt_salt"].toString();
    meta.encryptVerifyHash = obj["encrypt_verify_hash"].toString();
    meta.originalName = obj["original_name"].toString();
    meta.frn = static_cast<unsigned long long>(obj["frn"].toVariant().toLongLong());
    return meta;
}

QJsonObject FolderMetadata::toJson() const {
    QJsonObject obj;
    obj["sort_by"] = sortBy;
    obj["sort_order"] = sortOrder;
    obj["rating"] = rating;
    obj["color"] = color;
    obj["tags"] = QJsonArray::fromStringList(tags);
    obj["pinned"] = pinned;
    obj["note"] = note;
    return obj;
}

FolderMetadata FolderMetadata::fromJson(const QJsonObject& obj) {
    FolderMetadata meta;
    meta.sortBy = obj["sort_by"].toString("name");
    meta.sortOrder = obj["sort_order"].toString("asc");
    meta.rating = obj["rating"].toInt();
    meta.color = obj["color"].toString();

    QJsonArray tagsArray = obj["tags"].toArray();
    for (const auto& val : tagsArray) {
        meta.tags.append(val.toString());
    }

    meta.pinned = obj["pinned"].toBool();
    meta.note = obj["note"].toString();
    return meta;
}

QJsonObject AmMeta::toJson() const {
    QJsonObject obj;
    obj["version"] = version;
    obj["folder"] = folder.toJson();

    QJsonObject itemsObj;
    for (auto it = items.begin(); it != items.end(); ++it) {
        itemsObj[it.key()] = it.value().toJson();
    }
    obj["items"] = itemsObj;

    return obj;
}

AmMeta AmMeta::fromJson(const QJsonObject& obj) {
    AmMeta meta;
    meta.version = obj["version"].toString("1");
    meta.folder = FolderMetadata::fromJson(obj["folder"].toObject());

    QJsonObject itemsObj = obj["items"].toObject();
    for (auto it = itemsObj.begin(); it != itemsObj.end(); ++it) {
        meta.items[it.key()] = ItemMetadata::fromJson(it.value().toObject());
    }

    return meta;
}

bool AmMetaJson::save(const QString& dirPath, const AmMeta& meta) {
    QString fullPath = QDir(dirPath).filePath(META_FILENAME);
    QString tmpPath = QDir(dirPath).filePath(TMP_FILENAME);

    // 1. 序列化内容
    QJsonDocument doc(meta.toJson());
    QByteArray bytes = doc.toJson();

    // 2. 写入临时文件
    QFile tmpFile(tmpPath);
    if (!tmpFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    tmpFile.write(bytes);
    tmpFile.close();

    // 3. 校验临时文件（重新解析）
    QFile checkFile(tmpPath);
    if (!checkFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonParseError error;
    QJsonDocument::fromJson(checkFile.readAll(), &error);
    checkFile.close();

    if (error.error != QJsonParseError::NoError) {
        QFile::remove(tmpPath);
        return false;
    }

    // 4. 原子替换
    if (QFile::exists(fullPath)) {
        if (!QFile::remove(fullPath)) {
            return false;
        }
    }

    if (!QFile::rename(tmpPath, fullPath)) {
        return false;
    }

    // 5. 设置隐藏属性（Windows API）
    std::wstring wPath = fullPath.toStdWString();
    SetFileAttributesW(wPath.c_str(), FILE_ATTRIBUTE_HIDDEN);

    return true;
}

AmMeta AmMetaJson::load(const QString& dirPath) {
    QString fullPath = QDir(dirPath).filePath(META_FILENAME);
    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return AmMeta();
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return AmMeta::fromJson(doc.toObject());
}

} // namespace ArcMeta
