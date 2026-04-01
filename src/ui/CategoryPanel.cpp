#include "CategoryPanel.h"
#include "CategoryModel.h"
#include "CategoryDelegate.h"
#include "DropTreeView.h"
#include "UiHelper.h"
#include "ToolTipOverlay.h"
#include "FramelessDialog.h"
#include "../db/CategoryRepo.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QScrollBar>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QRandomGenerator>
#include <QSet>

namespace ArcMeta {

CategoryPanel::CategoryPanel(QWidget* parent)
    : QFrame(parent) {
    setObjectName("SidebarContainer");
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumWidth(230);
    setStyleSheet("color: #EEEEEE;");
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    initUi();
    setupContextMenu();
}

void CategoryPanel::setupContextMenu() {
    m_partitionTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_partitionTree, &QWidget::customContextMenuRequested, [this](const QPoint& pos) {
        QModelIndex index = m_partitionTree->indexAt(pos);
        
        QMenu menu(this);
        // [PHYSICAL RESTORATION] 8px radius for context menu
        menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; border-radius: 8px; } "
                           "QMenu::item { padding: 6px 25px 6px 10px; border-radius: 4px; } "
                           "QMenu::icon { margin-left: 6px; } "
                           "QMenu::item:selected { background-color: #3E3E42; color: white; } "
                           "QMenu::separator { height: 1px; background: #444; margin: 4px 8px; }");

        // 基于规范逻辑：如果没有选中项，或者选中了“我的分类”根节点
        if (!index.isValid() || index.data(CategoryModel::NameRole).toString() == "我的分类") {
            menu.addAction(UiHelper::getIcon("add", QColor("#aaaaaa"), 18), "新建分类", this, &CategoryPanel::onCreateCategory);
            
            auto* sortMenu = menu.addMenu(UiHelper::getIcon("list_ul", QColor("#aaaaaa"), 18), "排列");
            sortMenu->setStyleSheet(menu.styleSheet());
            sortMenu->addAction("按名称");
            sortMenu->addAction("按时间");
        } else {
            // 具体分类项的右键菜单
            QString type = index.data(CategoryModel::TypeRole).toString();
            if (type == "category") {
                menu.addAction(UiHelper::getIcon("add", QColor("#3498db"), 18), "新建数据", this, &CategoryPanel::onAddData);
                menu.addAction(UiHelper::getIcon("branch", QColor("#3498db"), 18), "归类到此分类", this, &CategoryPanel::onClassifyToCategory);

                menu.addSeparator();
                
                menu.addAction(UiHelper::getIcon("palette", QColor("#f39c12"), 18), "设置颜色", this, &CategoryPanel::onSetColor);
                menu.addAction(UiHelper::getIcon("random_color", QColor("#e91e63"), 18), "随机颜色", this, &CategoryPanel::onRandomColor);
                menu.addAction(UiHelper::getIcon("tag", QColor("#e91e63"), 18), "设置预设标签", this, &CategoryPanel::onSetPresetTags);

                menu.addSeparator();

                menu.addAction(UiHelper::getIcon("add", QColor("#aaaaaa"), 18), "新建分类", this, &CategoryPanel::onCreateCategory);
                menu.addAction(UiHelper::getIcon("add", QColor("#3498db"), 18), "新建子分类", this, &CategoryPanel::onCreateSubCategory);

                menu.addSeparator();

                bool isPinned = index.data(CategoryModel::PinnedRole).toBool();
                menu.addAction(UiHelper::getIcon("pin", isPinned ? QColor("#FF551C") : QColor("#aaaaaa"), 18), 
                               isPinned ? "取消置顶" : "置顶分类", this, &CategoryPanel::onTogglePin);
                               
                menu.addAction(UiHelper::getIcon("edit", QColor("#aaaaaa"), 18), "重命名分类", this, &CategoryPanel::onRenameCategory);
                menu.addAction(UiHelper::getIcon("trash", QColor("#e74c3c"), 18), "删除分类", this, &CategoryPanel::onDeleteCategory);

                menu.addSeparator();

                auto* sortMenu = menu.addMenu(UiHelper::getIcon("list_ul", QColor("#aaaaaa"), 18), "排列");
                sortMenu->setStyleSheet(menu.styleSheet());
                sortMenu->addAction("按名称");
                sortMenu->addAction("按时间");

                auto* pwdMenu = menu.addMenu(UiHelper::getIcon("lock", QColor("#aaaaaa"), 18), "密码保护");
                pwdMenu->setStyleSheet(menu.styleSheet());
                pwdMenu->addAction("设置密码", this, &CategoryPanel::onSetPassword);
                pwdMenu->addAction("清除密码", this, &CategoryPanel::onClearPassword);
            }
        }
        
