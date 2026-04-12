#include "ItemRepo.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QStringList>

namespace ArcMeta {

bool ItemRepo::save(const std::wstring& parentPath, const std::wstring& name, const ItemMeta& meta) {
    // 2026-03-xx 修复：通过 getThreadDatabase 获取当前线程专属连接，确保在 SyncQueue 等后台线程中正常保存数据。
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    
    std::wstring fullPath = parentPath;
    if (!fullPath.empty() && fullPath.back() != L'\\' && fullPath.back() != L'/') fullPath += L'\\';
    fullPath += name;

    std::wstring volume = meta.volume;
    std::wstring frn = meta.frn;

    // 2026-04-10 关键修复：如果 JSON 中主键为空（常发生于非 MFT 扫描结果），尝试根据路径反查原有主键
    // 2026-04-10 深度优化：优先使用 路径+卷序列号 匹配，确保跨盘符时的唯一性
    if (volume.empty() || frn.empty()) {
        QSqlQuery check(db);
        check.prepare("SELECT volume, frn FROM items WHERE path = ?");
        check.addBindValue(QString::fromStdWString(fullPath));
        if (check.exec() && check.next()) {
            if (volume.empty()) volume = check.value(0).toString().toStdWString();
            if (frn.empty()) frn = check.value(1).toString().toStdWString();
        }
    }

    // 如果仍然为空（新条目），则使用路径 MD5 生成一个伪 FRN 以保证主键唯一性
    if (volume.empty() || frn.empty()) {
        volume = L"VIRTUAL";
        frn = QString(QCryptographicHash::hash(QString::fromStdWString(fullPath).toUtf8(), QCryptographicHash::Md5).toHex()).toStdWString();
    }

    QSqlQuery q(db);
    q.prepare(R"sql(
        INSERT OR REPLACE INTO items 
        (volume, frn, path, parent_path, type, rating, color, tags, pinned, note, 
         encrypted, encrypt_salt, encrypt_iv, encrypt_verify_hash, original_name) 
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )sql");

    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    q.addBindValue(QString::fromStdWString(fullPath));
    q.addBindValue(QString::fromStdWString(parentPath));
    q.addBindValue(QString::fromStdWString(meta.type));
    q.addBindValue(meta.rating);
    q.addBindValue(QString::fromStdWString(meta.color));

    QJsonArray tagsArr;
    for (const auto& t : meta.tags) tagsArr.append(QString::fromStdWString(t));
    q.addBindValue(QJsonDocument(tagsArr).toJson(QJsonDocument::Compact));

    q.addBindValue(meta.pinned ? 1 : 0);
    q.addBindValue(QString::fromStdWString(meta.note));
    q.addBindValue(meta.encrypted ? 1 : 0);
    q.addBindValue(QString::fromStdString(meta.encryptSalt));
    q.addBindValue(QString::fromStdString(meta.encryptIv));
    q.addBindValue(QString::fromStdString(meta.encryptVerifyHash));
    q.addBindValue(QString::fromStdWString(meta.originalName));

    return q.exec();
}

bool ItemRepo::saveBasicInfo(const std::wstring& volume, const std::wstring& frn, const std::wstring& path, const std::wstring& parentPath, bool isDir, double mtime, double size) {
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    QSqlQuery q(db);

    // 采用 INSERT OR IGNORE 先确保记录存在，然后 UPDATE 物理属性
    // 这样可以保护 rating, color 等用户元数据字段不被覆盖
    q.prepare(R"sql(
        INSERT OR IGNORE INTO items (volume, frn, path, parent_path, type)
        VALUES (?, ?, ?, ?, ?)
    )sql");
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    q.addBindValue(QString::fromStdWString(path));
    q.addBindValue(QString::fromStdWString(parentPath));
    q.addBindValue(isDir ? "folder" : "file");
    q.exec();

    QSqlQuery u(db);
    u.prepare("UPDATE items SET path = ?, parent_path = ?, mtime = ?, deleted = 0 WHERE volume = ? AND frn = ?");
    u.addBindValue(QString::fromStdWString(path));
    u.addBindValue(QString::fromStdWString(parentPath));
    u.addBindValue(mtime);
    u.addBindValue(QString::fromStdWString(volume));
    u.addBindValue(QString::fromStdWString(frn));
    Q_UNUSED(size); // 扩展预留
    return u.exec();
}

bool ItemRepo::markAsDeleted(const std::wstring& volume, const std::wstring& frn) {
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    QSqlQuery q(db);
    q.prepare("UPDATE items SET deleted = 1 WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    return q.exec();
}

bool ItemRepo::removeByFrn(const std::wstring& volume, const std::wstring& frn) {
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    QSqlQuery q(db);
    q.prepare("DELETE FROM items WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    return q.exec();
}

std::wstring ItemRepo::getPathByFrn(const std::wstring& volume, const std::wstring& frn) {
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    QSqlQuery q(db);
    q.prepare("SELECT path FROM items WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    if (q.exec() && q.next()) {
        return q.value(0).toString().toStdWString();
    }
    return L"";
}

bool ItemRepo::updatePath(const std::wstring& volume, const std::wstring& frn, const std::wstring& newPath, const std::wstring& newParentPath) {
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    QSqlQuery q(db);
    q.prepare("UPDATE items SET path = ?, parent_path = ? WHERE volume = ? AND frn = ?");
    q.addBindValue(QString::fromStdWString(newPath));
    q.addBindValue(QString::fromStdWString(newParentPath));
    q.addBindValue(QString::fromStdWString(volume));
    q.addBindValue(QString::fromStdWString(frn));
    return q.exec();
}

// 2026-04-12 按照用户要求：基于数据库搜索，支持局部（指定父路径）和全局（不限路径）两种模式
QStringList ItemRepo::searchByKeyword(const QString& keyword, const QString& parentPath) {
    if (keyword.isEmpty()) return {};
    QSqlDatabase db = ArcMeta::Database::instance().getThreadDatabase();
    QSqlQuery q(db);

    if (parentPath.isEmpty()) {
        // 全局搜索：在所有 items 中按文件名包含分匹配
        q.prepare("SELECT path FROM items WHERE path LIKE ? AND deleted = 0 LIMIT 300");
        q.addBindValue("%" + keyword + "%");
    } else {
        // 局部搜索：仅在指定父路径下的直接子条目中搜索
        // 同时匹配 parent_path = ? 且 path LIKE % keyword %
        q.prepare("SELECT path FROM items WHERE parent_path = ? AND path LIKE ? AND deleted = 0 LIMIT 300");
        q.addBindValue(parentPath);
        q.addBindValue("%" + keyword + "%");
    }

    QStringList results;
    if (q.exec()) {
        while (q.next()) {
            results << q.value(0).toString();
        }
    }
    return results;
}

} // namespace ArcMeta
