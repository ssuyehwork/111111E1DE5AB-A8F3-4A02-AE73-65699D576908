#include "DropListView.h"
#include <QDrag>
#include <QPixmap>
#include <QMimeData>

namespace ArcMeta {

DropListView::DropListView(QWidget* parent) : QListView(parent) {}

void DropListView::startDrag(Qt::DropActions supportedActions) {
    QModelIndexList indexes = selectedIndexes();
    if (indexes.isEmpty()) return;

    QDrag* drag = new QDrag(this);
    drag->setMimeData(model()->mimeData(indexes));
    
    // 物理还原：消除卡片快照干扰，使用 1x1 透明像素
    QPixmap pix(1, 1);
    pix.fill(Qt::transparent);
    drag->setPixmap(pix);
    drag->setHotSpot(QPoint(0, 0));
    
    drag->exec(supportedActions, Qt::MoveAction);
}

} // namespace ArcMeta
