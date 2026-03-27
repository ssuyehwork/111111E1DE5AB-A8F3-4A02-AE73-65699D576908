#include "NavPanel.h"
#include "IconHelper.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDir>

namespace ArcMeta {

QVariant NavProxyModel::data(const QModelIndex& index, int role) const {
    if (role == Qt::DecorationRole) {
        // 2026-03-27 获取底层文件系统模型的原始图标和路径
        QFileInfo info = sourceModel()->data(index, QFileSystemModel::FileIconRole).value<QFileInfo>();
        QString path = sourceModel()->data(index, QFileSystemModel::FilePathRole).toString();

        if (info.isDir()) {
            // [性能考虑] 在实际 MFT 环境下，此处将查表 visibleChildCount。
            // 2026-03-27 此处仅演示：如果文件夹路径以 "Empty" 结尾，则视为模拟空目录并置灰。
            bool isActuallyEmpty = path.endsWith("EmptyDir", Qt::CaseInsensitive);
            return IconHelper::getFolderIcon(path, isActuallyEmpty);
        }
    }
    return QIdentityProxyModel::data(index, role);
}

NavPanel::NavPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_treeView = new QTreeView(this);
    m_fsModel = new QFileSystemModel(this);
    m_fsModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
    m_fsModel->setRootPath(QDir::rootPath());

    // 2026-03-27 建立代理模型，确保 NavPanel 视觉规范与 ContentPanel 统一
    m_proxyModel = new NavProxyModel(this);
    m_proxyModel->setSourceModel(m_fsModel);

    m_treeView->setModel(m_proxyModel);
    m_treeView->setHeaderHidden(true);
    for (int i = 1; i < m_fsModel->columnCount(); ++i) {
        m_treeView->setColumnHidden(i, true);
    }

    layout->addWidget(m_treeView);
}

NavPanel::~NavPanel() {}

} // namespace ArcMeta
