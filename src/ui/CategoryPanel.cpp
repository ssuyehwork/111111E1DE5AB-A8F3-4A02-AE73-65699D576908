#include "CategoryPanel.h"
#include "CategoryModel.h"
#include "CategoryDelegate.h"
#include "DropTreeView.h"
#include "UiHelper.h"
#include "../db/CategoryRepo.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QScrollBar>
#include <QMenu>
#include <QAction>
#include <QApplication>

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

    m_focusLine = new QWidget(this);
    m_focusLine->setObjectName("focusLine");
    m_focusLine->setFixedHeight(1);
    m_focusLine->setStyleSheet("background-color: #2ecc71; border: none;");
    m_focusLine->hide();
    m_mainLayout->addWidget(m_focusLine);
    
    initUi();
    setupContextMenu();

    // 监听全局焦点以驱动高亮线
    if (qApp) qApp->installEventFilter(this);
}

void CategoryPanel::setupContextMenu() {
    m_partitionTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_partitionTree, &QWidget::customContextMenuRequested, [this](const QPoint& pos) {
        QMenu menu(this);
        menu.setStyleSheet("QMenu { background: #2B2B2B; color: white; border: 1px solid #444; padding: 4px; border-radius: 4px; }");

        QModelIndex index = m_partitionTree->indexAt(pos);
        if (index.isValid()) {
            menu.addAction("重命名", this, &CategoryPanel::onRenameCategory);
            menu.addAction("删除", this, &CategoryPanel::onDeleteCategory);
        } else {
            menu.addAction("新建分类", this, &CategoryPanel::onCreateCategory);
        }
        menu.exec(m_partitionTree->viewport()->mapToGlobal(pos));
    });
}

void CategoryPanel::onCreateCategory() {
    Category cat;
    cat.name = L"新分类";
    cat.parentId = 0;
    cat.color = L"#aaaaaa";
    CategoryRepo::add(cat);
    m_partitionModel->refresh();
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

void CategoryPanel::initUi() {
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
    
    sbContentLayout->addWidget(m_systemTree);
    sbContentLayout->addWidget(m_partitionTree);
    m_mainLayout->addWidget(sbContent, 1);
}

bool CategoryPanel::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) {
        QWidget* focus = QApplication::focusWidget();
        bool isChildFocused = false;
        if (focus) {
            QWidget* p = focus;
            while (p) {
                if (p == this) { isChildFocused = true; break; }
                p = p->parentWidget();
            }
        }
        m_focusLine->setVisible(isChildFocused);
    }
    return QFrame::eventFilter(obj, event);
}

} // namespace ArcMeta
