#pragma once

#include <string>
#include <memory>

namespace ArcMeta {

/**
 * @brief 数据库管理类 (原生 QtSql 实现，拒绝第三方 SQLiteCpp)
 */
class Database {
public:
    static Database& instance();

    bool init(const std::wstring& dbPath);

private:
    Database();
    ~Database();

    void createTables();
    void createIndexes();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ArcMeta
