#pragma once

#include <QIcon>
#include <QString>
#include <QImage>

namespace ArcMeta {

/**
 * @brief 图标辅助类，负责原生 Shell 图标的抓取与置灰处理
 */
class IconHelper {
public:
    /**
     * @brief 抓取 Windows 原生文件夹图标
     * @param path 文件夹物理路径
     * @param isEmpty 是否判定为空（为空则应用置灰滤镜）
     */
    static QIcon getFolderIcon(const QString& path, bool isEmpty);

    /**
     * @brief 2026-03-27 按照用户要求：对原生图标应用“银灰色”滤镜
     * 核心逻辑：保持形状细节，仅调整饱和度与亮度
     */
    static QImage applyGrayscaleFilter(const QImage& source);
};

} // namespace ArcMeta
