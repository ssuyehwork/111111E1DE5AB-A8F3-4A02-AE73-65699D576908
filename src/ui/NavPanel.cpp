#include "NavPanel.h"
#include "IconHelper.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDir>

namespace ArcMeta {

QVariant NavProxyModel::data(const QModelIndex& index, int role) const {
    if (role == Qt::DecorationRole) {
        QFileInfo info = sourceModel()->data(index, QFileSystemModel::FileIconRole).value<QFileInfo>();
        QString path = sourceModel()->data(index, QFileSystemModel::FilePathRole).toString();

        if (info.isDir()) {
            // 2026-03-27 深度细节排查：此处不再使用 endsWith 模拟。
            // 在主逻辑集成后，此处将通过 MFT 全局索引查询该路径对应的 visibleChildCount。
            // 暂时逻辑：模拟判定，待 MftReader 模块完整加载后对齐。
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

    m_proxyModel = new NavProxyModel(this);
    m_proxyModel->setSourceModel(m_fsModel);

    m_treeView->setModel(m_proxyModel);
    m_treeView->setHeaderHidden(true);

    // 2026-03-27 按照宪法：必须处理 ToolTip 拦截
    m_treeView->viewport()->installEventFilter(this);

    for (int i = 1; i < m_fsModel->columnCount(); ++i) {
        m_treeView->setColumnHidden(i, true);
    }

    layout->addWidget(m_treeView);
}

NavPanel::~NavPanel() {}

bool NavPanel::eventFilter(QObject* watched, QEvent* event) {
    // 2026-03-27 按照宪法第五定律：拦截导航面板的原生 Tip
    if (watched == m_treeView->viewport() && event->type() == QEvent::ToolTip) {
        return true; // 拦截原生，等待 ToolTipOverlay 指令
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace ArcMeta
