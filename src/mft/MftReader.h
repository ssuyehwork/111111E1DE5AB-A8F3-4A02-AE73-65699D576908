#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <windows.h>

namespace ArcMeta {

/**
 * @brief 文件条目基础结构
 */
struct FileEntry {
    std::wstring volume;
    DWORDLONG frn = 0;
    DWORDLONG parentFrn = 0;
    std::wstring name;
    DWORD attributes = 0;

    bool isDir() const { return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
};

/**
 * @brief 文件索引管理器
 * 实现 MFT 枚举及无权限时的文件系统降级扫描
 */
class MftReader {
public:
    static MftReader& instance();

    /**
     * @brief 构建全盘索引
     * 自动尝试 MFT 模式，失败或无权限时降级为 std::filesystem 模式
     */
    void buildIndex();

    /**
     * @brief 获取指定目录下的子项
     */
    std::vector<FileEntry> getChildren(const std::wstring& folderPath);

private:
    MftReader() = default;

    void buildIndexMft();
    void buildIndexFallback(); // 降级模式

    // volume -> (frn -> Entry)
    std::unordered_map<std::wstring, std::unordered_map<DWORDLONG, FileEntry>> m_index;

    // 降级模式下的路径索引：fullPath -> Entry
    std::unordered_map<std::wstring, FileEntry> m_pathIndex;

    bool m_isUsingMft = false;
};

} // namespace ArcMeta
