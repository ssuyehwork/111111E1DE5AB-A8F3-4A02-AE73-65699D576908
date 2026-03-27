#include "NavPanel.h"
#include <QHeaderView>

namespace ArcMeta {

NavPanel::NavPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_treeView = new QTreeView(this);
    m_model = new QFileSystemModel(this);

    // 配置模型：只显示目录
    m_model->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    m_model->setRootPath(""); // 监控全盘

    m_treeView->setModel(m_model);

    // 隐藏不必要的列（只保留名称列）
    for (int i = 1; i < m_model->columnCount(); ++i) {
        m_treeView->hideColumn(i);
    }
    m_treeView->header()->hide(); // 隐藏表头
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(15);

    connect(m_treeView, &QTreeView::clicked, this, &NavPanel::onItemClicked);

    layout->addWidget(m_treeView);
}

void NavPanel::onItemClicked(const QModelIndex& index) {
    QString path = m_model->filePath(index);
    emit directorySelected(path);
}

} // namespace ArcMeta
