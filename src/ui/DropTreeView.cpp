#include "DropTreeView.h"
#include "CategoryModel.h"
#include <QDrag>
#include <QPixmap>
#include <QAbstractProxyModel>

namespace ArcMeta {

DropTreeView::DropTreeView(QWidget* parent) : QTreeView(parent) {
    setAcceptDrops(true);
    setDropIndicatorShown(true);
}

void DropTreeView::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat("application/x-note-ids") ||
        event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DropTreeView::dragMoveEvent(QDragMoveEvent* event) {
    QTreeView::dragMoveEvent(event);

    if (event->mimeData()->hasFormat("application/x-note-ids") ||
        event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DropTreeView::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasFormat("application/x-note-ids")) {
        QByteArray byteData = event->mimeData()->data("application/x-note-ids");
        QStringList idStrs = QString::fromUtf8(byteData).split(",", Qt::SkipEmptyParts);
        QList<int> ids;
        for (const QString& s : idStrs) ids << s.toInt();

        QModelIndex index = indexAt(event->position().toPoint());
        emit notesDropped(ids, index);
        event->acceptProposedAction();
    } else if (event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist")) {
        QTreeView::dropEvent(event);
    } else {
        event->ignore();
    }
}

void DropTreeView::startDrag(Qt::DropActions supportedActions) {
    CategoryModel* catModel = qobject_cast<CategoryModel*>(model());
    if (!catModel) {
        if (auto* proxy = qobject_cast<QAbstractProxyModel*>(model())) {
            catModel = qobject_cast<CategoryModel*>(proxy->sourceModel());
        }
    }

    if (catModel && !selectedIndexes().isEmpty()) {
        // catModel->setDraggingId(selectedIndexes().first().data(CategoryModel::IdRole).toInt());
    }

    QDrag* drag = new QDrag(this);
    drag->setMimeData(model()->mimeData(selectedIndexes()));

    QPixmap pix(1, 1);
    pix.fill(Qt::transparent);
    drag->setPixmap(pix);
    drag->setHotSpot(QPoint(0, 0));

    drag->exec(supportedActions, Qt::MoveAction);
}

void DropTreeView::keyboardSearch(const QString& search) {
    Q_UNUSED(search);
}

} // namespace ArcMeta
