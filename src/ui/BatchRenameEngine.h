#ifndef BATCH_RENAME_ENGINE_H
#define BATCH_RENAME_ENGINE_H

#include <QStringList>
#include <QDateTime>
#include <QRegularExpression>

namespace ArcMeta {

struct RenameRule {
    enum Type { Text, Number, Date, Metadata };
    Type type;
    QString value; // 固定文本、起始数字、日期格式等
};

class BatchRenameEngine {
public:
    static QString generateName(const QString& originalName, const QList<RenameRule>& rules, int index) {
        QString newName;
        for (const auto& rule : rules) {
            switch (rule.type) {
                case RenameRule::Text:
                    newName += rule.value;
                    break;
                case RenameRule::Number:
                    newName += QString::number(rule.value.toInt() + index).rightJustified(3, '0');
                    break;
                case RenameRule::Date:
                    newName += QDateTime::currentDateTime().toString(rule.value);
                    break;
                case RenameRule::Metadata:
                    // 演示逻辑，实际需从 JSON/DB 读取
                    newName += "[Meta]";
                    break;
            }
        }
        return newName;
    }
};

} // namespace ArcMeta

#endif // BATCH_RENAME_ENGINE_H
