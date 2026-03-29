#include "MftReader.h"
#include "PathBuilder.h"
#include <winioctl.h>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <execution>
#include <mutex>
#include <functional>
#include <thread>

namespace ArcMeta {

MftReader& MftReader::instance() {
    static MftReader inst;
    return inst;
}

/**
 * @brief 扫描所有固定驱动器并构建索引
 */
void MftReader::buildIndex() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
            
            // 2026-03-xx 极致性能优化：预存储小写名称
            entry.nameLower = entry.name;
            std::transform(entry.nameLower.begin(), entry.nameLower.end(), entry.nameLower.begin(), ::towlower);

            volumeIndex[entry.frn] = entry;
            // 建立父子索引
            m_parentToChildren[volumeName][entry.parentFrn].push_back(entry.frn);

            pData += pRecord->RecordLength;
        }
        enumData.StartFileReferenceNumber = ((USN_RECORD_V2*)buffer.data())->FileReferenceNumber;
    }

    CloseHandle(hVol);
    m_isUsingMft = true;

    // 2026-03-xx 极致性能重构：线性扫描预计算全量路径映射，消除 $O(N \cdot D)$ 递归
    precomputePaths(volumeName);

    return true;
}

/**
 * @brief 极致性能：使用非递归 BFS 一次性建立全量路径索引
 */
void MftReader::precomputePaths(const std::wstring& volume) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto& volIndex = m_index[volume];
    auto& childrenMap = m_parentToChildren[volume];
    auto& pathMap = m_pathToFrn[volume];
    auto& frnMap = m_frnToPath[volume];

    pathMap.clear();
    frnMap.clear();

    // BFS 核心循环：使用非递归方式避免栈溢出并达到 O(N) 建立全量索引
    struct Node { DWORDLONG frn; std::wstring path; };
    std::vector<Node> bfsQueue;
    bfsQueue.reserve(volIndex.size());

    // 1. 初始化根目录子项 (FRN=5 为根)
    if (childrenMap.count(5)) {
        for (DWORDLONG childFrn : childrenMap[5]) {
            if (volIndex.count(childFrn)) {
                FileEntry& fe = volIndex[childFrn];
                std::wstring path = volume + L"\\" + fe.name;
                pathMap[path] = childFrn;
                frnMap[childFrn] = path;
                if (fe.isDir()) bfsQueue.push_back({childFrn, path});
            }
        }
    }

    // 2. 层序遍历
    size_t head = 0;
    while (head < bfsQueue.size()) {
        Node parent = bfsQueue[head++];
        if (childrenMap.count(parent.frn)) {
            for (DWORDLONG childFrn : childrenMap[parent.frn]) {
                if (volIndex.count(childFrn)) {
                    FileEntry& fe = volIndex[childFrn];
                    std::wstring fullPath = parent.path + L"\\" + fe.name;
                    pathMap[fullPath] = childFrn;
                    frnMap[childFrn] = fullPath;
                    if (fe.isDir()) bfsQueue.push_back({childFrn, fullPath});
                }
            }
        }
    }
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
            std::wstring fullPath = entry.path().wstring();
            FileEntry fe;
            fe.volume = volumeName;
            fe.name = entry.path().filename().wstring();
            fe.attributes = entry.is_directory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;

            // 2026-03-xx 降级模式主键增强：为无 FRN 的环境生成基于路径哈希的虚拟 FRN
            fe.frn = std::hash<std::wstring>{}(fullPath);

            m_pathIndex[fullPath] = fe;
        }
    } catch (...) {}
}

/**
 * @brief 获取指定目录下的子项列表
 */
std::vector<FileEntry> MftReader::getChildren(const std::wstring& folderPath) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
                std::wstring fullPath = entry.path().wstring();
                FileEntry fe;
                fe.volume = folderPath.substr(0, 2);
                fe.name = entry.path().filename().wstring();
                fe.attributes = entry.is_directory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;

                // 2026-03-xx 降级模式主键增强
                fe.frn = std::hash<std::wstring>{}(fullPath);

                results.push_back(fe);
            }
        } catch (...) {}
    }
    return results;
}

/**
 * @brief 根据路径逆向检索 FRN
 */

/**
 * @brief 实现 O(1) 路径检索
 */
