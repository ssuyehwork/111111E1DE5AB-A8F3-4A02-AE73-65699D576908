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

    // 3. tags 表
    ok = query.exec(
        "CREATE TABLE IF NOT EXISTS tags ("
        "    tag         TEXT PRIMARY KEY,"
        "    item_count  INTEGER DEFAULT 0"
        ")"
    );
    if (!ok) return false;

    // 4. 索引创建
    query.exec("CREATE INDEX IF NOT EXISTS idx_items_path ON items(path)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_items_parent ON items(parent_path)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_items_rating ON items(rating)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_items_color ON items(color)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_items_tags ON items(tags)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_items_pinned ON items(pinned)");

    // 5. sync_state 表
    ok = query.exec(
        "CREATE TABLE IF NOT EXISTS sync_state ("
        "    key         TEXT PRIMARY KEY,"
        "    value       TEXT"
        ")"
    );
    if (!ok) return false;

    return true;
}

} // namespace ArcMeta
