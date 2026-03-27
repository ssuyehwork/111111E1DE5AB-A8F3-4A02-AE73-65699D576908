#ifndef FILE_ITEM_DELEGATE_H
#define FILE_ITEM_DELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QApplication>
#include <QFileSystemModel>
#include <QCache>
#include "IconHelper.h"
#include "ThumbnailProvider.h"

namespace ArcMeta {

class FileItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        // 绘制选中背景
        if (option.state & QStyle::State_Selected) {
            painter->fillRect(option.rect, option.palette.highlight());
        }

        // 1. 绘制缩略图或图标 (64x64)
        QRect iconRect = option.rect.adjusted(8, 8, -8, -40);

        QString path;
        // 尝试获取文件系统路径
        if (const auto* model = qobject_cast<const QFileSystemModel*>(index.model())) {
            path = model->filePath(index);
        }

        bool drawn = false;
        if (!path.isEmpty()) {
            QPixmap* cached = m_thumbCache.object(path);
            if (cached) {
                painter->drawPixmap(iconRect, *cached);
                drawn = true;
            } else {
                // 异步渲染缩略图（此处简化为按需获取并缓存，实际应在 worker thread 中处理）
                QPixmap thumb = ThumbnailProvider::getThumbnail(path, 64);
                if (!thumb.isNull()) {
                    m_thumbCache.insert(path, new QPixmap(thumb));
                    painter->drawPixmap(iconRect, thumb);
                    drawn = true;
                }
            }
        }

        if (!drawn) {
            QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
            if (!icon.isNull()) {
                icon.paint(painter, iconRect);
            }
        }

        // 2. 绘制星级 (图标下方)
        int rating = index.data(Qt::UserRole + 1).toInt();
        if (rating > 0) {
            painter->setPen(QColor("#EF9F27"));
            QFont starFont = painter->font();
            starFont.setPointSize(11);
            painter->setFont(starFont);
            QString stars = QString(rating, QChar(0x2605));
            QRect starRect = option.rect.adjusted(0, iconRect.bottom() + 2, 0, -32);
            painter->drawText(starRect, Qt::AlignCenter, stars);
        }

        // 3. 绘制颜色圆点 (右下角)
        QString colorHex = index.data(Qt::UserRole + 2).toString();
        if (!colorHex.isEmpty()) {
            painter->setBrush(QColor(colorHex));
            painter->setPen(Qt::NoPen);
            QRect dotRect(iconRect.right() - 10, iconRect.bottom() - 10, 8, 8);
            painter->drawEllipse(dotRect);
        }

        // 4. 绘制文件名 (底部 32px)
        painter->setPen(option.palette.text().color());
        QRect textRect = option.rect.adjusted(4, option.rect.height() - 32, -4, -4);
        QString text = index.data(Qt::DisplayRole).toString();
        painter->drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap,
                         painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()));

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option); Q_UNUSED(index);
        return QSize(80, 112);
    }

private:
    mutable QCache<QString, QPixmap> m_thumbCache;
};

} // namespace ArcMeta

#endif // FILE_ITEM_DELEGATE_H
