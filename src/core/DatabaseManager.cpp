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
    q.exec("CREATE TABLE IF NOT EXISTS items (id INTEGER PRIMARY KEY, title TEXT, content TEXT, tags TEXT, item_type TEXT, category_id INTEGER, rating INTEGER, is_pinned INTEGER, color TEXT, updated_at DATETIME)");
    q.exec("CREATE TABLE IF NOT EXISTS categories (id INTEGER PRIMARY KEY, name TEXT, parent_id INTEGER)");
    m_isInitialized = true;
    return true;
}

void DatabaseManager::closeAndPack() { if(m_db.isOpen()) m_db.close(); }

int DatabaseManager::addItem(const QString& t, const QString& c, const QStringList& tags, const QString& color, int catId, const QString& type) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO items (title, content, tags, color, category_id, item_type, updated_at) VALUES (?,?,?,?,?,?,?)");
    q.addBindValue(t); q.addBindValue(c); q.addBindValue(tags.join(",")); q.addBindValue(color); q.addBindValue(catId); q.addBindValue(type);
    q.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    if (q.exec()) return q.lastInsertId().toInt();
    return 0;
}

bool DatabaseManager::updateItem(int id, const QString& t, const QString& c, const QStringList& tags) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("UPDATE items SET title=?, content=?, tags=?, updated_at=? WHERE id=?");
    q.addBindValue(t); q.addBindValue(c); q.addBindValue(tags.join(",")); q.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")); q.addBindValue(id);
    return q.exec();
}

bool DatabaseManager::deleteItemsBatch(const QList<int>& ids) {
    QMutexLocker locker(&m_mutex);
    m_db.transaction();
    for (int id : ids) { QSqlQuery q(m_db); q.prepare("DELETE FROM items WHERE id=?"); q.addBindValue(id); q.exec(); }
    return m_db.commit();
}

bool DatabaseManager::updateItemState(int id, const QString& col, const QVariant& val) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare(QString("UPDATE items SET %1=? WHERE id=?").arg(col));
    q.addBindValue(val); q.addBindValue(id);
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
    q.prepare("SELECT * FROM items WHERE title LIKE ? OR content LIKE ?");
    q.addBindValue("%"+key+"%"); q.addBindValue("%"+key+"%"); q.exec();
    while (q.next()) { QVariantMap m; QSqlRecord r = q.record(); for(int i=0; i<r.count(); ++i) m[r.fieldName(i)] = q.value(i); res << m; }
    return res;
}

QVariantMap DatabaseManager::getItemById(int id) {
    QSqlQuery q(m_db); q.prepare("SELECT * FROM items WHERE id=?"); q.addBindValue(id); q.exec();
    if (q.next()) { QVariantMap m; QSqlRecord r = q.record(); for(int i=0; i<r.count(); ++i) m[r.fieldName(i)] = q.value(i); return m; }
    return {};
}

QVariantMap DatabaseManager::getCounts() { return {}; }
void DatabaseManager::beginBatch() { m_db.transaction(); }
void DatabaseManager::endBatch() { m_db.commit(); }
