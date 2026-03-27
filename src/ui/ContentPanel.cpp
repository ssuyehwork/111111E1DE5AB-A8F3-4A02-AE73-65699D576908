#include "ContentPanel.h"
#include <QHeaderView>

namespace ArcMeta {

ContentPanel::ContentPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    initListView();
    layout->addWidget(m_treeView);
}

void ContentPanel::initListView() {
    m_treeView = new QTreeView(this);
    m_model = new QFileSystemModel(this);

    // 配置模型：显示所有文件（除了隐藏的元数据）
    m_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    m_model->setRootPath(QDir::rootPath());

    m_treeView->setModel(m_model);

    // 设置列表视图规范
    // 名称列弹性，其他列固定宽度
    m_treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeView->setColumnWidth(1, 80);  // 类型
    m_treeView->setColumnWidth(2, 90);  // 大小
    m_treeView->setColumnWidth(3, 150); // 修改时间

    // 隐藏内置的最后几列（权限等），预留给元数据
    m_treeView->setSortingEnabled(true);
}

void ContentPanel::setRootPath(const QString& path) {
    QModelIndex index = m_model->index(path);
    if (index.isValid()) {
        m_treeView->setRootIndex(index);
    }
}

} // namespace ArcMeta
