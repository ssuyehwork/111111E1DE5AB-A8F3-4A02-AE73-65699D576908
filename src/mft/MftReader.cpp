#include "MftReader.h"
#include <winioctl.h>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <execution>
#include <mutex>

namespace ArcMeta {

MftReader& MftReader::instance() {
    static MftReader inst;
    return inst;
}

/**
 * @brief 扫描所有固定驱动器并构建索引
 */
void MftReader::buildIndex() {
    m_index.clear();
    m_pathIndex.clear();

    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (drives & (1 << i)) {
            wchar_t driveLetter = (wchar_t)(L'A' + i);
            std::wstring volumeName = std::wstring(1, driveLetter) + L":";

            wchar_t driveRoot[] = { driveLetter, L':', L'\\', L'\0' };
            if (GetDriveTypeW(driveRoot) == DRIVE_FIXED) {
                // 尝试 MFT 读取
                if (!loadMftForVolume(volumeName)) {
                    // 如果 MFT 失败（无权限等），执行降级扫描
                    scanDirectoryFallback(volumeName);
                }
            }
        }
    }
}

/**
 * @brief 使用 DeviceIoControl 枚举 MFT 记录
 */
bool MftReader::loadMftForVolume(const std::wstring& volumeName) {
    std::wstring path = L"\\\\.\\" + volumeName;
    HANDLE hVol = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (hVol == INVALID_HANDLE_VALUE) return false;

    USN_JOURNAL_DATA journalData;
    DWORD cb;
    if (!DeviceIoControl(hVol, FSCTL_QUERY_USN_JOURNAL, NULL, 0, &journalData, sizeof(journalData), &cb, NULL)) {
        CloseHandle(hVol);
        return false;
    }

    MFT_ENUM_DATA enumData;
    enumData.StartFileReferenceNumber = 0;
    enumData.LowUsn = 0;
    enumData.HighUsn = journalData.NextUsn;

    const int BUF_SIZE = 64 * 1024; // 64KB 缓冲区
    std::vector<BYTE> buffer(BUF_SIZE);

    auto& volumeIndex = m_index[volumeName];
    volumeIndex.reserve(1000000); // 预分配防止 rehash

    while (DeviceIoControl(hVol, FSCTL_ENUM_USN_DATA, &enumData, sizeof(enumData), buffer.data(), BUF_SIZE, &cb, NULL)) {
        BYTE* pData = buffer.data() + sizeof(USN);
        while (pData < buffer.data() + cb) {
            USN_RECORD_V2* pRecord = (USN_RECORD_V2*)pData;

            FileEntry entry;
            entry.volume = volumeName;
            entry.frn = pRecord->FileReferenceNumber;
            entry.parentFrn = pRecord->ParentFileReferenceNumber;
            entry.attributes = pRecord->FileAttributes;
            entry.name = std::wstring(pRecord->FileName, pRecord->FileNameLength / sizeof(wchar_t));

            volumeIndex[entry.frn] = entry;
            // 建立父子索引
            m_parentToChildren[volumeName][entry.parentFrn].push_back(entry.frn);

            pData += pRecord->RecordLength;
        }
        enumData.StartFileReferenceNumber = ((USN_RECORD_V2*)buffer.data())->FileReferenceNumber;
    }

    CloseHandle(hVol);
    m_isUsingMft = true;
    return true;
}

/**
 * @brief 降级扫描实现
 */
/**
 * @brief 优化：仅扫描顶层目录以防止启动过慢，深度扫描由 UI 驱动或后台按需进行
 */
void MftReader::scanDirectoryFallback(const std::wstring& volumeName) {
    try {
        std::wstring rootPath = volumeName + L"\\";
        // 仅迭代一级目录
        for (const auto& entry : std::filesystem::directory_iterator(rootPath, std::filesystem::directory_options::skip_permission_denied)) {
            FileEntry fe;
            fe.volume = volumeName;
            fe.name = entry.path().filename().wstring();
            fe.attributes = entry.is_directory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
            m_pathIndex[entry.path().wstring()] = fe;
        }
    } catch (...) {}
}

