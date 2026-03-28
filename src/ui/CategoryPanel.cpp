#include "CategoryPanel.h"
#include "../../SvgIcons.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QSvgRenderer>
#include <QPainter>
#include <QHeaderView>
#include <QScrollBar>

namespace ArcMeta {

/**
 * @brief 构造函数，设置面板属性
 */
CategoryPanel::CategoryPanel(QWidget* parent)
    : QWidget(parent) {
    setFixedWidth(230);
    setStyleSheet("QWidget { background-color: #1E1E1E; color: #EEEEEE; border: none; }");
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    initUi();
    setupContextMenu(); // 初始化 Action 指针
}

/**
 * @brief 初始化整体 UI 结构
 */
void CategoryPanel::initUi() {
    // 面板标题
    QLabel* titleLabel = new QLabel("灵感归档", this);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #888; padding: 10px 12px; background: #252525;");
    m_mainLayout->addWidget(titleLabel);

    initTopStats();
    
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    line->setStyleSheet("background-color: #333333; border: none;");
    m_mainLayout->addWidget(line);

    initCategoryTree();
    initBottomToolbar();
}

/**
 * @brief 初始化顶部统计区
 */
void CategoryPanel::initTopStats() {
    m_statsWidget = new QWidget(this);
    m_statsLayout = new QVBoxLayout(m_statsWidget);
    m_statsLayout->setContentsMargins(12, 12, 12, 12);
    m_statsLayout->setSpacing(10);
    
    addStatItem("all_data", "全部数据", 0);
    addStatItem("today", "今日数据", 0);
    addStatItem("today", "昨日数据", 0);
    addStatItem("clock_history", "最近访问", 0);
    addStatItem("uncategorized", "未分类", 0);
    addStatItem("untagged", "未标签", 0);
    addStatItem("star", "收藏", 0);
    addStatItem("trash", "回收站", 0);

    m_mainLayout->addWidget(m_statsWidget);
}

/**
 * @brief 添加统计项条目
 */
void CategoryPanel::addStatItem(const QString& iconKey, const QString& name, int count) {
    QWidget* item = new QWidget(m_statsWidget);
    QHBoxLayout* layout = new QHBoxLayout(item);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    
    QLabel* iconLabel = new QLabel(item);
    iconLabel->setFixedSize(18, 18);
    iconLabel->setPixmap(getSvgIcon(iconKey, QColor("#B0B0B0")).pixmap(18, 18));
    
    QLabel* nameLabel = new QLabel(name, item);
    nameLabel->setStyleSheet("font-size: 11px; color: #B0B0B0;");
    
    QLabel* countLabel = new QLabel(QString::number(count), item);
    countLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    countLabel->setStyleSheet("font-size: 11px; color: #5F5E5A;");
    
    layout->addWidget(iconLabel);
    layout->addWidget(nameLabel);
    layout->addStretch();
    layout->addWidget(countLabel);
    
    m_statsLayout->addWidget(item);
}

/**
 * @brief 初始化中间分类树区
 */
void CategoryPanel::initCategoryTree() {
    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(true);
    m_treeView->setIndentation(16);
    m_treeView->setAnimated(true);
    m_treeView->setDragEnabled(true);
    m_treeView->setAcceptDrops(true);
    m_treeView->setDropIndicatorShown(true);
    m_treeView->setDragDropMode(QAbstractItemView::InternalMove);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    
    m_treeView->verticalScrollBar()->setStyleSheet(
        "QScrollBar:vertical { background: transparent; width: 4px; }"
        "QScrollBar::handle:vertical { background: #444444; border-radius: 2px; }"
    );

    m_treeView->setStyleSheet(
        "QTreeView { background-color: transparent; border: none; font-size: 12px; selection-background-color: #378ADD; }"
        "QTreeView::item { height: 32px; padding-left: 4px; color: #EEEEEE; }"
        "QTreeView::item:hover { background-color: rgba(255, 255, 255, 0.05); }"
    );

    m_treeModel = new QStandardItemModel(this);
    m_treeView->setModel(m_treeModel);

    connect(m_treeView, &QTreeView::customContextMenuRequested, 
            this, &CategoryPanel::onCustomContextMenuRequested);

    m_mainLayout->addWidget(m_treeView, 1);
}

/**
 * @brief 初始化底部工具栏
 */
void CategoryPanel::initBottomToolbar() {
    m_bottomToolbar = new QWidget(this);
    m_bottomToolbar->setFixedHeight(36);
    m_bottomToolbar->setStyleSheet("background-color: #252525; border-top: 1px solid #333333;");
    
    QHBoxLayout* layout = new QHBoxLayout(m_bottomToolbar);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(8);

    m_addCategoryBtn = new QPushButton(this);
    m_addCategoryBtn->setFixedSize(24, 24);
    m_addCategoryBtn->setIcon(getSvgIcon("add", QColor("#EEEEEE")));
    m_addCategoryBtn->setToolTip("新建分类");
    m_addCategoryBtn->setCursor(Qt::PointingHandCursor);
    m_addCategoryBtn->setStyleSheet(
        "QPushButton { background: transparent; border-radius: 4px; }"
        "QPushButton:hover { background: rgba(255, 255, 255, 0.1); }"
        "QPushButton:pressed { background: rgba(255, 255, 255, 0.2); }"
    );

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("搜索分类...");
    m_searchEdit->setFixedHeight(24);
    m_searchEdit->setStyleSheet(
        "QLineEdit { background: #1E1E1E; border: 1px solid #333333; border-radius: 4px; padding-left: 6px; font-size: 11px; color: #EEEEEE; }"
        "QLineEdit:focus { border: 1px solid #378ADD; }"
    );

    layout->addWidget(m_addCategoryBtn);
    layout->addWidget(m_searchEdit);
    
    m_mainLayout->addWidget(m_bottomToolbar);
}

/**
 * @brief 实现声明的 setupContextMenu 方法，初始化所有 Action 指针
 */
void CategoryPanel::setupContextMenu() {
    actNewData = new QAction(getSvgIcon("file", QColor("#EEEEEE")), "新建数据", this);
    actAssignToCategory = new QAction("归类到此分类", this);
    actImportData = new QAction(getSvgIcon("file_import", QColor("#EEEEEE")), "导入数据", this);
    actExport = new QAction(getSvgIcon("file_export", QColor("#EEEEEE")), "导出", this);
    actSetColor = new QAction(getSvgIcon("palette", QColor("#EEEEEE")), "设置颜色", this);
    actRandomColor = new QAction(getSvgIcon("random_color", QColor("#EEEEEE")), "随机颜色", this);
    actSetPresetTags = new QAction(getSvgIcon("tag", QColor("#EEEEEE")), "设置预设标签", this);
    actNewSibling = new QAction("新建分类（同级）", this);
    actNewChild = new QAction("新建子分类", this);
    actTogglePin = new QAction(getSvgIcon("pin", QColor("#EEEEEE")), "置顶 / 取消置顶", this);
    actRename = new QAction(getSvgIcon("edit", QColor("#EEEEEE")), "重命名", this);
    actDelete = new QAction(getSvgIcon("trash", QColor("#EEEEEE")), "删除", this);
    actSortByName = new QAction("按名称", this);
    actSortByCount = new QAction("按数量", this);
    actSortByTime = new QAction("按创建时间", this);
    actPasswordProtect = new QAction(getSvgIcon("lock", QColor("#EEEEEE")), "密码保护", this);
}

/**
 * @brief 渲染右键菜单
 */
void CategoryPanel::onCustomContextMenuRequested(const QPoint& pos) {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #2B2B2B; border: 1px solid #444444; color: #EEEEEE; padding: 4px; }"
        "QMenu::item { height: 28px; padding: 0 24px 0 32px; border-radius: 4px; font-size: 12px; }"
        "QMenu::item:selected { background-color: #378ADD; }"
        "QMenu::separator { height: 1px; background: #444444; margin: 4px 8px 4px 8px; }"
    );

