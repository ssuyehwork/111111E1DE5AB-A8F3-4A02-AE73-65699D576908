#pragma once

#include <string>
#include <vector>
#include <functional>
#include <filesystem>

namespace ArcMeta {

/**
 * @brief 同步引擎
 * 负责离线变更追平与系统全量扫描逻辑
 */
class SyncEngine {
public:
    static SyncEngine& instance();

    /**
     * @brief 启动增量同步
     */
    void runIncrementalSync();

    /**
     * @brief 启动全量扫描（由启动、热插拔或手动触发）
     * @param onProgress 进度回调
     */
    void runFullScan(std::function<void(int current, int total)> onProgress = nullptr);

    /**
     * @brief 维护标签聚合表
     */
    void rebuildTagStats();

private:
    SyncEngine() = default;
    ~SyncEngine() = default;

    struct ScanContext;
    void scanDirInternal(const std::wstring& path, int depth, ScanContext& ctx);

    void scanDirectory(const std::filesystem::path& root, std::vector<std::wstring>& metaFiles);
};

} // namespace ArcMeta
