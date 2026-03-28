#include "DatabaseManager.h"
#include <QSqlRecord>
#include <QDebug>

DatabaseManager& DatabaseManager::instance() { static DatabaseManager inst; return inst; }
DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent) {}

bool DatabaseManager::init(const QString& dbPath) {
    QMutexLocker locker(&m_mutex);
    m_db = QSqlDatabase::addDatabase("QSQLITE", "MainConn");
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) return false;

    QSqlQuery q(m_db);

    // 1. 文件夹元数据表
    q.exec("CREATE TABLE IF NOT EXISTS folders ("
           "path TEXT PRIMARY KEY, "
           "rating INTEGER DEFAULT 0, "
           "color TEXT DEFAULT '', "
           "tags TEXT DEFAULT '', "
           "pinned INTEGER DEFAULT 0, "
           "note TEXT DEFAULT '', "
           "sort_by TEXT DEFAULT 'name', "
           "sort_order TEXT DEFAULT 'asc', "
           "last_sync REAL)");

    // 2. 文件与子文件夹元数据表
    q.exec("CREATE TABLE IF NOT EXISTS items ("
           "path TEXT, "
           "frn INTEGER PRIMARY KEY, "
           "type TEXT, "
           "rating INTEGER DEFAULT 0, "
           "color TEXT DEFAULT '', "
           "tags TEXT DEFAULT '', "
           "pinned INTEGER DEFAULT 0, "
           "note TEXT DEFAULT '', "
           "encrypted INTEGER DEFAULT 0, "
           "encrypt_salt TEXT DEFAULT '', "
           "encrypt_verify_hash TEXT DEFAULT '', "
           "original_name TEXT DEFAULT '', "
           "parent_path TEXT)");

    // 3. 标签聚合索引表
    q.exec("CREATE TABLE IF NOT EXISTS tags ("
           "tag TEXT PRIMARY KEY, "
           "item_count INTEGER DEFAULT 0)");

    // 4. 收藏夹表
    q.exec("CREATE TABLE IF NOT EXISTS favorites ("
           "path TEXT PRIMARY KEY, "
           "type TEXT, "
           "name TEXT, "
           "sort_order INTEGER DEFAULT 0, "
           "added_at REAL)");

    // 5. 分类表
    q.exec("CREATE TABLE IF NOT EXISTS categories ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
           "parent_id INTEGER DEFAULT 0, "
           "name TEXT NOT NULL, "
           "color TEXT DEFAULT '', "
           "preset_tags TEXT DEFAULT '', "
           "sort_order INTEGER DEFAULT 0, "
           "pinned INTEGER DEFAULT 0, "
           "encrypted INTEGER DEFAULT 0, "
           "encrypt_salt TEXT DEFAULT '', "
           "encrypt_verify_hash TEXT DEFAULT '', "
           "created_at REAL)");

    // 6. 分类条目关联表
    q.exec("CREATE TABLE IF NOT EXISTS category_items ("
           "category_id INTEGER, "
           "item_path TEXT, "
           "added_at REAL, "
           "PRIMARY KEY (category_id, item_path))");

    // 7. 同步状态与全局配置表
    q.exec("CREATE TABLE IF NOT EXISTS sync_state ("
           "key TEXT PRIMARY KEY, "
           "value TEXT)");

    // 索引创建
    q.exec("CREATE INDEX IF NOT EXISTS idx_items_path ON items(path)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_items_parent ON items(parent_path)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_items_rating ON items(rating)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_items_color ON items(color)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_items_tags ON items(tags)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_items_pinned ON items(pinned)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_cat_parent ON categories(parent_id)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_cat_items_path ON category_items(item_path)");

    m_isInitialized = true;
    return true;
}

void DatabaseManager::closeAndPack() { if(m_db.isOpen()) m_db.close(); }

bool DatabaseManager::updateItemMeta(quint64 frn, const QVariantMap& meta) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO items (frn, path, type, rating, color, tags, pinned, note, encrypted, encrypt_salt, encrypt_verify_hash, original_name, parent_path) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(frn);
    q.addBindValue(meta.value("path"));
    q.addBindValue(meta.value("type"));
    q.addBindValue(meta.value("rating", 0).toInt());
    q.addBindValue(meta.value("color", "").toString());
    q.addBindValue(meta.value("tags", "").toString());
    q.addBindValue(meta.value("pinned", 0).toInt());
    q.addBindValue(meta.value("note", "").toString());
    q.addBindValue(meta.value("encrypted", 0).toInt());
    q.addBindValue(meta.value("encrypt_salt", "").toString());
    q.addBindValue(meta.value("encrypt_verify_hash", "").toString());
    q.addBindValue(meta.value("original_name", "").toString());
    q.addBindValue(meta.value("parent_path", "").toString());
    return q.exec();
}

bool DatabaseManager::deleteItem(quint64 frn) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM items WHERE frn = ?");
    q.addBindValue(frn);
    return q.exec();
}

bool DatabaseManager::updateFolderMeta(const QString& path, const QVariantMap& meta) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO folders (path, rating, color, tags, pinned, note, sort_by, sort_order, last_sync) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(path);
    q.addBindValue(meta.value("rating", 0).toInt());
    q.addBindValue(meta.value("color", "").toString());
    q.addBindValue(meta.value("tags", "").toString());
    q.addBindValue(meta.value("pinned", 0).toInt());
    q.addBindValue(meta.value("note", "").toString());
    q.addBindValue(meta.value("sort_by", "name").toString());
    q.addBindValue(meta.value("sort_order", "asc").toString());
    q.addBindValue(meta.value("last_sync", 0).toDouble());
    return q.exec();
}

int DatabaseManager::addCategory(const QString& name, int pid) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO categories (name, parent_id) VALUES (?,?)");
    q.addBindValue(name); q.addBindValue(pid);
    if (q.exec()) return q.lastInsertId().toInt();
    return 0;
}

QList<QVariantMap> DatabaseManager::getAllCategories() {
    QList<QVariantMap> res; QSqlQuery q("SELECT * FROM categories", m_db);
    while (q.next()) { QVariantMap m; QSqlRecord r = q.record(); for(int i=0; i<r.count(); ++i) m[r.fieldName(i)] = q.value(i); res << m; }
    return res;
}

QList<QVariantMap> DatabaseManager::searchItems(const QString& key, int) {
    QList<QVariantMap> res; QSqlQuery q(m_db);
    q.prepare("SELECT * FROM items WHERE path LIKE ? OR note LIKE ?");
    q.addBindValue("%"+key+"%"); q.addBindValue("%"+key+"%"); q.exec();
    while (q.next()) { QVariantMap m; QSqlRecord r = q.record(); for(int i=0; i<r.count(); ++i) m[r.fieldName(i)] = q.value(i); res << m; }
    return res;
}

QVariantMap DatabaseManager::getItemById(int frn) {
    QSqlQuery q(m_db); q.prepare("SELECT * FROM items WHERE frn=?"); q.addBindValue(frn); q.exec();
    if (q.next()) { QVariantMap m; QSqlRecord r = q.record(); for(int i=0; i<r.count(); ++i) m[r.fieldName(i)] = q.value(i); return m; }
    return {};
}

QVariantMap DatabaseManager::getCounts() { return {}; }
void DatabaseManager::beginBatch() { m_db.transaction(); }
void DatabaseManager::endBatch() { m_db.commit(); }
