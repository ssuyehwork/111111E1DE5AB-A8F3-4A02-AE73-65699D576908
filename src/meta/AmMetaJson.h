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
#include <QCryptographicHash>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class AmMetaJson {
public:
    static quint64 generateVirtualFrn(const QString& path) {
        // [DEGRADE] 降级方案：路径哈希作为虚拟 FRN
        QByteArray hash = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256);
        // 取前 8 字节作为 64 位整数
        quint64 result = 0;
        memcpy(&result, hash.constData(), 8);
        return result;
    }

    static void setHidden(const QString& path) {
#ifdef Q_OS_WIN
        SetFileAttributesW((LPCWSTR)path.utf16(), FILE_ATTRIBUTE_HIDDEN);
#endif
    }

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
        bool success = false;
#ifdef Q_OS_WIN
        if (QFile::exists(jsonPath)) {
            success = ReplaceFileW((LPCWSTR)jsonPath.utf16(), (LPCWSTR)tmpPath.utf16(), NULL, REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL);
        } else {
            success = QFile::rename(tmpPath, jsonPath);
        }
#else
        if (QFile::exists(jsonPath)) {
            QFile::remove(jsonPath);
        }
        success = QFile::rename(tmpPath, jsonPath);
#endif
        if (success) {
            setHidden(jsonPath);
        }
        return success;
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
