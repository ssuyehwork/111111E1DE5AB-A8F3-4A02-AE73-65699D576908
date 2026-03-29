#include "MftReader.h"
#include "PathBuilder.h"
#include <winioctl.h>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <execution>
#include <mutex>
#include <stack>

namespace ArcMeta {

// 2026-03-xx：规范化常量定义
constexpr DWORDLONG NTFS_ROOT_FRN = 5;

MftReader& MftReader::instance() {
    static MftReader inst;
    return inst;
}

/**
 * @brief 构建全盘索引
 */
void MftReader::buildIndex() {
    std::unordered_map<std::wstring, std::unordered_map<DWORDLONG, FileEntry>> localIndex;
    std::unordered_map<std::wstring, std::unordered_map<DWORDLONG, std::vector<DWORDLONG>>> localParentToChildren;
    std::unordered_map<std::wstring, std::unordered_map<std::wstring, DWORDLONG>> localPathToFrn;
    std::unordered_map<std::wstring, FileEntry> localPathIndex;
    bool usingMft = false;

    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (drives & (1 << i)) {
            wchar_t driveLetter = (wchar_t)(L'A' + i);
            std::wstring volumeName = std::wstring(1, driveLetter) + L":";
            
            wchar_t driveRoot[] = { driveLetter, L':', L'\\', L'\0' };
            if (GetDriveTypeW(driveRoot) == DRIVE_FIXED) {
                if (loadMftForVolumeInternal(volumeName, localIndex, localParentToChildren)) {
                    usingMft = true;
                    prewarmPathMapping(volumeName, localIndex[volumeName], localParentToChildren[volumeName], localPathToFrn[volumeName]);
                } else {
                    scanDirectoryFallbackInternal(volumeName, localPathIndex);
                }
            }
        }
    }

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_index = std::move(localIndex);
        m_parentToChildren = std::move(localParentToChildren);
        m_pathIndex = std::move(localPathIndex);
        m_pathToFrn = std::move(localPathToFrn);
        m_isUsingMft = usingMft;
    }
}

void MftReader::prewarmPathMapping(const std::wstring& volume,
    const std::unordered_map<DWORDLONG, FileEntry>& volumeIndex,
    const std::unordered_map<DWORDLONG, std::vector<DWORDLONG>>& parentToChildren,
    std::unordered_map<std::wstring, DWORDLONG>& outPathToFrn) {

    struct StackNode {
        DWORDLONG frn;
        std::wstring path;
    };
    std::stack<StackNode> s;
    s.push({ NTFS_ROOT_FRN, volume + L"\\" });
    outPathToFrn[volume + L"\\"] = NTFS_ROOT_FRN;
    outPathToFrn[volume] = NTFS_ROOT_FRN;

    while (!s.empty()) {
        StackNode current = s.top();
        s.pop();

        auto itChildren = parentToChildren.find(current.frn);
        if (itChildren != parentToChildren.end()) {
            for (DWORDLONG childFrn : itChildren->second) {
                auto itEntry = volumeIndex.find(childFrn);
                if (itEntry != volumeIndex.end() && itEntry->second.isDir()) {
                    std::wstring childPath = current.path + itEntry->second.name + L"\\";
                    outPathToFrn[childPath] = childFrn;
                    outPathToFrn[current.path + itEntry->second.name] = childFrn;
                    s.push({ childFrn, childPath });
                }
            }
        }
    }
}

bool MftReader::loadMftForVolumeInternal(const std::wstring& volumeName,
    std::unordered_map<std::wstring, std::unordered_map<DWORDLONG, FileEntry>>& outIndex,
    std::unordered_map<std::wstring, std::unordered_map<DWORDLONG, std::vector<DWORDLONG>>>& outParentToChildren) {

    std::wstring path = L"\\\\.\\" + volumeName;
    HANDLE hVol = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hVol == INVALID_HANDLE_VALUE) return false;

    USN_JOURNAL_DATA journalData; DWORD cb;
    if (!DeviceIoControl(hVol, FSCTL_QUERY_USN_JOURNAL, NULL, 0, &journalData, sizeof(journalData), &cb, NULL)) {
        CloseHandle(hVol); return false;
    }

    MFT_ENUM_DATA enumData = {0};
    enumData.StartFileReferenceNumber = 0;
    enumData.LowUsn = 0;
    enumData.HighUsn = journalData.NextUsn;

    const int BUF_SIZE = 64 * 1024;
    std::vector<BYTE> buffer(BUF_SIZE);
    
    auto& volumeIndex = outIndex[volumeName];
    volumeIndex.reserve(1000000);
    auto& childrenMap = outParentToChildren[volumeName];

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
            childrenMap[entry.parentFrn].push_back(entry.frn);
            pData += pRecord->RecordLength;
        }
        enumData.StartFileReferenceNumber = ((USN_RECORD_V2*)buffer.data())->FileReferenceNumber;
    }
    CloseHandle(hVol);
    return true;
}

