#ifndef CONFIG_REPO_H
#define CONFIG_REPO_H

#include <QString>
#include <QVariant>

namespace ArcMeta {

class ConfigRepo {
public:
    static bool setConfig(const QString& key, const QString& value);
    static QString getConfig(const QString& key, const QString& defaultValue = "");

    // 便捷接口：存储面板宽度数组
    static void savePanelWidths(const QList<int>& widths);
    static QList<int> loadPanelWidths();
};

} // namespace ArcMeta

#endif // CONFIG_REPO_H