    menu.addAction(actNewData);
    menu.addAction(actAssignToCategory);
    menu.addSeparator();
    menu.addAction(actImportData);
    menu.addAction(actExport);
    menu.addSeparator();
    menu.addAction(actSetColor);
    menu.addAction(actRandomColor);
    menu.addAction(actSetPresetTags);
    menu.addSeparator();
    menu.addAction(actNewSibling);
    menu.addAction(actNewChild);
    menu.addSeparator();
    menu.addAction(actTogglePin);
    menu.addAction(actRename);
    menu.addAction(actDelete);
    menu.addSeparator();

    QMenu* sortMenu = menu.addMenu("排列");
    sortMenu->addAction(actSortByName);
    sortMenu->addAction(actSortByCount);
    sortMenu->addAction(actSortByTime);

    menu.addAction(actPasswordProtect);

    menu.exec(m_treeView->viewport()->mapToGlobal(pos));
}

/**
 * @brief 从 SVG 注入颜色并渲染图标
 */
QIcon CategoryPanel::getSvgIcon(const QString& key, const QColor& color) {
    if (!SvgIcons::icons.contains(key)) return QIcon();
    
    QString svgDataStr = SvgIcons::icons[key];
    svgDataStr.replace("currentColor", color.name());
    QByteArray svgData = svgDataStr.toUtf8();
    
    QSvgRenderer renderer(svgData);
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    
    return QIcon(pixmap);
}

} // namespace ArcMeta