DWORDLONG MftReader::getFrnFromPath(const std::wstring& folderPath) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::wstring vol = folderPath.length() >= 2 ? folderPath.substr(0, 2) : L"";
    if (vol.empty()) return 0;
    
    const auto& volPathMap = m_pathToFrn[vol];
    // 2026-03-xx 极致性能优化：全量预计算后，此处检索复杂度严格为 O(1)
    auto it = volPathMap.find(folderPath);
    if (it != volPathMap.end()) {
        return it->second;
    }
    
    return 0; 
}

/**
 * @brief 实现并行文件名搜索 (std::execution::par)
 */
std::vector<FileEntry> MftReader::search(const std::wstring& query, const std::wstring& volume) {
    if (query.empty()) return {};

    // 2026-03-xx 极致性能重构：小写转换提前至外部
    std::wstring lQuery = query;
    std::transform(lQuery.begin(), lQuery.end(), lQuery.begin(), ::towlower);

    // 1. 收集所有待搜索项的指针
    std::vector<const FileEntry*> pool;
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!volume.empty()) {
            auto it = m_index.find(volume);
            if (it != m_index.end()) {
                for (auto& pair : it->second) pool.push_back(&pair.second);
            }
        } else {
            for (auto& volPair : m_index) {
                for (auto& pair : volPair.second) pool.push_back(&pair.second);
            }
        }
    }

    // 3. 并行过滤 (2026-03-xx 极致性能：消除循环内内存分配，直接匹配预存的小写字段)
    std::vector<FileEntry> results;
    std::mutex resultsMutex;

    std::for_each(std::execution::par, pool.begin(), pool.end(), [&](const FileEntry* entry) {
        if (entry->nameLower.find(lQuery) != std::wstring::npos) {
            std::lock_guard<std::mutex> lock(resultsMutex);
            results.push_back(*entry);
        }
    });

    return results;
}

/**
 * @brief USN 监听器更新内存索引，并同步维护反向索引 (极致性能：增量维护路径缓存)
 */
void MftReader::updateEntry(const FileEntry& entry) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    // 1. 维护主索引
    auto& volIndex = m_index[entry.volume];
    bool isNew = (volIndex.find(entry.frn) == volIndex.end());
    DWORDLONG oldParentFrn = isNew ? 0 : volIndex[entry.frn].parentFrn;
    
    FileEntry newEntry = entry;
    // 2026-03-xx 极致性能优化：维护预存储的小写名称
    newEntry.nameLower = newEntry.name;
    std::transform(newEntry.nameLower.begin(), newEntry.nameLower.end(), newEntry.nameLower.begin(), ::towlower);

    volIndex[entry.frn] = newEntry;

    // 2. 维护父子关系索引
    if (!isNew && oldParentFrn != entry.parentFrn) {
        // 如果移动了位置，从旧父节点移除
        auto& oldChildren = m_parentToChildren[entry.volume][oldParentFrn];
        oldChildren.erase(std::remove(oldChildren.begin(), oldChildren.end(), entry.frn), oldChildren.end());
    }
    
    if (isNew || oldParentFrn != entry.parentFrn) {
        m_parentToChildren[entry.volume][entry.parentFrn].push_back(entry.frn);
    }

    // 3. 增量维护路径缓存 (2026-03-xx 极致性能优化：采用异步延迟刷新，防止批量操作卡死)
    triggerPathRefresh(entry.volume);
}

void MftReader::triggerPathRefresh(const std::wstring& volume) {
    if (m_refreshPending[volume].exchange(true)) return; // 已有挂起的任务

    // 开启后台线程执行延时刷新
    std::thread([this, volume]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        precomputePaths(volume);
        m_refreshPending[volume] = false;
    }).detach();
}

std::wstring MftReader::getPathFromFrn(const std::wstring& volume, DWORDLONG frn) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto itVol = m_frnToPath.find(volume);
    if (itVol != m_frnToPath.end()) {
        auto itPath = itVol->second.find(frn);
        if (itPath != itVol->second.end()) {
            return itPath->second;
        }
    }
    return L"";
}

/**
 * @brief USN 监听器移除记录
 */
void MftReader::removeEntry(const std::wstring& volume, DWORDLONG frn) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto& volIndex = m_index[volume];
    auto it = volIndex.find(frn);
    if (it != volIndex.end()) {
        DWORDLONG parentFrn = it->second.parentFrn;
        // 从主索引移除
        volIndex.erase(it);
        // 从父子索引移除
        auto& children = m_parentToChildren[volume][parentFrn];
        children.erase(std::remove(children.begin(), children.end(), frn), children.end());
        // 清理路径缓存
        m_pathToFrn[volume].clear();
    }
}

} // namespace ArcMeta
