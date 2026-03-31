#include "CategoryPanel.h"
#include "CategoryModel.h"
#include "CategoryDelegate.h"
#include "DropTreeView.h"
#include "UiHelper.h"
#include "ToolTipOverlay.h"
#include "../db/CategoryRepo.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QScrollBar>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QInputDialog>

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
        menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                           "QMenu::item { padding: 6px 10px 6px 10px; border-radius: 3px; } "
                           "QMenu::icon { margin-left: 6px; } "
                           "QMenu::item:selected { background-color: #3E3E42; color: white; }");

        // 基于规范逻辑：如果没有选中项，或者选中了“我的分类”根节点
        if (!index.isValid() || index.data(CategoryModel::NameRole).toString() == "我的分类") {
            menu.addAction(UiHelper::getIcon("add", QColor("#3498db"), 18), "新建分类", this, &CategoryPanel::onCreateCategory);

            auto* importMenu = menu.addMenu(UiHelper::getIcon("file_import", QColor("#1abc9c"), 18), "导入数据");
            importMenu->setStyleSheet(menu.styleSheet());
            importMenu->addAction(UiHelper::getIcon("file", QColor("#1abc9c"), 18), "导入文件(s)...");
            importMenu->addAction(UiHelper::getIcon("folder", QColor("#1abc9c"), 18), "导入文件夹...");
        } else {
            // 具体分类项的右键菜单
            QString type = index.data(CategoryModel::TypeRole).toString();
            if (type == "category") {
                menu.addAction(UiHelper::getIcon("add", QColor("#3498db"), 18), "新建子分类");
                menu.addAction(UiHelper::getIcon("edit", QColor("#3498db"), 18), "重命名", this, &CategoryPanel::onRenameCategory);
                menu.addSeparator();

                bool isPinned = index.data(CategoryModel::PinnedRole).toBool();
                menu.addAction(UiHelper::getIcon("pin", isPinned ? QColor("#FF551C") : QColor("#aaaaaa"), 18),
                               isPinned ? "取消置顶" : "置顶分类");

                menu.addAction(UiHelper::getIcon("trash", QColor("#e74c3c"), 18), "删除分类", this, &CategoryPanel::onDeleteCategory);
            }
        }

        if (!menu.isEmpty()) {
            menu.exec(m_partitionTree->viewport()->mapToGlobal(pos));
        }
    });
}

void CategoryPanel::onCreateCategory() {
    bool ok;
    QString text = QInputDialog::getText(this, "新建分类", "名称:", QLineEdit::Normal, "", &ok);
    if (ok && !text.isEmpty()) {
        Category cat;
        cat.name = text.toStdWString();
        cat.parentId = 0;
        cat.color = L"#3498db";
        CategoryRepo::add(cat);
        m_partitionModel->refresh();
    }
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
            ArcMeta::CategoryRepo::remove(id);
            m_partitionModel->refresh();
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
    headerLayout->setContentsMargins(15, 0, 15, 0);
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
        QTreeView::item { height: 22px; padding-left: 10px; }
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
    m_partitionTree->setIndentation(10);
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
