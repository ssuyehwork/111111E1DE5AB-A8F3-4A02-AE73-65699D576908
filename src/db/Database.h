#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

namespace ArcMeta {

class Database {
public:
    static Database& instance();

    bool init(const QString& dbPath = "arcmeta.db");
    void close();

    QSqlDatabase& getDb() { return m_db; }

private:
    Database() = default;
    ~Database() = default;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool createTables();

    QSqlDatabase m_db;
};

} // namespace ArcMeta

#endif // DATABASE_H
