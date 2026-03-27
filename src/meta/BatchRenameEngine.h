#ifndef BATCH_RENAME_ENGINE_H
#define BATCH_RENAME_ENGINE_H

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace ArcMeta {

class BatchRenameEngine {
public:
    struct RenameComponent {
        enum Type { Text, Number, Date, Metadata };
        Type type;
        QString value; // 固定文本或格式串
        int startNum = 1;
        int step = 1;
        int padding = 0;
    };

    // 根据组件序列生成新文件名
    static QString generateName(const QString& originalName, const QList<RenameComponent>& components,
                               int index, const QVariantMap& metadata = {}) {
        QString newName;
        for (const auto& comp : components) {
            switch (comp.type) {
                case RenameComponent::Text:
                    newName += comp.value;
                    break;
                case RenameComponent::Number: {
                    int val = comp.startNum + index * comp.step;
                    newName += QString("%1").arg(val, comp.padding, 10, QChar('0'));
                    break;
                }
                case RenameComponent::Metadata:
                    if (metadata.contains(comp.value)) {
                        newName += metadata[comp.value].toString();
                    }
                    break;
                default: break;
            }
        }
        return newName;
    }
};

} // namespace ArcMeta

#endif // BATCH_RENAME_ENGINE_H
