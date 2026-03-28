#ifndef ICONHELPER_H
#define ICONHELPER_H

#include <QIcon>
#include <QPainter>
#include <QSvgRenderer>
#include <QPixmap>
#include "SvgIcons.h"

class IconHelper {
public:
    static QIcon getIcon(const QString& name, const QString& color = "#aaaaaa", int size = 18) {
        QString svgData = SvgIcons::icons.value(name);
        if (svgData.isEmpty()) {
            if (name == "folder") svgData = SvgIcons::icons.value("folder");
            else svgData = SvgIcons::icons.value("file");
        }

        // 替换颜色
        svgData.replace("currentColor", color);

        QSvgRenderer renderer(svgData.toUtf8());
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        return QIcon(pixmap);
    }
};

#endif // ICONHELPER_H
