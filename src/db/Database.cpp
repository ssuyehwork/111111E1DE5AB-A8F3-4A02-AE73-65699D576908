#include "Database.h"
#include <QString>

namespace ArcMeta {

Database& Database::instance() {
    static Database inst;
    return inst;
}

bool Database::init(const std::wstring& dbPath) {
    try {
        // 1. 建立数据库连接（以 std::string 传入，对于 Windows 路径需注意 UTF-8）
        // 这里使用 QString::fromStdWString().toUtf8().constData() 转换为 UTF-8
        std::string utf8Path = QString::fromStdWString(dbPath).toUtf8().constData();
        m_db = std::make_shared<SQLite::Database>(utf8Path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

        // 2. 关键红线：解决多线程死锁配置
        m_db->exec("PRAGMA journal_mode=WAL;");
        m_db->exec("PRAGMA synchronous=NORMAL;");
        m_db->exec("PRAGMA busy_timeout=5000;");

        // 3. 初始化 Schema
        createTables();
        createIndexes();

        return true;
    } catch (const std::exception& e) {
        // 错误处理由外部通过日志记录
        return false;
    }
}

std::unique_ptr<SQLite::Transaction> Database::createTransaction() {
    if (!m_db) return nullptr;
    return std::make_unique<SQLite::Transaction>(*m_db);
}

void Database::createTables() {
    if (!m_db) return;

    // 文件夹元数据表
    m_db->exec(R"sql(
        CREATE TABLE IF NOT EXISTS folders (
            path        TEXT PRIMARY KEY,
            rating      INTEGER DEFAULT 0,
            color       TEXT    DEFAULT '',
            tags        TEXT    DEFAULT '',
            pinned      INTEGER DEFAULT 0,
            note        TEXT    DEFAULT '',
            sort_by     TEXT    DEFAULT 'name',
            sort_order  TEXT    DEFAULT 'asc',
            last_sync   REAL
        );
    )sql");

    // 文件与子文件夹元数据表
    m_db->exec(R"sql(
        CREATE TABLE IF NOT EXISTS items (
            volume              TEXT    NOT NULL DEFAULT '',
            frn                 TEXT    NOT NULL DEFAULT '',
            path                TEXT,
            parent_path         TEXT,
            type                TEXT,
            rating              INTEGER DEFAULT 0,
            color               TEXT    DEFAULT '',
            tags                TEXT    DEFAULT '',
            pinned              INTEGER DEFAULT 0,
            note                TEXT    DEFAULT '',
            encrypted           INTEGER DEFAULT 0,
            encrypt_salt        TEXT    DEFAULT '',
            encrypt_iv          TEXT    DEFAULT '',
            encrypt_verify_hash TEXT    DEFAULT '',
            original_name       TEXT    DEFAULT '',
            ctime               REAL    DEFAULT 0,
            mtime               REAL    DEFAULT 0,
            atime               REAL    DEFAULT 0,
            deleted             INTEGER DEFAULT 0,
            PRIMARY KEY (volume, frn)
        );
    )sql");

    // 标签聚合索引表
    m_db->exec(R"sql(
        CREATE TABLE IF NOT EXISTS tags (
            tag         TEXT PRIMARY KEY,
            item_count  INTEGER DEFAULT 0
        );
    )sql");

    // 收藏夹表
    m_db->exec(R"sql(
        CREATE TABLE IF NOT EXISTS favorites (
            path        TEXT PRIMARY KEY,
            type        TEXT,
            name        TEXT,
            sort_order  INTEGER DEFAULT 0,
            added_at    REAL
        );
    )sql");

    // 分类表
    m_db->exec(R"sql(
        CREATE TABLE IF NOT EXISTS categories (
            id                  INTEGER PRIMARY KEY AUTOINCREMENT,
            parent_id           INTEGER DEFAULT 0,
            name                TEXT NOT NULL,
            color               TEXT DEFAULT '',
            preset_tags         TEXT DEFAULT '',
            sort_order          INTEGER DEFAULT 0,
            pinned              INTEGER DEFAULT 0,
            encrypted           INTEGER DEFAULT 0,
            encrypt_salt        TEXT DEFAULT '',
            encrypt_verify_hash TEXT DEFAULT '',
            created_at          REAL
        );
    )sql");

    // 分类条目关联表
    m_db->exec(R"sql(
        CREATE TABLE IF NOT EXISTS category_items (
            category_id INTEGER,
            item_path   TEXT,
            added_at    REAL,
            PRIMARY KEY (category_id, item_path)
        );
    )sql");

    // 同步状态与全局配置表
    m_db->exec(R"sql(
        CREATE TABLE IF NOT EXISTS sync_state (
            key         TEXT PRIMARY KEY,
            value       TEXT
        );
    )sql");
}

void Database::createIndexes() {
    if (!m_db) return;

    m_db->exec("CREATE INDEX IF NOT EXISTS idx_items_path     ON items(path);");
    m_db->exec("CREATE INDEX IF NOT EXISTS idx_items_parent   ON items(parent_path);");
    m_db->exec("CREATE INDEX IF NOT EXISTS idx_items_rating   ON items(rating);");
    m_db->exec("CREATE INDEX IF NOT EXISTS idx_items_color    ON items(color);");
    m_db->exec("CREATE INDEX IF NOT EXISTS idx_items_tags     ON items(tags);");
    m_db->exec("CREATE INDEX IF NOT EXISTS idx_items_pinned   ON items(pinned);");
    m_db->exec("CREATE INDEX IF NOT EXISTS idx_items_ctime    ON items(ctime);");
    m_db->exec("CREATE INDEX IF NOT EXISTS idx_items_mtime    ON items(mtime);");
    m_db->exec("CREATE INDEX IF NOT EXISTS idx_items_deleted  ON items(deleted);");
    m_db->exec("CREATE INDEX IF NOT EXISTS idx_cat_parent     ON categories(parent_id);");
    m_db->exec("CREATE INDEX IF NOT EXISTS idx_cat_items_path ON category_items(item_path);");
}

} // namespace ArcMeta