        if (!menu.isEmpty()) {
            menu.exec(m_partitionTree->viewport()->mapToGlobal(pos));
        }
    });
}

/**
 * @brief 递归保存 QTreeView 的展开状态
 */
static void saveExpandedState(QTreeView* tree, const QModelIndex& parent, QSet<int>& expandedIds) {
    for (int i = 0; i < tree->model()->rowCount(parent); ++i) {
        QModelIndex idx = tree->model()->index(i, 0, parent);
        if (tree->isExpanded(idx)) {
            int id = idx.data(CategoryModel::IdRole).toInt();
            if (id > 0) expandedIds.insert(id);
            // 递归处理子项
            saveExpandedState(tree, idx, expandedIds);
        }
    }
}

/**
 * @brief 递归恢复 QTreeView 的展开状态
 */
static void restoreExpandedState(QTreeView* tree, const QModelIndex& parent, const QSet<int>& expandedIds) {
    for (int i = 0; i < tree->model()->rowCount(parent); ++i) {
        QModelIndex idx = tree->model()->index(i, 0, parent);
        int id = idx.data(CategoryModel::IdRole).toInt();

        // “我的分类”根节点强制展开
        if (idx.data(CategoryModel::NameRole).toString() == "我的分类") {
            tree->setExpanded(idx, true);
            restoreExpandedState(tree, idx, expandedIds);
        } else if (id > 0 && expandedIds.contains(id)) {
            tree->setExpanded(idx, true);
            restoreExpandedState(tree, idx, expandedIds);
        }
    }
}

void CategoryPanel::onCreateCategory() {
    FramelessInputDialog dlg("新建分类", "请输入分类名称:", "", this);
    if (dlg.exec() == QDialog::Accepted) {
        QString text = dlg.text();
        if (!text.isEmpty()) {
            Category cat;
            cat.name = text.toStdWString();
            cat.parentId = 0;
            cat.color = L"#3498db";

            QSet<int> expandedIds;
            saveExpandedState(m_partitionTree, QModelIndex(), expandedIds);

            CategoryRepo::add(cat);
            m_partitionModel->refresh();

            restoreExpandedState(m_partitionTree, QModelIndex(), expandedIds);
        }
    }
}

void CategoryPanel::onCreateSubCategory() {
    QModelIndex index = m_partitionTree->currentIndex();
    if (!index.isValid()) return;
    int parentId = index.data(CategoryModel::IdRole).toInt();

    FramelessInputDialog dlg("新建子分类", "请输入子分类名称:", "", this);
    if (dlg.exec() == QDialog::Accepted) {
        QString text = dlg.text();
        if (!text.isEmpty()) {
            Category cat;
            cat.name = text.toStdWString();
            cat.parentId = parentId;
            cat.color = L"#3498db";

            QSet<int> expandedIds;
            saveExpandedState(m_partitionTree, QModelIndex(), expandedIds);
            // 确保父分类在列表中也被标记为展开
            if (parentId > 0) expandedIds.insert(parentId);

            CategoryRepo::add(cat);
            m_partitionModel->refresh();

            restoreExpandedState(m_partitionTree, QModelIndex(), expandedIds);
        }
    }
}

void CategoryPanel::onAddData() {
    ToolTipOverlay::instance()->showText(QCursor::pos(), "新建数据功能开发中...", 1000);
}

void CategoryPanel::onClassifyToCategory() {
    ToolTipOverlay::instance()->showText(QCursor::pos(), "归类功能开发中...", 1000);
}

void CategoryPanel::onSetColor() {
    ToolTipOverlay::instance()->showText(QCursor::pos(), "设置颜色功能开发中...", 1000);
}

