#ifndef ICON_HELPER_H
#define ICON_HELPER_H

#include <QIcon>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>
#include "../SvgIcons.h"

namespace ArcMeta {

class IconHelper {
public:
    // 获取指定名称和颜色的 SVG 图标
    static QIcon getIcon(const QString& name, const QString& color = "#aaaaaa", int size = 24) {
        if (!SvgIcons::icons.contains(name)) {
            return QIcon();
        }

        QString svgData = SvgIcons::icons.value(name);
        // 简单替换颜色占位符（假设 SVG 中使用了 stroke="currentColor" 或 fill="currentColor"）
        svgData.replace("currentColor", color);

        QSvgRenderer renderer(svgData.toUtf8());
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        renderer.render(&painter);

        return QIcon(pixmap);
    }
};

} // namespace ArcMeta

#endif // ICON_HELPER_H