/**
 * @brief 获取指定目录下的子项列表
 */
std::vector<FileEntry> MftReader::getChildren(const std::wstring& folderPath) {
    std::vector<FileEntry> results;
    std::wstring vol = folderPath.length() >= 2 ? folderPath.substr(0, 2) : L"";
    if (vol.empty()) return results;

    if (m_isUsingMft) {
        DWORDLONG parentFrn = getFrnFromPath(folderPath);
        if (parentFrn == 0) return results;

        auto& childrenMap = m_parentToChildren[vol];
        if (childrenMap.count(parentFrn)) {
            auto& entries = m_index[vol];
            for (DWORDLONG childFrn : childrenMap[parentFrn]) {
                if (entries.count(childFrn)) results.push_back(entries[childFrn]);
            }
        }
    } else {
        // 2. 降级模式：直接扫描文件系统
        try {
            std::filesystem::path p(folderPath);
            for (const auto& entry : std::filesystem::directory_iterator(p, std::filesystem::directory_options::skip_permission_denied)) {
                FileEntry fe;
                fe.volume = folderPath.substr(0, 2);
                fe.name = entry.path().filename().wstring();
                fe.attributes = entry.is_directory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
                results.push_back(fe);
            }
        } catch (...) {}
    }
    return results;
}

/**
 * @brief 根据路径逆向检索 FRN
 */
#include "PathBuilder.h"

/**
 * @brief 实现 O(1) 路径检索
 */
DWORDLONG MftReader::getFrnFromPath(const std::wstring& folderPath) {
    std::wstring vol = folderPath.length() >= 2 ? folderPath.substr(0, 2) : L"";
    if (vol.empty()) return 0;

    const auto& volPathMap = m_pathToFrn[vol];
    auto it = volPathMap.find(folderPath);
    if (it != volPathMap.end()) {
        return it->second;
    }

    // 备选逻辑：如果映射表未填充，则全量匹配并缓存（仅在特殊初始化阶段发生）
    auto& entries = m_index[vol];
    for (const auto& [frn, entry] : entries) {
        if (entry.isDir()) {
            std::wstring fullPath = PathBuilder::getPath(vol, frn);
            m_pathToFrn[vol][fullPath] = frn;
            if (fullPath == folderPath) return frn;
        }
    }
    return 0;
}

/**
 * @brief 实现并行文件名搜索 (std::execution::par)
 */
std::vector<FileEntry> MftReader::search(const std::wstring& query, const std::wstring& volume) {
    if (query.empty()) return {};

    // 1. 收集所有待搜索项的指针
    std::vector<const FileEntry*> pool;
    if (!volume.empty()) {
        if (m_index.count(volume)) {
            for (auto& pair : m_index[volume]) pool.push_back(&pair.second);
        }
    } else {
        for (auto& volPair : m_index) {
            for (auto& pair : volPair.second) pool.push_back(&pair.second);
        }
    }

    // 2. 将 query 转换为小写，支持大小写不敏感匹配
    std::wstring lQuery = query;
    std::transform(lQuery.begin(), lQuery.end(), lQuery.begin(), ::towlower);

    // 3. 并行过滤
    std::vector<FileEntry> results;
    std::mutex resultsMutex;

    std::for_each(std::execution::par, pool.begin(), pool.end(), [&](const FileEntry* entry) {
        std::wstring lName = entry->name;
        std::transform(lName.begin(), lName.end(), lName.begin(), ::towlower);

        if (lName.find(lQuery) != std::wstring::npos) {
            std::lock_guard<std::mutex> lock(resultsMutex);
            results.push_back(*entry);
        }
    });

    return results;
}

} // namespace ArcMeta
