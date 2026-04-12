#pragma once

#include <QIcon>
#include <QString>
#include <QColor>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>
#include <QMap>
#include <QCache>
#include <QSettings>
#include <QFileInfo>
#include <QImage>
#include <QStringList>

// Windows Shell 缩略图引擎依赖
#ifdef Q_OS_WIN
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl_core.h>
#include <shlwapi.h>
#include <thumbcache.h>
#endif

#include "../../SvgIcons.h"

namespace ArcMeta {

/**
 * @brief UI 辅助类 (内联实现版 - 全量支持图片/图形识别)
 */
class UiHelper {
public:
    /**
     * @brief 判断是否为需要提取缩略图的图片/图形格式
     * 2026-04-11 按照用户要求：全量覆盖所有常见及专业图片格式
     */
    static bool isGraphicsFile(const QString& ext) {
        static const QStringList graphicsExts = {
            "png", "jpg", "jpeg", "bmp", "gif", "webp", "ico", "tiff", "tif", // 基础位图
            "psd", "psb", "ai", "eps", "pdf", "svg", "cdr"                   // 专业/矢量
        };
        return graphicsExts.contains(ext.toLower());
    }

    /**
     * @brief 获取带颜色的 SVG 图标
     */
    static QIcon getIcon(const QString& key, const QColor& color, int size = 18) {
        return QIcon(getPixmap(key, QSize(size, size), color));
    }

    /**
     * @brief 获取带颜色的 SVG Pixmap
     */
    static QPixmap getPixmap(const QString& key, const QSize& size, const QColor& color) {
        static QCache<QString, QPixmap> s_renderCache(500);
        QString cKey = QString("%1_%2_%3_%4").arg(key).arg(size.width()).arg(size.height()).arg(color.rgba());
        if (s_renderCache.contains(cKey)) return *s_renderCache.object(cKey);
        if (!SvgIcons::icons.contains(key)) return QPixmap();

        QString svgData = SvgIcons::icons[key];
        if (svgData.contains("currentColor")) {
            svgData.replace("currentColor", color.name());
        } else {
            svgData.replace("fill=\"none\"", QString("fill=\"%1\"").arg(color.name()));
            svgData.replace("stroke=\"currentColor\"", QString("stroke=\"%1\"").arg(color.name()));
        }
        
        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        QSvgRenderer renderer(svgData.toUtf8());
        renderer.render(&painter);
        s_renderCache.insert(cKey, new QPixmap(pixmap));
        return pixmap;
    }

    /**
     * @brief 获取扩展名对应的颜色
     */
    static QColor getExtensionColor(const QString& ext) {
        static QMap<QString, QColor> s_cache;
        QString upperExt = ext.toUpper();
        if (upperExt == "DIR") return QColor(45, 65, 85, 200);
        if (upperExt.isEmpty()) return QColor(60, 60, 60, 180);
        if (s_cache.contains(upperExt)) return s_cache[upperExt];

        QSettings settings("ArcMeta团队", "ArcMeta");
        QString settingKey = QString("ExtensionColors/%1").arg(upperExt);
        if (settings.contains(settingKey)) {
            QColor color = settings.value(settingKey).value<QColor>();
            s_cache[upperExt] = color;
            return color;
        }

        size_t hash = qHash(upperExt);
        int hue = static_cast<int>(hash % 360);
        QColor color = QColor::fromHsl(hue, 160, 110, 200); 
        s_cache[upperExt] = color;
        settings.setValue(settingKey, color);
        return color;
    }

    /**
     * @brief 利用 Shell 提取文件高清缩略图
     */
    static QPixmap getShellThumbnail(const QString& path, int size, bool forceMirror = false) {
#ifdef Q_OS_WIN
        PIDLIST_ABSOLUTE pidl = nullptr;
        HRESULT hr = SHParseDisplayName(path.toStdWString().c_str(), nullptr, &pidl, 0, nullptr);
        if (FAILED(hr)) return QPixmap();

        IShellItem* pItem = nullptr;
        hr = SHCreateItemFromIDList(pidl, IID_IShellItem, (void**)&pItem);
        ILFree(pidl);

        if (SUCCEEDED(hr)) {
            IShellItemImageFactory* pFactory = nullptr;
            hr = pItem->QueryInterface(IID_IShellItemImageFactory, (void**)&pFactory);
            if (SUCCEEDED(hr)) {
                SIZE nativeSize = { size, size };
                HBITMAP hBitmap = nullptr;
                // SIIGBF_THUMBNAILONLY: 仅提取高质量缩略图内容，不带图标装饰
                // SIIGBF_RESIZETOFIT: 确保比例正确
                hr = pFactory->GetImage(nativeSize, SIIGBF_THUMBNAILONLY | SIIGBF_RESIZETOFIT, &hBitmap);
                if (SUCCEEDED(hr) && hBitmap) {
                    // 2026-04-11 按照用户要求：引入分离适配机制，对取自缓存的缩略图单独进行翻转扶正
                    QImage img = QImage::fromHBITMAP(hBitmap);
                    if (forceMirror) {
                        img = img.mirrored(false, true);
                    }
                    QPixmap pix = QPixmap::fromImage(img);
                    DeleteObject(hBitmap);
                    pFactory->Release();
                    pItem->Release();
                    return pix;
                }
                pFactory->Release();
            }
            pItem->Release();
        }
#else
        Q_UNUSED(path); Q_UNUSED(size);
#endif
        return QPixmap();
    }
};

} // namespace ArcMeta
