#include "NavPanel.h"
#include <QHeaderView>
#include <QScrollBar>
#include <QLabel>

namespace ArcMeta {

/**
 * @brief 构造函数，设置面板属性
 */
NavPanel::NavPanel(QWidget* parent)
    : QWidget(parent) {
    // 设置面板宽度（遵循文档：导航面板 200px）
    setFixedWidth(200);
    setStyleSheet("QWidget { background-color: #1E1E1E; color: #EEEEEE; border: none; }");

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    initUi();
}

/**
 * @brief 初始化 UI 组件
 */
void NavPanel::initUi() {
    // 面板标题
    QLabel* titleLabel = new QLabel("本地目录", this);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #4a90e2; padding: 10px 12px; background: #252526;");
    m_mainLayout->addWidget(titleLabel);

    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(true); // 隐藏表头
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(16);
    m_treeView->setSortingEnabled(true);
    m_treeView->setExpandsOnDoubleClick(true);

    // 设置文件系统模型
    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot); // 只显示文件夹，不显示文件
    m_fileModel->setRootPath(""); // 监视全盘

    m_treeView->setModel(m_fileModel);

    // 隐藏除“名称”列以外的所有列
    for (int i = 1; i < m_fileModel->columnCount(); ++i) {
        m_treeView->hideColumn(i);
    }

    // 树形控件样式美化
    m_treeView->setStyleSheet(
        "QTreeView { background-color: transparent; border: none; font-size: 12px; selection-background-color: #378ADD; }"
        "QTreeView::item { height: 28px; padding-left: 4px; color: #EEEEEE; }"
        "QTreeView::item:hover { background-color: rgba(255, 255, 255, 0.05); }"
        "QTreeView::branch:has-children:!has-siblings:closed,"
        "QTreeView::branch:closed:has-children:has-siblings { border-image: none; image: none; }"
        "QTreeView::branch:has-children:!has-siblings:open,"
        "QTreeView::branch:open:has-children:has-siblings { border-image: none; image: none; }"
    );

    // 滚动条样式
    m_treeView->verticalScrollBar()->setStyleSheet(
        "QScrollBar:vertical { background: transparent; width: 4px; }"
        "QScrollBar::handle:vertical { background: #444444; border-radius: 2px; }"
    );

    connect(m_treeView, &QTreeView::clicked, this, &NavPanel::onTreeClicked);

    m_mainLayout->addWidget(m_treeView);
}

/**
 * @brief 设置当前显示的根路径并自动展开
 */
void NavPanel::setRootPath(const QString& path) {
    QModelIndex index = m_fileModel->index(path);
    if (index.isValid()) {
        m_treeView->expand(index);
        m_treeView->scrollTo(index);
        m_treeView->setCurrentIndex(index);
    }
}

/**
 * @brief 当用户点击目录时，发出信号告知外部组件（如内容面板）
 */
void NavPanel::onTreeClicked(const QModelIndex& index) {
    QString path = m_fileModel->filePath(index);
    emit directorySelected(path);
}

} // namespace ArcMeta
