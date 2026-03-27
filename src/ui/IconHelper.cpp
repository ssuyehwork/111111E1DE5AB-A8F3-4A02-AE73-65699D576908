#include "IconHelper.h"
#include <QFileIconProvider>
#include <QFileInfo>
#include <QPainter>
#include <QColor>

namespace ArcMeta {

QIcon IconHelper::getFolderIcon(const QString& path, bool isEmpty) {
    // 2026-03-27 按照用户要求：使用系统自带的原生图标
    QFileIconProvider provider;
    QIcon originalIcon = provider.icon(QFileInfo(path));

    if (!isEmpty) {
        return originalIcon;
    }

    // [核心逻辑] 2026-03-27 针对空文件夹执行动态置灰
    QPixmap pixmap = originalIcon.pixmap(64, 64);
    QImage img = pixmap.toImage();
    QImage grayImg = applyGrayscaleFilter(img);

    return QIcon(QPixmap::fromImage(grayImg));
}

QImage IconHelper::applyGrayscaleFilter(const QImage& source) {
    if (source.isNull()) return source;

    QImage result = source.convertToFormat(QImage::Format_ARGB32);

    for (int y = 0; y < result.height(); ++y) {
        QRgb* line = (QRgb*)result.scanLine(y);
        for (int x = 0; x < result.width(); ++x) {
            QRgb& pixel = line[x];
            int a = qAlpha(pixel);
            if (a == 0) continue;

            // 2026-03-27 银灰色算法：灰度化 + 适当提亮
            // 公式：L = 0.299R + 0.587G + 0.114B
            int gray = qGray(pixel);

            // 提亮 20% 以获得更好的“银灰”质感（#A0A0A0 风格）
            gray = qMin(255, (int)(gray * 1.2));

            pixel = qRgba(gray, gray, gray, a);
        }
    }
    return result;
}

} // namespace ArcMeta
