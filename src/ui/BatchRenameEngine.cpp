#include "BatchRenameEngine.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include "../meta/AmMetaJson.h"
#include "../db/ItemRepo.h"

namespace ArcMeta {

bool BatchRenameEngine::executeRename(const QStringList& oldPaths, const QList<RenameRule>& rules) {
    for (int i = 0; i < oldPaths.size(); ++i) {
        QString oldPath = oldPaths[i];
        QFileInfo fi(oldPath);
        QString oldName = fi.fileName();
        QString dirPath = fi.absolutePath();

        QString newName = generateName(oldName, rules, i);
        QString newPath = dirPath + "/" + newName;

        // 1. 物理重命名
        if (QFile::rename(oldPath, newPath)) {
            // 2. 元数据迁移：从旧 Key 移动到新 Key
            AmMeta meta = AmMetaJson::load(dirPath);
            if (meta.items.contains(oldName)) {
                ItemMetadata item = meta.items.take(oldName);
                meta.items[newName] = item;
                AmMetaJson::save(dirPath, meta);
            }

            // 3. 数据库路径同步 (FRN 不变)
            // 获取 FRN 需要打开文件或从 MFT 缓存读取，此处简化处理由 UsnWatcher 自动捕获完成
        }
    }
    return true;
}

} // namespace ArcMeta