void MftReader::scanDirectoryFallbackInternal(const std::wstring& volumeName,
    std::unordered_map<std::wstring, FileEntry>& outPathIndex) {
    try {
        std::wstring rootPath = volumeName + L"\\";
        for (const auto& entry : std::filesystem::directory_iterator(rootPath, std::filesystem::directory_options::skip_permission_denied)) {
            FileEntry fe; fe.volume = volumeName; fe.name = entry.path().filename().wstring();
            fe.attributes = entry.is_directory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
            outPathIndex[entry.path().wstring()] = fe;
        }
    } catch (...) {}
}

std::vector<FileEntry> MftReader::getChildren(const std::wstring& folderPath) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<FileEntry> results;
    std::wstring vol = folderPath.length() >= 2 ? folderPath.substr(0, 2) : L"";
    if (vol.empty()) return results;

    if (m_isUsingMft) {
        DWORDLONG parentFrn = getFrnFromPath(folderPath);
        if (parentFrn != 0) {
            auto itVol = m_parentToChildren.find(vol);
            if (itVol != m_parentToChildren.end()) {
                auto itParent = itVol->second.find(parentFrn);
                if (itParent != itVol->second.end()) {
                    auto& entries = m_index[vol];
                    for (DWORDLONG childFrn : itParent->second) {
                        auto itEntry = entries.find(childFrn);
                        if (itEntry != entries.end()) results.push_back(itEntry->second);
                    }
                    return results;
                }
            }
        }
    }

    try {
        std::filesystem::path p(folderPath);
        if (std::filesystem::exists(p) && std::filesystem::is_directory(p)) {
            for (const auto& entry : std::filesystem::directory_iterator(p, std::filesystem::directory_options::skip_permission_denied)) {
                FileEntry fe; fe.volume = vol; fe.name = entry.path().filename().wstring();
                fe.attributes = entry.is_directory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
                results.push_back(fe);
            }
        }
    } catch (...) {}
    return results;
}

DWORDLONG MftReader::getFrnFromPath(const std::wstring& folderPath) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::wstring vol = folderPath.length() >= 2 ? folderPath.substr(0, 2) : L"";
    if (vol.empty()) return 0;
    std::wstring normPath = folderPath;
    if (normPath.length() > 3 && (normPath.back() == L'\\' || normPath.back() == L'/')) normPath.pop_back();

    auto itVol = m_pathToFrn.find(vol);
    if (itVol != m_pathToFrn.end()) {
        auto it = itVol->second.find(normPath);
        if (it != itVol->second.end()) return it->second;
    }
    if (normPath.length() <= 3) return NTFS_ROOT_FRN;
    return 0; 
}

std::vector<FileEntry> MftReader::search(const std::wstring& query, const std::wstring& volume) {
    if (query.empty()) return {};
    std::vector<const FileEntry*> pool;
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!volume.empty()) {
            auto it = m_index.find(volume);
            if (it != m_index.end()) { for (auto& pair : it->second) pool.push_back(&pair.second); }
        } else {
            for (auto& volPair : m_index) { for (auto& pair : volPair.second) pool.push_back(&pair.second); }
        }
    }
    if (pool.empty()) return {};
    std::wstring lQuery = query;
    std::transform(lQuery.begin(), lQuery.end(), lQuery.begin(), ::towlower);
    std::vector<FileEntry> results; std::mutex resultsMutex;
    std::for_each(std::execution::par, pool.begin(), pool.end(), [&](const FileEntry* entry) {
        std::wstring lName = entry->name; std::transform(lName.begin(), lName.end(), lName.begin(), ::towlower);
        if (lName.find(lQuery) != std::wstring::npos) { std::lock_guard<std::mutex> lock(resultsMutex); results.push_back(*entry); }
    });
    return results;
}

void MftReader::updateEntry(const FileEntry& entry) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto& volIndex = m_index[entry.volume];
    bool isNew = (volIndex.find(entry.frn) == volIndex.end());
    DWORDLONG oldParentFrn = isNew ? 0 : volIndex[entry.frn].parentFrn;
    volIndex[entry.frn] = entry;
    if (!isNew && oldParentFrn != entry.parentFrn) {
        auto& oldChildren = m_parentToChildren[entry.volume][oldParentFrn];
        oldChildren.erase(std::remove(oldChildren.begin(), oldChildren.end(), entry.frn), oldChildren.end());
    }
    if (isNew || oldParentFrn != entry.parentFrn) m_parentToChildren[entry.volume][entry.parentFrn].push_back(entry.frn);
    m_pathToFrn[entry.volume].clear();
}

void MftReader::removeEntry(const std::wstring& volume, DWORDLONG frn) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto& volIndex = m_index[volume];
    auto it = volIndex.find(frn);
    if (it != volIndex.end()) {
        DWORDLONG parentFrn = it->second.parentFrn;
        volIndex.erase(it);
        auto& children = m_parentToChildren[volume][parentFrn];
        children.erase(std::remove(children.begin(), children.end(), frn), children.end());
        m_pathToFrn[volume].clear();
    }
}

} // namespace ArcMeta
