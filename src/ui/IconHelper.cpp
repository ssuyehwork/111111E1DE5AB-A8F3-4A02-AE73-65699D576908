#include "IconHelper.h"
#include <QFileIconProvider>
#include <QFileInfo>
#include <QPainter>
#include <QColor>
#include <QCache>

namespace ArcMeta {

// 2026-03-27 按照用户要求：静态化 Provider 以提升海量图标抓取性能，防止 UI 卡顿
static QFileIconProvider& globalProvider() {
    static QFileIconProvider provider;
    return provider;
}

QIcon IconHelper::getFolderIcon(const QString& path, bool isEmpty) {
    // 2026-03-27 按照用户要求：直接抓取 Windows 系统的原生图标
    QIcon originalIcon = globalProvider().icon(QFileInfo(path));

    if (!isEmpty) {
        return originalIcon;
    }

    // [逻辑增强] 2026-03-27 针对空文件夹执行置灰
    QPixmap pixmap = originalIcon.pixmap(64, 64);
    QImage img = pixmap.toImage();
    QImage grayImg = applyGrayscaleFilter(img);

    return QIcon(QPixmap::fromImage(grayImg));
}

QImage IconHelper::applyGrayscaleFilter(const QImage& source) {
    if (source.isNull()) return source;

    // 2026-03-27 深度细节：确保转换格式以处理 Alpha 通道，保留原生图标透明度
    QImage result = source.convertToFormat(QImage::Format_ARGB32);

    for (int y = 0; y < result.height(); ++y) {
        QRgb* line = (QRgb*)result.scanLine(y);
        for (int x = 0; x < result.width(); ++x) {
            QRgb& pixel = line[x];
            int a = qAlpha(pixel);
            if (a == 0) continue;

            // 银灰色算法：去色 + 提升明度（符合银灰色视觉感）
            int gray = qGray(pixel);
            gray = qMin(255, (int)(gray * 1.25)); // 2026-03-27 细节微调：提亮系数由 1.2 升至 1.25 以增强银色质感

            pixel = qRgba(gray, gray, gray, a);
        }
    }
    return result;
}

} // namespace ArcMeta
