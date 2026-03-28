#pragma once

#include <QIcon>
#include <QString>
#include <QColor>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>
#include "../../SvgIcons.h"

namespace ArcMeta {

/**
 * @brief UI 辅助类
 * 提供统一的图标渲染、样式计算等工具函数
 */
class UiHelper {
public:
    /**
     * @brief 获取带颜色的 SVG 图标 (返回 QIcon)
     */
    static QIcon getIcon(const QString& key, const QColor& color, int size = 18) {
        return QIcon(getPixmap(key, QSize(size, size), color));
    }

    /**
     * @brief 获取带颜色的 SVG Pixmap (返回 QPixmap)
     */
    static QPixmap getPixmap(const QString& key, const QSize& size, const QColor& color) {
        if (!SvgIcons::icons.contains(key)) return QPixmap();

        QString svgData = SvgIcons::icons[key];
        // 渲染前替换颜色占位符
        if (svgData.contains("currentColor")) {
            svgData.replace("currentColor", color.name());
        } else {
            // 如果原本没有 currentColor 占位符但指定了颜色，尝试注入
            svgData.replace("fill=\"none\"", QString("fill=\"%1\"").arg(color.name()));
            svgData.replace("stroke=\"currentColor\"", QString("stroke=\"%1\"").arg(color.name()));
        }
        
        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);
        
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        QSvgRenderer renderer(svgData.toUtf8());
        renderer.render(&painter);
        
        return pixmap;
    }
};

} // namespace ArcMeta
