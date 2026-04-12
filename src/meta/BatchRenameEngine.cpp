#include "BatchRenameEngine.h"
#include "MetadataManager.h"
#include <QFileInfo>
#include <QDateTime>
#include <filesystem>

namespace ArcMeta {

BatchRenameEngine& BatchRenameEngine::instance() {
    static BatchRenameEngine inst;
    return inst;
}

std::vector<std::wstring> BatchRenameEngine::preview(const std::vector<std::wstring>& originalPaths, const std::vector<RenameRule>& rules) {
    std::vector<std::wstring> results;
    for (size_t i = 0; i < originalPaths.size(); ++i) {
        results.push_back(processOne(originalPaths[i], (int)i, rules).toStdWString());
    }
    return results;
}

/**
 * @brief 管道模式处理单个文件名
 */
QString BatchRenameEngine::processOne(const std::wstring& path, int index, const std::vector<RenameRule>& rules) {
    QFileInfo info(QString::fromStdWString(path));
    QString newName = "";

    for (const auto& rule : rules) {
        switch (rule.type) {
            case RenameComponentType::Text:
                newName += rule.value;
                break;
            case RenameComponentType::Sequence: {
                int val = rule.start + (index * rule.step);
                newName += QString::number(val).rightJustified(rule.padding, '0');
                break;
            }
            case RenameComponentType::Date:
                newName += QDateTime::currentDateTime().toString(rule.value.isEmpty() ? "yyyyMMdd" : rule.value);
                break;
            case RenameComponentType::OriginalName:
                newName += info.baseName();
                break;
            case RenameComponentType::Metadata:
                // 注入 ArcMeta 元数据标记（如评级星级）
                newName += "[ArcMeta]"; 
                break;
        }
    }

    // 保留原始后缀
    QString ext = info.suffix();
    if (!ext.isEmpty()) newName += "." + ext;

    return newName;
}

/**
 * @brief 执行物理重命名（红线：重命名成功后必须迁移元数据）
 */
bool BatchRenameEngine::execute(const std::vector<std::wstring>& originalPaths, const std::vector<RenameRule>& rules) {
    auto newNames = preview(originalPaths, rules);
    
    for (size_t i = 0; i < originalPaths.size(); ++i) {
        std::filesystem::path oldP(originalPaths[i]);
        std::filesystem::path newP = oldP.parent_path() / newNames[i];
        
        try {
            std::filesystem::rename(oldP, newP);
            // 2026-05-24 按照用户要求：彻底移除 JSON 逻辑。重命名成功后，仅需更新数据库和内存缓存。
            // 同步更新内存缓存
            MetadataManager::instance().renameItem(oldP.wstring(), newP.wstring());
        } catch (...) {
            return false; // 任一失败则中断（实际生产应支持回滚）
        }
    }
    return true;
}

} // namespace ArcMeta
