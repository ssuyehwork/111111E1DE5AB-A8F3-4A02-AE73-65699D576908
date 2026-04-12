#pragma once

#include <string>
#include <vector>
#include <QString>

namespace ArcMeta {

/**
 * @brief 文件夹级别的元数据
 */
struct FolderMeta {
    std::wstring sortBy = L"name";
    std::wstring sortOrder = L"asc";
    int rating = 0;
    std::wstring color = L"";
    std::vector<std::wstring> tags;
    bool pinned = false;
    std::wstring note = L"";
    bool encrypted = false;
    std::string encryptSalt;
    std::string encryptIv;
    std::string encryptVerifyHash;

    bool isDefault() const {
        return sortBy == L"name" && sortOrder == L"asc" && rating == 0 &&
               color.empty() && tags.empty() && !pinned && note.empty() && !encrypted;
    }
};

/**
 * @brief 单个条目（文件或子文件夹）的元数据
 */
struct ItemMeta {
    std::wstring type = L"file"; // "file" | "folder"
    int rating = 0;
    std::wstring color = L"";
    std::vector<std::wstring> tags;
    bool pinned = false;
    std::wstring note = L"";
    bool encrypted = false;
    std::string encryptSalt;
    std::string encryptIv;
    std::string encryptVerifyHash;
    std::wstring originalName;
    std::wstring volume;
    std::wstring frn;

    bool hasUserOperations() const {
        return rating > 0 || !color.empty() || !tags.empty() || pinned ||
               !note.empty() || encrypted;
    }
};

} // namespace ArcMeta
