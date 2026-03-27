#ifndef BATCH_RENAME_ENGINE_H
#define BATCH_RENAME_ENGINE_H

#include <QStringList>
#include <QDateTime>
#include <QRegularExpression>

namespace ArcMeta {

struct RenameRule {
    enum Type { Text, Number, Date, Metadata };
    Type type;
    QString value;
};

class BatchRenameEngine {
public:
    static QString generateName(const QString& originalName, const QList<RenameRule>& rules, int index);

    // 执行物理重命名并迁移元数据
    static bool executeRename(const QStringList& oldPaths, const QList<RenameRule>& rules);
};

} // namespace ArcMeta

#endif // BATCH_RENAME_ENGINE_H
