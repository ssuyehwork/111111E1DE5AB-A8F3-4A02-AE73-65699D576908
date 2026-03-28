#ifndef AMMETAJSON_H
#define AMMETAJSON_H

#include <QString>
#include <QVariantMap>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QVariant>
#include <QList>
#include <QDebug>

class AmMetaJson {
public:
    static QVariantMap read(const QString& folderPath) {
        QString jsonPath = QDir(folderPath).filePath(".am_meta.json");
        QFile file(jsonPath);
        if (!file.open(QIODevice::ReadOnly)) return {};

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        return doc.toVariant().toMap();
    }

    static bool write(const QString& folderPath, const QVariantMap& data) {
        QString jsonPath = QDir(folderPath).filePath(".am_meta.json");
        QString tmpPath = jsonPath + ".tmp";

        QJsonDocument doc = QJsonDocument::fromVariant(data);
        QByteArray bytes = doc.toJson();

        // 1. 写入临时文件
        QFile tmpFile(tmpPath);
        if (!tmpFile.open(QIODevice::WriteOnly)) return false;
        tmpFile.write(bytes);
        tmpFile.close();

        // 2. 验证写入是否完整
        QFile verifyFile(tmpPath);
        if (verifyFile.open(QIODevice::ReadOnly)) {
            QJsonDocument verifyDoc = QJsonDocument::fromJson(verifyFile.readAll());
            verifyFile.close();
            if (verifyDoc.isNull()) {
                QFile::remove(tmpPath);
                return false;
            }
        }

        // 3. 原子替换 (Windows 兼容)
        if (QFile::exists(jsonPath)) {
            if (!QFile::remove(jsonPath)) return false;
        }
        return QFile::rename(tmpPath, jsonPath);
    }

    static QVariantMap getItemMeta(const QString& fullPath) {
        QFileInfo info(fullPath);
        QString folder = info.absolutePath();
        QString fileName = info.fileName();
        
        QVariantMap meta = read(folder);
        QVariantMap items = meta.value("items").toMap();
        return items.value(fileName).toMap();
    }

    static bool setItemMeta(const QString& fullPath, const QString& key, const QVariant& value) {
        QFileInfo info(fullPath);
        QString folder = info.absolutePath();
        QString fileName = info.fileName();

        QVariantMap meta = read(folder);
        if (meta.isEmpty()) meta["version"] = "1";
        
        QVariantMap items = meta.value("items").toMap();
        QVariantMap itemMeta = items.value(fileName).toMap();
        
        itemMeta[key] = value;
        // 如果该项没有任何元数据了，则从 items 中移除 (保持 JSON 精简)
        bool hasValue = false;
        for (auto it = itemMeta.begin(); it != itemMeta.end(); ++it) {
            if (it.key() == "type") continue;
            if (it.value().isValid() && !it.value().toString().isEmpty() && it.value() != 0 && it.value() != false) {
                hasValue = true;
                break;
            }
        }
        
        if (!hasValue) items.remove(fileName);
        else items[fileName] = itemMeta;

        meta["items"] = items;
        return write(folder, meta);
    }

    static bool renameItem(const QString& folderPath, const QString& oldName, const QString& newName) {
        QVariantMap meta = read(folderPath);
        if (meta.isEmpty()) return true;

        QVariantMap items = meta.value("items").toMap();
        if (items.contains(oldName)) {
            items[newName] = items.take(oldName);
            meta["items"] = items;
            return write(folderPath, meta);
        }
        return true;
    }
};

#endif // AMMETAJSON_H