void CategoryPanel::onRandomColor() {
    QModelIndex index = m_partitionTree->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(CategoryModel::IdRole).toInt();

    QStringList colors = {"#3498db", "#2ecc71", "#e74c3c", "#f1c40f", "#9b59b6", "#1abc9c", "#e67e22"};
    QString color = colors[QRandomGenerator::global()->bounded(static_cast<int>(colors.size()))];

    auto all = CategoryRepo::getAll();
    for(auto& cat : all) {
        if(cat.id == id) {
            cat.color = color.toStdWString();
            CategoryRepo::update(cat);
            break;
        }
    }

    QSet<int> expandedIds;
    saveExpandedState(m_partitionTree, QModelIndex(), expandedIds);

    m_partitionModel->refresh();

    restoreExpandedState(m_partitionTree, QModelIndex(), expandedIds);
}

void CategoryPanel::onSetPresetTags() {
    ToolTipOverlay::instance()->showText(QCursor::pos(), "预设标签功能开发中...", 1000);
}

void CategoryPanel::onTogglePin() {
    QModelIndex index = m_partitionTree->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(CategoryModel::IdRole).toInt();
    bool isPinned = index.data(CategoryModel::PinnedRole).toBool();

    auto all = CategoryRepo::getAll();
    for(auto& cat : all) {
        if(cat.id == id) {
            cat.pinned = !isPinned;
            CategoryRepo::update(cat);
            break;
        }
    }

    QSet<int> expandedIds;
    saveExpandedState(m_partitionTree, QModelIndex(), expandedIds);

    m_partitionModel->refresh();

    restoreExpandedState(m_partitionTree, QModelIndex(), expandedIds);
}

void CategoryPanel::onSetPassword() {
    ToolTipOverlay::instance()->showText(QCursor::pos(), "密码设置功能开发中...", 1000);
}

void CategoryPanel::onClearPassword() {
    QModelIndex index = m_partitionTree->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(CategoryModel::IdRole).toInt();

    auto all = CategoryRepo::getAll();
    for(auto& cat : all) {
        if(cat.id == id) {
            cat.encrypted = false;
            CategoryRepo::update(cat);
            break;
        }
    }

    QSet<int> expandedIds;
    saveExpandedState(m_partitionTree, QModelIndex(), expandedIds);

    m_partitionModel->refresh();

    restoreExpandedState(m_partitionTree, QModelIndex(), expandedIds);
}

void CategoryPanel::onRenameCategory() {
    QModelIndex index = m_partitionTree->currentIndex();
    if (index.isValid()) m_partitionTree->edit(index);
}

void CategoryPanel::onDeleteCategory() {
    QModelIndex index = m_partitionTree->currentIndex();
    if (index.isValid()) {
        int id = index.data(CategoryModel::IdRole).toInt();
        if (id > 0) {
            QSet<int> expandedIds;
            saveExpandedState(m_partitionTree, QModelIndex(), expandedIds);

            ArcMeta::CategoryRepo::remove(id);
            m_partitionModel->refresh();

            restoreExpandedState(m_partitionTree, QModelIndex(), expandedIds);
        }
    }
}

void CategoryPanel::setFocusHighlight(bool visible) {
    if (m_focusLine) m_focusLine->setVisible(visible);
}

