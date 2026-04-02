#include "DropListView.h"
#include <QDrag>
#include <QPixmap>
#include <QMimeData>

namespace ArcMeta {

DropListView::DropListView(QWidget* parent) : QListView(parent) {}

void DropListView::startDrag(Qt::DropActions supportedActions) {
    QModelIndexList indexes = selectedIndexes();
    if (indexes.isEmpty()) return;

    // 1. 获取基础 MimeData
    QMimeData* mimeData = model()->mimeData(indexes);
    if (!mimeData) mimeData = new QMimeData();

    // 2. 核心补全：注入物理路径 (QUrl)
    QList<QUrl> urls;
    for (const QModelIndex& idx : indexes) {
        // 依次尝试 PathRole (UserRole+5) 和 UserRole+1 (兼容性提取)
        QString path = idx.data(Qt::UserRole + 5).toString();
        if (path.isEmpty()) {
            path = idx.data(Qt::UserRole + 1).toString();
        }

        if (!path.isEmpty()) {
            urls << QUrl::fromLocalFile(path);
        }
    }

    if (!urls.isEmpty()) {
        mimeData->setUrls(urls);
    }

    // 3. 执行拖拽
    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    
    // 物理还原：消除卡片快照干扰，使用 1x1 透明像素作为拖拽图标
    QPixmap pix(1, 1);
    pix.fill(Qt::transparent);
    drag->setPixmap(pix);
    drag->setHotSpot(QPoint(0, 0));
    
    drag->exec(supportedActions, Qt::MoveAction);
}

} // namespace ArcMeta
