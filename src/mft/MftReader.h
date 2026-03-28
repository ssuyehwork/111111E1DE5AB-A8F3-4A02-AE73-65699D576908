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

    // 关键优化：父子关系反向索引 volume -> (parentFrn -> vector of childFrns)
    std::unordered_map<std::wstring, std::unordered_map<DWORDLONG, std::vector<DWORDLONG>>> m_parentToChildren;

    /**
     * @brief 根据路径获取对应的 FRN（用于 MFT 模式下的钻取）
     */
    DWORDLONG getFrnFromPath(const std::wstring& folderPath);

    // 降级模式下的路径索引：fullPath -> Entry
    std::unordered_map<std::wstring, FileEntry> m_pathIndex;

    bool m_isUsingMft = false;

public:
    const std::unordered_map<std::wstring, std::unordered_map<DWORDLONG, FileEntry>>& getIndex() const { return m_index; }

    /**
     * @brief USN 监听器更新内存索引
     */
    void updateEntry(const FileEntry& entry) {
        m_index[entry.volume][entry.frn] = entry;
    }
};

} // namespace ArcMeta
