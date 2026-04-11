#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <unordered_map>
#include <shared_mutex>
#include <string>

namespace ArcMeta {

/**
 * @brief 内存元数据镜像结构
 * 仅保留 UI 高频展现和筛选所需的字段，极致压缩内存占用。
 */
struct RuntimeMeta {
    int rating = 0;
    std::wstring color;
    QStringList tags;
    std::wstring note;
    bool pinned = false;
    bool encrypted = false;
};

/**
 * @brief 元数据管理器 (架构核心层)
 * 1. 维护元数据内存 L1 缓存，消除 UI 刷新的磁盘 IO。
 * 2. 统一数据更新入口，确保 Memory, DB, JSON 三方一致性。
 */
class MetadataManager : public QObject {
    Q_OBJECT
public:
    static MetadataManager& instance();

    /**
     * @brief 从数据库初始化内存镜像
     */
    void initFromDatabase();

    /**
     * @brief 高效查询接口 (O(1))
     */
    RuntimeMeta getMeta(const std::wstring& path);

    /**
     * @brief 预热指定目录及其子项的元数据
     */
    void prefetchDirectory(const std::wstring& dirPath);

    /**
     * @brief 统一更新接口
     */
    void setRating(const std::wstring& path, int rating);
    void setColor(const std::wstring& path, const std::wstring& color);
    void setPinned(const std::wstring& path, bool pinned);
    void setTags(const std::wstring& path, const QStringList& tags);
    void setNote(const std::wstring& path, const std::wstring& note);
    void setEncrypted(const std::wstring& path, bool encrypted);

    /**
     * @brief 同步更新内存缓存中的路径键值（重命名时调用）
     */
    void renameItem(const std::wstring& oldPath, const std::wstring& newPath);

signals:
    /**
     * @brief 元数据变更信号，UI 订阅此信号以实现局部刷新
     */
    void metaChanged(const std::wstring& path);

private:
    MetadataManager(QObject* parent = nullptr);
    ~MetadataManager() override = default;

    // 路径到元数据的映射
    std::unordered_map<std::wstring, RuntimeMeta> m_cache;
    mutable std::shared_mutex m_mutex;

    // 2026-05-20 性能优化：延迟持久化防抖
    std::unordered_map<std::wstring, QTimer*> m_debounceTimers;

    /**
     * @brief 异步持久化：刷入数据库和 JSON
     */
    void persistAsync(const std::wstring& path);

    /**
     * @brief 内部辅助：防抖持久化
     */
    void debouncePersist(const std::wstring& path);
};

} // namespace ArcMeta
