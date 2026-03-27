#pragma once

#include <windows.h>
#include <string>
#include <unordered_map>
#include <mutex>

namespace ArcMeta {

/**
 * @brief 文件条目结构
 * 2026-03-27 按照用户要求：增加 visibleChildCount 上下文统计逻辑
 */
struct FileEntry {
    DWORDLONG frn = 0;
    DWORDLONG parentFrn = 0;
    std::wstring name;
    DWORD attributes = 0;

    // [NEW] 2026-03-27 核心判定字段
    int visibleChildCount = 0;

    bool isDir() const { return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
    bool isHidden() const { return (attributes & FILE_ATTRIBUTE_HIDDEN) != 0; }

    /**
     * @brief 判定是否为空文件夹（逻辑：是目录且可见子项数为 0）
     */
    bool isEmptyDir() const {
        return isDir() && (visibleChildCount == 0);
    }
};

using FileIndex = std::unordered_map<DWORDLONG, FileEntry>;

class MftReader {
public:
    MftReader();
    ~MftReader();

    /**
     * @brief 2026-03-27 按照用户要求：建立带子项计数的索引
     */
    bool buildIndexStub();

    const FileIndex& getIndex() const { return m_index; }

    // 2026-03-27 核心算法：遍历索引维护可见子项计数
    void calculateChildCounts();

private:
    FileIndex m_index;
    mutable std::mutex m_mutex;
};

} // namespace ArcMeta