void CategoryPanel::initUi() {
    // 物理还原：1px 翠绿高亮焦点线 (#2ecc71)
    m_focusLine = new QWidget(this);
    m_focusLine->setFixedHeight(1);
    m_focusLine->setStyleSheet("background-color: #2ecc71;");
    m_focusLine->hide(); // 初始隐藏
    m_mainLayout->addWidget(m_focusLine);

    // 1. 标题栏
    QWidget* header = new QWidget(this);
    header->setObjectName("ContainerHeader");
    header->setFixedHeight(32);
    header->setStyleSheet(
        "QWidget#ContainerHeader {"
        "  background-color: #252526;"
        "  border-bottom: 1px solid #333;"
        "}"
    );
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(15, 2, 15, 0);
    headerLayout->setSpacing(8);

    QLabel* iconLabel = new QLabel(header);
    iconLabel->setPixmap(UiHelper::getIcon("category", QColor("#3498db"), 18).pixmap(18, 18));
    headerLayout->addWidget(iconLabel);

    QLabel* titleLabel = new QLabel("数据分类", header);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #3498db; background: transparent; border: none;");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_mainLayout->addWidget(header);

    // 2. 内容区包裹容器 (物理还原 8, 8, 8, 8 呼吸边距)
    QWidget* sbContent = new QWidget(this);
    sbContent->setStyleSheet("background: transparent; border: none;");
    auto* sbContentLayout = new QVBoxLayout(sbContent);
    sbContentLayout->setContentsMargins(8, 8, 8, 8);
    sbContentLayout->setSpacing(0);

    QString treeStyle = R"(
        QTreeView { background-color: transparent; border: none; color: #CCC; outline: none; }
        QTreeView::branch:has-children:closed { image: url(:/icons/arrow_right.svg); }
        QTreeView::branch:has-children:open   { image: url(:/icons/arrow_down.svg); }
        QTreeView::branch:has-children:closed:has-siblings { image: url(:/icons/arrow_right.svg); }
        QTreeView::branch:has-children:open:has-siblings   { image: url(:/icons/arrow_down.svg); }
        QTreeView::item { height: 26px; padding-left: 0px; }
    )";

    // 系统树
    m_systemTree = new DropTreeView(this);
    m_systemTree->setStyleSheet(treeStyle); 
    m_systemTree->setItemDelegate(new CategoryDelegate(this));
    m_systemModel = new CategoryModel(CategoryModel::System, this);
    m_systemTree->setModel(m_systemModel);
    m_systemTree->setHeaderHidden(true);
    m_systemTree->setRootIsDecorated(false);
    m_systemTree->setIndentation(10);
    m_systemTree->setFixedHeight(176); // 8 items * 22px
    m_systemTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_systemTree->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // 分区树
    m_partitionTree = new DropTreeView(this);
    m_partitionTree->setStyleSheet(treeStyle);
    m_partitionTree->setItemDelegate(new CategoryDelegate(this));
    m_partitionModel = new CategoryModel(CategoryModel::User, this);
    m_partitionTree->setModel(m_partitionModel);
    m_partitionTree->setHeaderHidden(true);
    m_partitionTree->setRootIsDecorated(true);
    m_partitionTree->setIndentation(20);
    m_partitionTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_partitionTree->setDragEnabled(true);
    m_partitionTree->setAcceptDrops(true);
    m_partitionTree->setDropIndicatorShown(true);
    m_partitionTree->setDragDropMode(QAbstractItemView::InternalMove);
    m_partitionTree->setDefaultDropAction(Qt::MoveAction);
    m_partitionTree->expandAll();
    m_partitionTree->setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(m_systemTree, &QTreeView::clicked, [this](const QModelIndex& index) {
        emit categorySelected(index.data(CategoryModel::NameRole).toString());
    });
    connect(m_partitionTree, &QTreeView::clicked, [this](const QModelIndex& index) {
        emit categorySelected(index.data(CategoryModel::NameRole).toString());
    });

    connect(m_partitionTree, &DropTreeView::pathsDropped, [this](const QStringList& paths, const QModelIndex& index) {
        if (!index.isValid()) return;
        
        int categoryId = index.data(CategoryModel::IdRole).toInt();
        QString categoryName = index.data(CategoryModel::NameRole).toString();
        
        // 核心红线：仅对具体分类执行“建立引用收藏”逻辑
        if (categoryId > 0) {
            int count = 0;
            for (const QString& path : paths) {
                if (CategoryRepo::addItemToCategory(categoryId, path.toStdWString())) {
                    count++;
                }
            }
            // 物理反馈：提示收藏成功
            ToolTipOverlay::instance()->showText(QCursor::pos(), 
                QString("<b style='color:#2ecc71;'>已收藏 %1 个项目到 [%2]</b>").arg(count).arg(categoryName), 1200);
        }
    });
    
    sbContentLayout->addWidget(m_systemTree);
    sbContentLayout->addWidget(m_partitionTree);
    m_mainLayout->addWidget(sbContent, 1);
}

bool CategoryPanel::eventFilter(QObject* obj, QEvent* event) {
    return QFrame::eventFilter(obj, event);
}

} // namespace ArcMeta
