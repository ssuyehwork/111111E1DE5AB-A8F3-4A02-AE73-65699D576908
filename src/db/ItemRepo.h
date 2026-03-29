#pragma once

#include "Database.h"
#include "../meta/AmMetaJson.h"
#include <string>
#include <vector>
#include <QSqlDatabase>

namespace ArcMeta {

/**
 * @brief 文件条目持久层
 */
class ItemRepo {
public:
    static bool save(const std::wstring& parentPath, const std::wstring& name, const ItemMeta& meta, QSqlDatabase db = QSqlDatabase::database());
    static bool removeByFrn(const std::wstring& volume, const std::wstring& frn, QSqlDatabase db = QSqlDatabase::database());
    static bool markAsDeleted(const std::wstring& volume, const std::wstring& frn, QSqlDatabase db = QSqlDatabase::database());
    
    /**
     * @brief 通过 FRN 获取当前数据库记录的路径
     */
    static std::wstring getPathByFrn(const std::wstring& volume, const std::wstring& frn, QSqlDatabase db = QSqlDatabase::database());

    /**
     * @brief 物理更新路径
     */
    static bool updatePath(const std::wstring& volume, const std::wstring& frn, const std::wstring& newPath, const std::wstring& newParentPath, QSqlDatabase db = QSqlDatabase::database());

    /**
     * @brief 极致性能：批量获取目录下所有条目的元数据
     */
    static std::map<std::wstring, ItemMeta> getMetadataBatch(const std::wstring& parentPath, QSqlDatabase db = QSqlDatabase::database());
};

} // namespace ArcMeta
