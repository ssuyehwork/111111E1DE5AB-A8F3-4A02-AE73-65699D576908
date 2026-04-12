#pragma once

#include "Database.h"
#include "../meta/MetadataDefs.h"
#include <string>
#include <vector>

namespace ArcMeta {

/**
 * @brief 文件条目持久层
 */
class ItemRepo {
public:
    static bool save(const std::wstring& parentPath, const std::wstring& name, const ItemMeta& meta);
    
    /**
     * @brief 2026-05-24 按照用户要求：仅保存/更新磁盘物理属性（Mtime等），不触碰用户元数据
     */
    static bool saveBasicInfo(const std::wstring& volume, const std::wstring& frn, const std::wstring& path, const std::wstring& parentPath, bool isDir, double mtime, double size);

    static bool removeByFrn(const std::wstring& volume, const std::wstring& frn);
    static bool markAsDeleted(const std::wstring& volume, const std::wstring& frn);
    
    /**
     * @brief 通过 FRN 获取当前数据库记录的路径
     */
    static std::wstring getPathByFrn(const std::wstring& volume, const std::wstring& frn);

    /**
     * @brief 物理更新路径
     */
    static bool updatePath(const std::wstring& volume, const std::wstring& frn, const std::wstring& newPath, const std::wstring& newParentPath);

    /**
     * @brief 获取当前数据库中的总记录数
     */
    static int count();

    /**
     * @brief 2026-04-12 按照用户要求：基于数据库的文件名关键词搜索
     * @param keyword 搜索关键词
     * @param parentPath 局部搜索时指定父路径，为空则全局搜索
     * @return 匹配的文件物理路径列表（最多 300 条）
     */
    static QStringList searchByKeyword(const QString& keyword, const QString& parentPath = "");
};

} // namespace ArcMeta
