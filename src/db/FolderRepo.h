#pragma once

#include "Database.h"
#include "../meta/AmMetaJson.h"
#include <string>
#include <vector>
#include <QSqlDatabase>

namespace ArcMeta {

/**
 * @brief 文件夹元数据持久层
 */
class FolderRepo {
public:
    /**
     * @brief 保存或更新文件夹元数据
     */
    static bool save(const std::wstring& path, const FolderMeta& meta, QSqlDatabase db = QSqlDatabase::database());

    /**
     * @brief 获取文件夹元数据
     */
    static bool get(const std::wstring& path, FolderMeta& meta, QSqlDatabase db = QSqlDatabase::database());

    /**
     * @brief 删除文件夹记录
     */
    static bool remove(const std::wstring& path, QSqlDatabase db = QSqlDatabase::database());
};

} // namespace ArcMeta
