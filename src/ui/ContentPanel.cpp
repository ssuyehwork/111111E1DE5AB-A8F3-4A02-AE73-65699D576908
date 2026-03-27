#include "ContentPanel.h"
#include "IconHelper.h"
#include <QVBoxLayout>
#include <QStandardItem>
#include <QDir>
#include <QEvent>
#include <QHelpEvent>
// 2026-03-27 按照宪法：必须使用项目内建的 ToolTipOverlay 替换系统原生 Tip
// #include "ToolTipOverlay.h"

namespace ArcMeta {

ContentPanel::ContentPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_listView = new QListView(this);
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setIconSize(QSize(64, 64));
    m_listView->setGridSize(QSize(110, 100));
    m_listView->setSpacing(10);

    // 2026-03-27 按照宪法：物理级拦截原生 ToolTip 事件
    m_listView->viewport()->installEventFilter(this);

    m_model = new QStandardItemModel(this);
    m_listView->setModel(m_model);

    layout->addWidget(m_listView);
}

ContentPanel::~ContentPanel() {}

void ContentPanel::updateItems(const std::vector<const FileEntry*>& entries, const QString& currentPath) {
    m_model->clear();

    for (const auto* entry : entries) {
        QString name = QString::fromStdWString(entry->name);
        auto* item = new QStandardItem(name);

        if (entry->isDir()) {
            QString fullPath = QDir(currentPath).filePath(name);
            item->setIcon(IconHelper::getFolderIcon(fullPath, entry->isEmptyDir()));

            // 2026-03-27 细节：在 UserRole 中存储“空状态”，供自定义 ToolTip 逻辑查询
            item->setData(entry->isEmptyDir(), Qt::UserRole + 1);
        } else {
            QFileIconProvider provider;
            item->setIcon(provider.icon(QFileInfo(QDir(currentPath).filePath(name))));
        }

        m_model->appendRow(item);
    }
}

bool ContentPanel::eventFilter(QObject* watched, QEvent* event) {
    // 2026-03-27 按照宪法第五定律：严禁使用原生 Tip。此处拦截并转发给 ToolTipOverlay。
    if (watched == m_listView->viewport() && event->type() == QEvent::ToolTip) {
        auto* helpEvent = static_cast<QHelpEvent*>(event);
        QModelIndex index = m_listView->indexAt(helpEvent->pos());

        if (index.isValid()) {
            bool isEmpty = index.data(Qt::UserRole + 1).toBool();
            if (isEmpty) {
                // TODO: ToolTipOverlay::instance()->showText(helpEvent->globalPos(), "此文件夹为空（无可见子项）");
            }
        }
        return true; // 拦截，阻止原生提示框弹出
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace ArcMeta
