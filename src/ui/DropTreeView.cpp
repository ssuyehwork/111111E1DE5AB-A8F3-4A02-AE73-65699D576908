#include "DropTreeView.h"
#include "CategoryModel.h"
#include <QDrag>
#include <QPixmap>
#include <QAbstractProxyModel>
#include <QMimeData>
#include <QUrl>
#include <QDir>
#include <QStringList>
#include <QFileInfo>
#include <QDebug>

namespace ArcMeta {

DropTreeView::DropTreeView(QWidget* parent) : QTreeView(parent) {
    setAcceptDrops(true);
    setDropIndicatorShown(true);
}

void DropTreeView::dragEnterEvent(QDragEnterEvent* event) {
    qDebug() << "[DropTreeView] dragEnterEvent | Formats:" << event->mimeData()->formats();
    if (event->mimeData()->hasFormat("application/x-note-ids") || 
        event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist") ||
        event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        qDebug() << "[DropTreeView] Accepted DragEnter";
    } else {
        event->ignore();
    }
}

void DropTreeView::dragMoveEvent(QDragMoveEvent* event) {
    QTreeView::dragMoveEvent(event);

    if (event->mimeData()->hasFormat("application/x-note-ids") || 
        event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist") ||
        event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DropTreeView::dropEvent(QDropEvent* event) {
    QModelIndex index = indexAt(event->position().toPoint());
    qDebug() << "[DropTreeView] dropEvent | Target Index Valid:" << index.isValid()
             << "| Name:" << index.data().toString();

    // 优先处理路径拖入 (收藏逻辑)
    if (event->mimeData()->hasUrls()) {
        QStringList paths;
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                paths << QDir::toNativeSeparators(url.toLocalFile());
            }
        }
        qDebug() << "[DropTreeView] Dropped Paths:" << paths;
        if (!paths.isEmpty()) {
            emit pathsDropped(paths, index);
            event->setDropAction(Qt::LinkAction); // 视觉上显示为“链接/快捷方式”
            event->accept();
            return;
        }
    }

    // 处理内部 ID 拖拽 (如果以后有用)
    if (event->mimeData()->hasFormat("application/x-note-ids")) {
        QByteArray byteData = event->mimeData()->data("application/x-note-ids");
        QStringList idStrs = QString::fromUtf8(byteData).split(",", Qt::SkipEmptyParts);
        QList<int> ids;
        for (const QString& s : idStrs) ids << s.toInt();

        emit notesDropped(ids, index);
        event->acceptProposedAction();
    } else if (event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist")) {
        QTreeView::dropEvent(event);
    } else {
        event->ignore();
    }
}

void DropTreeView::startDrag(Qt::DropActions supportedActions) {
    QModelIndexList indexes = selectedIndexes();
    if (indexes.isEmpty()) return;

    qDebug() << "[DropTreeView] startDrag | Selected Count:" << indexes.count();

    // 核心增强：拦截并注入物理路径 QUrl，确保 CategoryPanel 接收校验通过
    QMimeData* mimeData = model()->mimeData(indexes);
    QList<QUrl> urls;
    for (const QModelIndex& idx : indexes) {
        if (idx.column() != 0) continue;

        // 兼容性提取：NavPanel 使用 UserRole+1，ContentPanel 使用 PathRole (UserRole+5)
        QString path = idx.data(Qt::UserRole + 1).toString(); // 尝试 NavPanel 角色
        qDebug() << "[DropTreeView] Trying Role+1 for" << idx.data().toString() << ":" << path;

        if (path.isEmpty() || !QFileInfo::exists(path)) {
            path = idx.data(Qt::UserRole + 5).toString(); // 尝试 ContentPanel/PathRole 角色
            qDebug() << "[DropTreeView] Trying Role+5 for" << idx.data().toString() << ":" << path;
        }

        if (!path.isEmpty() && QFileInfo::exists(path)) {
            urls << QUrl::fromLocalFile(path);
        }
    }

    qDebug() << "[DropTreeView] Final URLs injected:" << urls;
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
    
    qDebug() << "[DropTreeView] Executing drag...";
    drag->exec(supportedActions, Qt::MoveAction);
}

void DropTreeView::keyboardSearch(const QString& search) {
    Q_UNUSED(search);
}

} // namespace ArcMeta
