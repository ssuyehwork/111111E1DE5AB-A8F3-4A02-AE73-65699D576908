#include "Database.h"
#include <QDir>
#include <QStandardPaths>

namespace ArcMeta {

Database& Database::instance() {
    static Database inst;
    return inst;
}

bool Database::init(const QString& dbPath) {
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qDebug() << "无法打开数据库:" << m_db.lastError().text();
        return false;
    }

    return createTables();
}

void Database::close() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool Database::createTables() {
    QSqlQuery query(m_db);

    // 1. folders 表
    bool ok = query.exec(
        "CREATE TABLE IF NOT EXISTS folders ("
        "    path        TEXT PRIMARY KEY,"
        "    rating      INTEGER DEFAULT 0,"
        "    color       TEXT    DEFAULT '',"
        "    tags        TEXT    DEFAULT '',"
        "    pinned      INTEGER DEFAULT 0,"
        "    note        TEXT    DEFAULT '',"
        "    sort_by     TEXT    DEFAULT 'name',"
        "    sort_order  TEXT    DEFAULT 'asc',"
        "    last_sync   REAL"
        ")"
    );
    if (!ok) return false;

    // 2. items 表
    ok = query.exec(
        "CREATE TABLE IF NOT EXISTS items ("
        "    path               TEXT,"
        "    frn                INTEGER PRIMARY KEY,"
        "    type               TEXT,"
        "    rating             INTEGER DEFAULT 0,"
        "    color              TEXT    DEFAULT '',"
        "    tags               TEXT    DEFAULT '',"
        "    pinned             INTEGER DEFAULT 0,"
        "    note               TEXT    DEFAULT '',"
        "    encrypted          INTEGER DEFAULT 0,"
        "    encrypt_salt       TEXT    DEFAULT '',"
        "    encrypt_verify_hash TEXT   DEFAULT '',"
        "    original_name      TEXT    DEFAULT '',"
        "    parent_path        TEXT"
        ")"
    );
    if (!ok) return false;

    // 3. categories 表
    ok = query.exec(
        "CREATE TABLE IF NOT EXISTS categories ("
        "    id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    parent_id   INTEGER DEFAULT 0,"
        "    name        TEXT,"
        "    color       TEXT,"
        "    preset_tags TEXT,"
        "    sort_order  INTEGER DEFAULT 0,"
        "    pinned      INTEGER DEFAULT 0,"
        "    created_at  REAL"
        ")"
    );
    if (!ok) return false;

    // 4. category_items 表 (关联 FRN 与 Category ID)
    ok = query.exec(
        "CREATE TABLE IF NOT EXISTS category_items ("
        "    category_id INTEGER,"
        "    frn         INTEGER,"
        "    PRIMARY KEY (category_id, frn)"
        ")"
    );
    if (!ok) return false;

    // 5. tags 表
    ok = query.exec(
        "CREATE TABLE IF NOT EXISTS tags ("
        "    tag         TEXT PRIMARY KEY,"
        "    item_count  INTEGER DEFAULT 0"
        ")"
    );
    if (!ok) return false;

    // 6. sync_state 表
    ok = query.exec(
        "CREATE TABLE IF NOT EXISTS sync_state ("
        "    key         TEXT PRIMARY KEY,"
        "    value       TEXT"
        ")"
    );
    if (!ok) return false;

    // 索引
    query.exec("CREATE INDEX IF NOT EXISTS idx_items_path ON items(path)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_cat_items_frn ON category_items(frn)");

    return true;
}

} // namespace ArcMeta
