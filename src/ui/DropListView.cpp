#include "DropListView.h"
#include <QDrag>
#include <QPixmap>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QDebug>

namespace ArcMeta {

DropListView::DropListView(QWidget* parent) : QListView(parent) {}

void DropListView::startDrag(Qt::DropActions supportedActions) {
    QModelIndexList indexes = selectedIndexes();
    if (indexes.isEmpty()) return;

    qDebug() << "[DropListView] startDrag | Selected Count:" << indexes.count();

    // 核心增强：拦截并注入物理路径 QUrl，确保 CategoryPanel 接收校验通过
    QMimeData* mimeData = model()->mimeData(indexes);
    QList<QUrl> urls;
    for (const QModelIndex& idx : indexes) {
        // ContentPanel 网格视图主要使用 PathRole (UserRole+5)
        QString path = idx.data(Qt::UserRole + 5).toString();
        qDebug() << "[DropListView] Extracting Path (Role+5) for" << idx.data().toString() << ":" << path;

        if (!path.isEmpty() && QFileInfo::exists(path)) {
            urls << QUrl::fromLocalFile(path);
        }
    }

    qDebug() << "[DropListView] Final URLs injected:" << urls;
    if (!urls.isEmpty()) {
        mimeData->setUrls(urls);
    }

    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    
    // 物理还原：消除卡片快照干扰，使用 1x1 透明像素
    QPixmap pix(1, 1);
    pix.fill(Qt::transparent);
    drag->setPixmap(pix);
    drag->setHotSpot(QPoint(0, 0));
    
    drag->exec(supportedActions, Qt::MoveAction);
}

} // namespace ArcMeta
