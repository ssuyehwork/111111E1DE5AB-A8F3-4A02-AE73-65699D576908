#include "CategoryPanel.h"
#include "../../SvgIcons.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QSvgRenderer>
#include <QPainter>
#include <QHeaderView>
#include <QScrollBar>
#include <QLabel>
#include <QFrame>
#include <QMenu>
#include <QAction>
#include <QEvent>
#include <QMouseEvent>
#include <QInputDialog>
#include "UiHelper.h"

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
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #4a90e2; padding: 10px 12px; background: #252526;");
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
    m_statsLayout->setContentsMargins(12, 8, 12, 8); // 缩小上下边距
    m_statsLayout->setSpacing(2); // 还原紧凑间距
    
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
    QWidget* item = new QWidget(this);
    item->setFixedHeight(26); // 恢复紧凑型物理约束
    QHBoxLayout* layout = new QHBoxLayout(item);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(8);

    QLabel* iconLabel = new QLabel(this);
    iconLabel->setPixmap(UiHelper::getIcon(iconKey, QColor("#EEEEEE"), 16).pixmap(16, 16));
    layout->addWidget(iconLabel);
    QLabel* nameLabel = new QLabel(name, item);
    nameLabel->setStyleSheet("font-size: 11px; color: #B0B0B0;");
    
    QLabel* countLabel = new QLabel(QString::number(count), item);
    countLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    countLabel->setStyleSheet("font-size: 11px; color: #5F5E5A;");
    
    layout->addStretch();
    layout->addWidget(countLabel);
    
    // 安装事件过滤器以拦截统计项点击
    item->installEventFilter(this);
    item->setProperty("statName", name);
    item->setCursor(Qt::PointingHandCursor);

    m_statsLayout->addWidget(item);
}

bool CategoryPanel::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        if (obj->property("statName").isValid()) {
            emit categorySelected(obj->property("statName").toString());
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
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
        "QTreeView::item { height: 26px; padding-left: 4px; color: #EEEEEE; }"
        "QTreeView::item:hover { background-color: rgba(255, 255, 255, 0.05); }"
    );

    m_treeModel = new QStandardItemModel(this);
    m_treeView->setModel(m_treeModel);

    connect(m_treeView, &QTreeView::customContextMenuRequested, 
            this, &CategoryPanel::onCustomContextMenuRequested);
    connect(m_treeView, &QTreeView::clicked, [this](const QModelIndex& index) {
        emit categorySelected(index.data().toString());
    });

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

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("搜索分类...");
    m_searchEdit->setFixedHeight(24);
    m_searchEdit->setStyleSheet(
        "QLineEdit { background: #1E1E1E; border: 1px solid #333333; border-radius: 6px; padding-left: 6px; font-size: 11px; color: #EEEEEE; }"
        "QLineEdit:focus { border: 1px solid #378ADD; }"
    );
    m_addCategoryBtn = new QPushButton(this);
    m_addCategoryBtn->setFixedSize(28, 28);
    m_addCategoryBtn->setIcon(UiHelper::getIcon("add", QColor("#EEEEEE"), 16));
    m_addCategoryBtn->setStyleSheet("QPushButton { border: none; background: transparent; } QPushButton:hover { background: #444444; border-radius: 4px; }");
    
    layout->addWidget(m_searchEdit);
    layout->addWidget(m_addCategoryBtn);
    
    m_mainLayout->addWidget(m_bottomToolbar);
}

/**
 * @brief 实现声明的 setupContextMenu 方法，初始化所有 Action 指针
 */
void CategoryPanel::setupContextMenu() {
    actNewData = new QAction(UiHelper::getIcon("add", QColor("#EEEEEE")), "新建数据...", this);
    actAssignToCategory = new QAction(UiHelper::getIcon("category", QColor("#EEEEEE")), "归类到...", this);
    actImportData = new QAction(UiHelper::getIcon("refresh", QColor("#EEEEEE")), "导入数据 / 刷新索引", this);
    actExport = new QAction(UiHelper::getIcon("redo", QColor("#EEEEEE")), "导出数据", this);

    actSetColor = new QAction(UiHelper::getIcon("palette", QColor("#EEEEEE")), "设置颜色", this);
    actRandomColor = new QAction(UiHelper::getIcon("random_color", QColor("#EEEEEE")), "随机颜色", this);
    actSetPresetTags = new QAction(UiHelper::getIcon("tag", QColor("#EEEEEE")), "设置预设标签", this);

    actNewSibling = new QAction("新建同级分类", this);
    actNewChild = new QAction("新建子分类", this);

    actTogglePin = new QAction(UiHelper::getIcon("pin", QColor("#EEEEEE")), "置顶 / 取消置顶", this);
    actRename = new QAction(UiHelper::getIcon("edit", QColor("#EEEEEE")), "重命名", this);
    actDelete = new QAction(UiHelper::getIcon("trash", QColor("#EEEEEE")), "删除", this);

    // 绑定基础创建逻辑 (从 UI 层穿透至 Model 层)
    connect(actNewSibling, &QAction::triggered, this, [this]() {
        QModelIndex currentIndex = m_treeView->currentIndex();
        bool ok;
        QString name = QInputDialog::getText(this, "新建同级分类", "请输入分类名称:", QLineEdit::Normal, "", &ok);
        if(ok && !name.isEmpty()) {
            QStandardItem* item = new QStandardItem(name);
            if(!currentIndex.isValid() || !m_treeModel->itemFromIndex(currentIndex)->parent()) {
                m_treeModel->appendRow(item);
            } else {
                m_treeModel->itemFromIndex(currentIndex)->parent()->appendRow(item);
            }
        }
    });

    connect(actNewChild, &QAction::triggered, this, [this]() {
        QModelIndex currentIndex = m_treeView->currentIndex();
        if(!currentIndex.isValid()) return;
        bool ok;
        QString name = QInputDialog::getText(this, "新建子分类", "请输入子分类名称:", QLineEdit::Normal, "", &ok);
        if(ok && !name.isEmpty()) {
            QStandardItem* parentItem = m_treeModel->itemFromIndex(currentIndex);
            parentItem->appendRow(new QStandardItem(name));
            m_treeView->expand(currentIndex);
        }
    });

    actSortByName = new QAction("按名称", this);
    actSortByCount = new QAction("按数量", this);
    actSortByTime = new QAction("按时间", this);

    actPasswordProtect = new QAction(UiHelper::getIcon("lock", QColor("#EEEEEE")), "密码保护", this);
}

/**
 * @brief 渲染右键菜单
 */
void CategoryPanel::onCustomContextMenuRequested(const QPoint& pos) {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #2B2B2B; border: 1px solid #444444; color: #EEEEEE; padding: 4px; border-radius: 6px; }"
        "QMenu::item { height: 26px; padding: 0 10px 0 10px; border-radius: 3px; font-size: 12px; }"
        "QMenu::item:selected { background-color: #505050; }"
        "QMenu::separator { height: 1px; background: #444444; margin: 4px 8px 4px 8px; }"
        "QMenu::right-arrow { image: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCIgZmlsbD0ibm9uZSIgc3Ryb2tlPSIjRUVFRUVFIiBzdHJva2Utd2lkdGg9IjIiIHN0cm9rZS1saW5lY2FwPSJyb3VuZCIgc3Ryb2tlLWxpbmVqb2luPSJyb3VuZCI+PHBvbHlsaW5lIHBvaW50cz0iOSAxOCAxNSAxMiA5IDYiPjwvcG9seWxpbmU+PC9zdmc+); width: 12px; height: 12px; right: 8px; }"
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

} // namespace ArcMeta
