#include "FileItemDelegate.h"
#include <QLineEdit>
#include <QMouseEvent>
#include <QFile>
#include <QDir>
#include "../meta/AmMetaJson.h"
#include "../core/SuffixColorManager.h"

FileItemDelegate::FileItemDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void FileItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    if (!index.isValid()) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    bool isSelected = (option.state & QStyle::State_Selected);
    // bool isHovered = (option.state & QStyle::State_MouseOver);

    QRect rect = option.rect.adjusted(4, 4, -4, -4);
    
    // 1. 绘制背景与边框
    QPainterPath borderPath;
    borderPath.addRoundedRect(rect, 8, 8);
    
    if (isSelected) {
        painter->setPen(QPen(QColor("#3498db"), 2));
        painter->setBrush(QColor(40, 40, 40)); // 深色背景
        painter->drawPath(borderPath);
    } else {
        painter->setPen(QPen(QColor("#333333"), 1));
        painter->setBrush(QColor(45, 45, 45));
        painter->drawPath(borderPath);
    }

    // 2. 绘制左上角 Badge (后缀名)
    QString path = index.data(PathRole).toString();
    QFileInfo info(path);
    QString suffix = info.suffix().toUpper();
    if (info.isDir()) suffix = "DIR";
    if (suffix.isEmpty()) suffix = "FILE";

    if (!suffix.isEmpty()) {
        // 2026-03-xx 按照要求：使用动态后缀颜色管理器
        QColor badgeColor = SuffixColorManager::instance().getColor(suffix);
        QRect badgeRect(rect.left() + 8, rect.top() + 8, 32, 18);
        painter->setPen(Qt::NoPen);
        painter->setBrush(badgeColor);
        painter->drawRoundedRect(badgeRect, 4, 4);
        
        painter->setPen(badgeColor.lightness() > 180 ? Qt::black : Qt::white);
        QFont badgeFont = painter->font();
        badgeFont.setPointSize(8);
        badgeFont.setBold(true);
        painter->setFont(badgeFont);
        painter->drawText(badgeRect, Qt::AlignCenter, suffix);
    }

    // 3. 绘制主图标
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    int iconDisplaySize = option.decorationSize.width();
    if (iconDisplaySize <= 0) iconDisplaySize = 64;
    
    // 计算图标居中位置 (极致紧凑：顶边距 30 -> 8)
    QRect iconRect(rect.left() + (rect.width() - iconDisplaySize) / 2,
                   rect.top() + 8,
                   iconDisplaySize, iconDisplaySize);
    icon.paint(painter, iconRect, Qt::AlignCenter);

    // 4. 绘制星级与锁定图标 (图标下方)
    // 2026-03-xx 按照用户要求：整体上移 7 像素，间距由 +2 变为 -5
    int rating = index.data(RatingRole).toInt();
    bool isLocked = index.data(IsLockedRole).toBool();
    
    int metaY = iconRect.bottom() - 5;
    QRect metaRect(rect.left() + 10, metaY, rect.width() - 20, 20);
    
    // 居中计算评分区域总宽度：[禁止图标 18px] + [锁定图标 20px (如有)] + [5颗星 90px]
    int totalWidth = 18 + (isLocked ? 20 : 0) + (5 * 18);
    int startX = metaRect.left() + (metaRect.width() - totalWidth) / 2;

    // 1. 绘制“禁止/无评级”图标 (始终显示在最左侧)
    QPixmap banPix = IconHelper::getIcon("no_color", "#555555", 14).pixmap(14, 14);
    painter->drawPixmap(startX, metaRect.top() + 3, banPix);
    int currentX = startX + 18;

    // 2. 绘制锁定图标 (如果有)
    if (isLocked) {
        QPixmap lockPix = IconHelper::getIcon("lock", "#888", 14).pixmap(14, 14);
        painter->drawPixmap(currentX, metaRect.top() + 3, lockPix);
        currentX += 20;
    }

    // 3. 绘制星级
    QFont starFont = painter->font();
    starFont.setPointSize(12);
    painter->setFont(starFont);
    for (int i = 0; i < 5; ++i) {
        painter->setPen(i < rating ? QColor("#F2B705") : QColor("#444444"));
        painter->drawText(currentX + i * 18, metaRect.top() + 15, i < rating ? "★" : "☆");
    }

    // 5. 绘制文件名区域 (最底部)
    QString fileName = index.data(Qt::DisplayRole).toString();
    QString tagColorStr = index.data(ColorRole).toString();
    QRect nameBoxRect(rect.left() + 6, metaRect.bottom() + 2, rect.width() - 12, 20);
    
    if (!tagColorStr.isEmpty()) {
        // 仅当设定了颜色标签时才显示背景色块 (圆角由 6px 修改成 4px)
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(tagColorStr));
        painter->drawRoundedRect(nameBoxRect, 4, 4);
        painter->setPen(Qt::black); // 有背景时用黑色文字
    } else {
        painter->setPen(QColor("#CCCCCC")); // 无背景时用亮灰色文字
    }
    
    QFont nameFont = painter->font();
    nameFont.setPointSize(9);
    nameFont.setBold(true);
    painter->setFont(nameFont);
    painter->drawText(nameBoxRect.adjusted(4, 0, -4, 0), Qt::AlignCenter, 
                     painter->fontMetrics().elidedText(fileName, Qt::ElideMiddle, nameBoxRect.width() - 8));

    painter->restore();
}

