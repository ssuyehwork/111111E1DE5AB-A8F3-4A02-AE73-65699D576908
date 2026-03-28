#pragma once

#include <string>
#include <memory>
#include <vector>
#include <SQLiteCpp/SQLiteCpp.h>

namespace ArcMeta {

/**
 * @brief 数据库管理类
 * 负责 SQLite 连接初始化、Schema 创建及多线程防死锁配置
 */
class Database {
public:
    static Database& instance();

    /**
     * @brief 初始化数据库连接与建表
     * @param dbPath 数据库文件路径（std::wstring）
     * @return 成功返回 true
     */
    bool init(const std::wstring& dbPath);

    /**
     * @brief 获取底层 SQLiteCpp 连接对象
     */
    std::shared_ptr<SQLite::Database> sqlite() { return m_db; }

    /**
     * @brief 执行事务（RAII）
     */
    std::unique_ptr<SQLite::Transaction> createTransaction();

private:
    Database() = default;
    ~Database() = default;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // 创建 Schema 的核心方法
    void createTables();
    void createIndexes();

    std::shared_ptr<SQLite::Database> m_db = nullptr;
};

} // namespace ArcMeta
