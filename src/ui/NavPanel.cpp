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
    : QFrame(parent) {
    setObjectName("ListContainer");
    setAttribute(Qt::WA_StyledBackground, true);
    // 设置面板宽度（遵循文档：导航面板 230px）
    setMinimumWidth(230);
    
    setStyleSheet(
        "#ListContainer {"
        "  background-color: #1e1e1e;"
        "  border: 1px solid #333333;"
        "  border-radius: 0px;"
        "}"
        "QWidget { color: #EEEEEE; }"
    );

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // 1. 物理聚焦线 (复原：1px 绿线，默认隐藏)
    m_focusLine = new QWidget(this);
    m_focusLine->setFixedHeight(1);
    m_focusLine->setStyleSheet("background-color: #2ecc71;");
    m_focusLine->hide();
    m_mainLayout->addWidget(m_focusLine);

    initUi();
}

/**
 * @brief 初始化 UI 组件
 */
void NavPanel::initUi() {
    // 面板标题 (复原：列表标题栏全宽下划线方案)
    QWidget* header = new QWidget(this);
    header->setObjectName("ContainerHeader");
    header->setFixedHeight(32);
    header->setStyleSheet(
        "background-color: #252526; "
        "border-top-left-radius: 0px; "
        "border-top-right-radius: 0px; "
        "border-bottom: 1px solid #333;"
    );
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    // [CRITICAL] 视觉对齐锁定：此处顶部边距必须设为 2px，以配合 32px 的标题栏高度，使文字达到垂直居中。
    headerLayout->setContentsMargins(15, 2, 15, 0);
    headerLayout->setSpacing(8);

    QLabel* iconLabel = new QLabel(header);
    iconLabel->setPixmap(UiHelper::getIcon("list_ul", QColor("#2ecc71"), 18).pixmap(18, 18));
    headerLayout->addWidget(iconLabel);

    QLabel* titleLabel = new QLabel("笔记列表", header);
    titleLabel->setStyleSheet("color: #2ecc71; font-size: 13px; font-weight: bold; background: transparent; border: none;");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_mainLayout->addWidget(header);

    // 核心修正：为列表内容包裹容器，恢复旧版 (15, 8, 15, 8) 的呼吸边距
    QWidget* contentWrapper = new QWidget(this);
    contentWrapper->setStyleSheet("background: transparent; border: none;");
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWrapper);
    contentLayout->setContentsMargins(15, 8, 15, 8);
    contentLayout->setSpacing(0);

    m_treeView = new QTreeView(this);
    m_treeView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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


    connect(m_treeView, &QTreeView::clicked, this, &NavPanel::onTreeClicked);

    contentLayout->addWidget(m_treeView);
    m_mainLayout->addWidget(contentWrapper, 1);
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

void NavPanel::setFocusLineVisible(bool visible) {
    if (m_focusLine) m_focusLine->setVisible(visible);
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
