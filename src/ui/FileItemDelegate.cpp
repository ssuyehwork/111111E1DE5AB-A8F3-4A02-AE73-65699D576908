#include "FileItemDelegate.h"
#include <QLineEdit>
#include <QMouseEvent>
#include <QFile>
#include <QDir>
#include <QPainter>
#include <QFileInfo>
#include <QPainterPath>
#include "../meta/AmMetaJson.h"
#include "../core/SuffixColorManager.h"
#include "IconHelper.h"

FileItemDelegate::FileItemDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void FileItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    if (!index.isValid()) return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    bool isSelected = (option.state & QStyle::State_Selected);
    QRect rect = option.rect.adjusted(4, 4, -4, -4);
    QPainterPath borderPath;
    borderPath.addRoundedRect(rect, 8, 8);
    if (isSelected) {
        painter->setPen(QPen(QColor("#3498db"), 2));
        painter->setBrush(QColor(40, 40, 40));
        painter->drawPath(borderPath);
    } else {
        painter->setPen(QPen(QColor("#333333"), 1));
        painter->setBrush(QColor(45, 45, 45));
        painter->drawPath(borderPath);
    }
    QString path = index.data(FileItemDelegate::PathRole).toString();
    QFileInfo info(path);
    QString suffix = info.isDir() ? "DIR" : (info.suffix().isEmpty() ? "FILE" : info.suffix().toUpper());
    QColor badgeColor = SuffixColorManager::instance().getColor(suffix);
    QRect badgeRect(rect.left() + 8, rect.top() + 8, 32, 18);
    painter->setPen(Qt::NoPen); painter->setBrush(badgeColor); painter->drawRoundedRect(badgeRect, 4, 4);
    painter->setPen(badgeColor.lightness() > 180 ? Qt::black : Qt::white);
    QFont badgeFont = painter->font(); badgeFont.setPointSize(8); badgeFont.setBold(true); painter->setFont(badgeFont);
    painter->drawText(badgeRect, Qt::AlignCenter, suffix);
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    int iconSize = option.decorationSize.width() > 0 ? option.decorationSize.width() : 64;
    QRect iconRect(rect.left() + (rect.width() - iconSize) / 2, rect.top() + 8, iconSize, iconSize);
    icon.paint(painter, iconRect, Qt::AlignCenter);
    int rating = index.data(FileItemDelegate::RatingRole).toInt();
    int metaY = iconRect.bottom() - 5;
    QRect metaRect(rect.left() + 10, metaY, rect.width() - 20, 20);
    int startX = metaRect.left() + (metaRect.width() - (18 + 5 * 18)) / 2;
    QPixmap banPix = IconHelper::getIcon("no_color", "#555555", 14).pixmap(14, 14);
    painter->drawPixmap(startX, metaRect.top() + 3, banPix);
    int currentX = startX + 18;
    QFont starFont = painter->font(); starFont.setPointSize(12); painter->setFont(starFont);
    for (int i = 0; i < 5; ++i) {
        painter->setPen(i < rating ? QColor("#F2B705") : QColor("#444444"));
        painter->drawText(currentX + i * 18, metaRect.top() + 15, i < rating ? "★" : "☆");
    }
    QString fileName = index.data(Qt::DisplayRole).toString();
    QString tagColorStr = index.data(FileItemDelegate::ColorRole).toString();
    QRect nameBoxRect(rect.left() + 6, metaRect.bottom() + 2, rect.width() - 12, 20);
    if (!tagColorStr.isEmpty()) {
        painter->setPen(Qt::NoPen); painter->setBrush(QColor(tagColorStr)); painter->drawRoundedRect(nameBoxRect, 4, 4);
        painter->setPen(Qt::black);
    } else {
        painter->setPen(QColor("#CCCCCC"));
    }
    QFont nameFont = painter->font(); nameFont.setPointSize(9); nameFont.setBold(true); painter->setFont(nameFont);
    painter->drawText(nameBoxRect.adjusted(4, 0, -4, 0), Qt::AlignCenter, painter->fontMetrics().elidedText(fileName, Qt::ElideMiddle, nameBoxRect.width() - 8));
    painter->restore();
}

QSize FileItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const {
    int w = option.decorationSize.width() > 0 ? option.decorationSize.width() : 96;
    return QSize(w + 30, w + 60);
}

QWidget* FileItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QLineEdit* editor = new QLineEdit(parent);
    editor->setAlignment(Qt::AlignCenter); editor->setFrame(false);
    QString tagColorStr = index.data(FileItemDelegate::ColorRole).toString();
    QString bgColor = tagColorStr.isEmpty() ? "#3E3E42" : tagColorStr;
    QString textColor = tagColorStr.isEmpty() ? "#FFFFFF" : "#000000";
    editor->setStyleSheet(QString("QLineEdit { background-color: %1; color: %2; border-radius: 4px; border: 2px solid #3498db; font-weight: bold; font-size: 9pt; }").arg(bgColor, textColor));
    return editor;
}

void FileItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    QString value = index.model()->data(index, Qt::EditRole).toString();
    QLineEdit* lineEdit = static_cast<QLineEdit*>(editor); lineEdit->setText(value);
    int lastDot = value.lastIndexOf('.');
    if (lastDot > 0) lineEdit->setSelection(0, lastDot); else lineEdit->selectAll();
}

void FileItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    QLineEdit* lineEdit = static_cast<QLineEdit*>(editor);
    QString value = lineEdit->text();
    QString oldPath = index.data(FileItemDelegate::PathRole).toString();
    QFileInfo info(oldPath);
    QString newPath = info.absolutePath() + "/" + value;
    if (oldPath != newPath && QFile::rename(oldPath, newPath)) {
        AmMetaJson::renameItem(info.absolutePath(), info.fileName(), value);
        model->setData(index, newPath, FileItemDelegate::PathRole);
    }
    model->setData(index, value, Qt::EditRole);
}

void FileItemDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QRect rect = option.rect.adjusted(4, 4, -4, -4);
    int iconSize = option.decorationSize.width() > 0 ? option.decorationSize.width() : 64;
    int metaBottom = rect.top() + 8 + iconSize - 5 + 20;
    editor->setGeometry(QRect(rect.left() + 6, metaBottom + 2, rect.width() - 12, 20));
}

bool FileItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            QRect rect = option.rect.adjusted(4, 4, -4, -4);
            int iconSize = option.decorationSize.width() > 0 ? option.decorationSize.width() : 64;
            int metaY = rect.top() + 8 + iconSize - 5;
            int startX = rect.left() + 10 + (rect.width() - 20 - (18 + 5 * 18)) / 2;
            if (QRect(startX, metaY + 3, 18, 18).contains(mouseEvent->pos())) {
                if (AmMetaJson::setItemMeta(index.data(FileItemDelegate::PathRole).toString(), "rating", 0)) {
                    model->setData(index, 0, FileItemDelegate::RatingRole);
                    return true;
                }
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