QSize FileItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(index);
    int w = option.decorationSize.width();
    if (w <= 0) w = 96;
    // 极致紧凑：8 (top) + w (icon) + 2 (gap1) + 20 (rating) + 2 (gap2) + 20 (name) + 8 (bottom) = w + 60
    return QSize(w + 30, w + 60);
}

QWidget* FileItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QLineEdit* editor = new QLineEdit(parent);
    editor->setAlignment(Qt::AlignCenter);
    editor->setFrame(false);
    
    // 2026-03-xx 编辑器背景也要动态匹配标签色，若无标签则用默认深色
    QString tagColorStr = index.data(ColorRole).toString();
    QString bgColor = tagColorStr.isEmpty() ? "#3E3E42" : tagColorStr;
    QString textColor = tagColorStr.isEmpty() ? "#FFFFFF" : "#000000";

    // 2026-03-xx 行内编辑器（F2 重命名框）：圆角由 6px 修改成 4px
    editor->setStyleSheet(
        QString("QLineEdit { background-color: %1; color: %2; border-radius: 4px; "
                "border: 2px solid #3498db; font-weight: bold; font-size: 9pt; padding: 0px; }")
        .arg(bgColor, textColor)
    );
    return editor;
}

void FileItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    QString value = index.model()->data(index, Qt::EditRole).toString();
    QLineEdit* lineEdit = static_cast<QLineEdit*>(editor);
    lineEdit->setText(value);
    
    // 2026-03-xx 智能选中：选中不含扩展名的文件名
    int lastDot = value.lastIndexOf('.');
    if (lastDot > 0) {
        lineEdit->setSelection(0, lastDot);
    } else {
        lineEdit->selectAll();
    }
}

void FileItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    QLineEdit* lineEdit = static_cast<QLineEdit*>(editor);
    QString value = lineEdit->text();
    model->setData(index, value, Qt::EditRole);
    
    // 同步到物理文件系统重命名
    QString oldPath = index.data(PathRole).toString();
    QFileInfo info(oldPath);
    QString newPath = info.absolutePath() + "/" + value;
    
    if (oldPath != newPath) {
        if (QFile::rename(oldPath, newPath)) {
            // 同步元数据到新路径
            AmMetaJson::renameItem(info.absolutePath(), info.fileName(), value);
            model->setData(index, newPath, PathRole);
        }
    }
}

void FileItemDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    // 强制编辑器位置与 NameBox 重合 (整体上移 7 像素方案)
    QRect rect = option.rect.adjusted(4, 4, -4, -4);
    int iconDisplaySize = option.decorationSize.width();
    if (iconDisplaySize <= 0) iconDisplaySize = 64;
    
    int iconBottom = rect.top() + 8 + iconDisplaySize;
    int metaBottom = iconBottom - 5 + 20; 
    QRect nameBoxRect(rect.left() + 6, metaBottom + 2, rect.width() - 12, 20);
    
    editor->setGeometry(nameBoxRect);
}

bool FileItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            // 计算“禁止/无评分”图标的区域
            QRect rect = option.rect.adjusted(4, 4, -4, -4);
            int iconDisplaySize = option.decorationSize.width();
            if (iconDisplaySize <= 0) iconDisplaySize = 64;
            
            int iconBottom = rect.top() + 8 + iconDisplaySize;
            int metaY = iconBottom - 5;
            QRect metaRect(rect.left() + 10, metaY, rect.width() - 20, 20);

            bool isLocked = index.data(IsLockedRole).toBool();
            int totalWidth = 18 + (isLocked ? 20 : 0) + (5 * 18);
            int startX = metaRect.left() + (metaRect.width() - totalWidth) / 2;

            QRect banRect(startX, metaRect.top() + 3, 18, 18);

            if (banRect.contains(mouseEvent->pos())) {
                QString path = index.data(PathRole).toString();
                if (AmMetaJson::setItemMeta(path, "rating", 0)) {
                    model->setData(index, 0, RatingRole);
                    return true;
                }
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

