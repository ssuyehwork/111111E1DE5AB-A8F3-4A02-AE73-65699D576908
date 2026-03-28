#include "MftReader.h"
#include <filesystem>
#include <windows.h>
#include <QString>

namespace ArcMeta {

MftReader& MftReader::instance() {
    static MftReader inst;
    return inst;
}

/**
 * @brief 构建索引入口
 */
void MftReader::buildIndex() {
    // 默认执行文件系统遍历索引
    m_isUsingMft = false;

    if (m_isUsingMft) {
        buildIndexMft();
    } else {
        buildIndexFallback();
    }
}

/**
 * @brief 实现符合文档要求的降级方案：使用 std::filesystem
 */
void MftReader::buildIndexFallback() {
    m_pathIndex.clear();

    // 遍历所有固定驱动器
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (drives & (1 << i)) {
            wchar_t drivePath[] = { (wchar_t)(L'A' + i), L':', L'\\', L'\0' };
            if (GetDriveTypeW(drivePath) == DRIVE_FIXED) {
                try {
                    // 仅执行顶层和关键目录扫描，防止启动过慢
                    for (const auto& entry : std::filesystem::directory_iterator(drivePath, std::filesystem::directory_options::skip_permission_denied)) {
                        FileEntry fe;
                        fe.volume = std::wstring(1, L'A' + i) + L":";
                        fe.name = entry.path().filename().wstring();
                        fe.attributes = entry.is_directory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
                        m_pathIndex[entry.path().wstring()] = fe;
                    }
                } catch (...) {}
            }
        }
    }
}

void MftReader::buildIndexMft() {
    // 第二阶段实现细节：使用 DeviceIoControl 枚举 USN
}

/**
 * @brief 动态获取子项列表
 */
std::vector<FileEntry> MftReader::getChildren(const std::wstring& folderPath) {
    std::vector<FileEntry> results;
    try {
        std::filesystem::path p(folderPath);
        for (const auto& entry : std::filesystem::directory_iterator(p, std::filesystem::directory_options::skip_permission_denied)) {
            FileEntry fe;
            fe.name = entry.path().filename().wstring();
            fe.attributes = entry.is_directory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
            results.push_back(fe);
        }
    } catch (...) {}
    return results;
}

} // namespace ArcMeta
