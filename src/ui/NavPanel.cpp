#include "NavPanel.h"
#include "UiHelper.h"
#include <QHeaderView>
#include <QScrollBar>
#include <QLabel>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QFileIconProvider>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QStandardPaths>

namespace ArcMeta {

/**
 * @brief 构造函数，设置面板属性
 */
NavPanel::NavPanel(QWidget* parent)
    : QWidget(parent) {
    // 2026-03-xx 按照旧版参数锁定：恢复 230px 最小宽度限制与 1 像素边缘像素轮廓
    setMinimumWidth(230);
    setStyleSheet("QWidget { background-color: #1E1E1E; color: #EEEEEE; border: 1px solid #333333; border-radius: 0px; }");

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    initUi();
}

/**
 * @brief 初始化 UI 组件
 */
void NavPanel::initUi() {
    // 2026-03-xx 按照旧版参数修复：标题栏样式对齐 (高度 32px, 背景 #252526, 底部边框 #333)
    QWidget* header = new QWidget(this);
    header->setFixedHeight(32);
    header->setStyleSheet("background-color: #252526; border: none; border-bottom: 1px solid #333;");
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(15, 0, 15, 0);
    headerLayout->setSpacing(8);

    QLabel* iconLabel = new QLabel(header);
    iconLabel->setPixmap(UiHelper::getIcon("folder", QColor("#3498db"), 18).pixmap(18, 18));
    iconLabel->setStyleSheet("border: none; background: transparent;");

    QLabel* titleLabel = new QLabel("本地目录", header);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #3498db; background: transparent; border: none;");

    headerLayout->addWidget(iconLabel);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    m_mainLayout->addWidget(header);

    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(true);
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(16);
    m_treeView->setExpandsOnDoubleClick(true);

    m_model = new QStandardItemModel(this);
    QFileIconProvider iconProvider;

    // 1. 新增：桌面入口 (使用系统原生图标)
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QStandardItem* desktopItem = new QStandardItem(iconProvider.icon(QFileInfo(desktopPath)), "桌面");
    desktopItem->setData(desktopPath, Qt::UserRole + 1);
    // 增加虚拟子项以便显示展开箭头
    desktopItem->appendRow(new QStandardItem("Loading..."));
    m_model->appendRow(desktopItem);

    // 2. 新增：此电脑入口 (使用系统原生图标)
    // 对于此电脑这种虚拟路径，尝试用 Computer 专用图标，若失败则回退到系统驱动器图标
    QIcon computerIcon = iconProvider.icon(QFileIconProvider::Computer);
    QStandardItem* computerItem = new QStandardItem(computerIcon, "此电脑");
    computerItem->setData("computer://", Qt::UserRole + 1);
    m_model->appendRow(computerItem);

    // 3. 磁盘列表
    const auto drives = QDir::drives();
    for (const QFileInfo& drive : drives) {
        QString driveName = drive.absolutePath();
        QStandardItem* driveItem = new QStandardItem(iconProvider.icon(drive), driveName);
        driveItem->setData(driveName, Qt::UserRole + 1);
        driveItem->appendRow(new QStandardItem("Loading..."));
        m_model->appendRow(driveItem);
    }

    m_treeView->setModel(m_model);
    connect(m_treeView, &QTreeView::expanded, this, &NavPanel::onItemExpanded);

    // 树形控件样式美化 (禁止使用或显示三角形)
    m_treeView->setStyleSheet(
        "QTreeView { background-color: transparent; border: none; font-size: 12px; selection-background-color: #378ADD; outline: none; }"
        "QTreeView::item { height: 28px; padding-left: 4px; color: #EEEEEE; }"
        "QTreeView::item:hover { background-color: rgba(255, 255, 255, 0.05); }"
        "QTreeView::branch:has-children:!has-siblings:closed,"
        "QTreeView::branch:closed:has-children:has-siblings,"
        "QTreeView::branch:has-children:!has-siblings:open,"
        "QTreeView::branch:open:has-children:has-siblings { border-image: none; image: none; }"
    );

    // 滚动条样式
    m_treeView->verticalScrollBar()->setStyleSheet(
        "QScrollBar:vertical { background: transparent; width: 4px; }"
        "QScrollBar::handle:vertical { background: #444444; border-radius: 4px; }"
    );

    connect(m_treeView, &QTreeView::clicked, this, &NavPanel::onTreeClicked);

    m_mainLayout->addWidget(m_treeView);
}

/**
 * @brief 设置当前显示的根路径并自动展开
 */
void NavPanel::setRootPath(const QString& path) {
    Q_UNUSED(path);
    // 由于改为扁平化快捷入口列表，不再支持 setRootPath 的树深度同步
}

void NavPanel::selectPath(const QString& path) {
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QStandardItem* item = m_model->item(i);
        if (item->data(Qt::UserRole + 1).toString() == path) {
            m_treeView->setCurrentIndex(item->index());
            m_treeView->setFocus();
            break;
        }
    }
}

/**
 * @brief 当用户点击目录时，发出信号告知外部组件（如内容面板）
 */
void NavPanel::onTreeClicked(const QModelIndex& index) {
    QString path = index.data(Qt::UserRole + 1).toString();
    if (!path.isEmpty() && path != "computer://") {
        emit directorySelected(path);
    } else if (path == "computer://") {
        emit directorySelected("computer://");
    }
}

void NavPanel::onItemExpanded(const QModelIndex& index) {
    QStandardItem* item = m_model->itemFromIndex(index);
    if (!item) return;

    // 如果只有一个 Loading 子项，则触发真实加载
    if (item->rowCount() == 1 && item->child(0)->text() == "Loading...") {
        fetchChildDirs(item);
    }
}

void NavPanel::fetchChildDirs(QStandardItem* parent) {
    QString path = parent->data(Qt::UserRole + 1).toString();
    if (path.isEmpty() || path == "computer://") return;

    parent->removeRows(0, parent->rowCount());

    QDir dir(path);
    QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QFileIconProvider iconProvider;

    for (const QFileInfo& info : list) {
        QStandardItem* child = new QStandardItem(iconProvider.icon(info), info.fileName());
        child->setData(info.absoluteFilePath(), Qt::UserRole + 1);
        
        // 探测是否有子目录，有则加占位符
        QDir subDir(info.absoluteFilePath());
        if (!subDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) {
            child->appendRow(new QStandardItem("Loading..."));
        }
        parent->appendRow(child);
    }
}

} // namespace ArcMeta
