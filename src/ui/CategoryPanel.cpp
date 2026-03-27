#include "CategoryPanel.h"
#include <QHeaderView>
#include "../db/CategoryItemRepo.h"
#include "../db/CategoryRepo.h"

namespace ArcMeta {

CategoryPanel::CategoryPanel(QWidget* parent) : QWidget(parent) {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    initStatisticsArea();
    initCategoryTree();
    initBottomToolbar();

    refreshStatistics();
}

void CategoryPanel::initStatisticsArea() {
    m_statsList = new QListWidget(this);
    m_statsList->setFixedHeight(220);
    m_statsList->setStyleSheet("QListWidget { border: none; background: transparent; }");

    QStringList statsItems = {
        "全部数据", "今日数据", "昨日数据", "最近访问",
        "未分类", "未标签", "收藏", "回收站"
    };

    for (const QString& name : statsItems) {
        auto* item = new QListWidgetItem(name, m_statsList);
        item->setSizeHint(QSize(0, 26));
    }

    connect(m_statsList, &QListWidget::currentRowChanged, [this](int row) {
        emit categorySelected(-1 - row);
    });

    m_mainLayout->addWidget(m_statsList);
}

void CategoryPanel::refreshStatistics() {
    auto updateItem = [this](int row, const QString& name, int count) {
        if (auto* item = m_statsList->item(row)) {
            item->setText(QString("%1 (%2)").arg(name).arg(count));
        }
    };

    updateItem(0, "全部数据", CategoryRepo::getTotalItemCount());
    updateItem(1, "今日数据", CategoryRepo::getTodayItemCount());
    updateItem(4, "未分类", CategoryRepo::getUncategorizedCount());
    updateItem(5, "未标签", CategoryRepo::getUntaggedCount());
    updateItem(6, "收藏", CategoryRepo::getPinnedCount());
}

void CategoryPanel::initCategoryTree() {
    m_categoryTree = new QTreeView(this);
    m_categoryTree->header()->hide();
    m_categoryTree->setIndentation(15);
    m_categoryTree->setDragEnabled(true);
    m_categoryTree->setAcceptDrops(true);
    m_categoryTree->setDropIndicatorShown(true);
    m_categoryTree->setDragDropMode(QAbstractItemView::InternalMove);
    m_categoryTree->setStyleSheet("QTreeView { border: none; background: transparent; }");

    m_model = new CategoryModel(this);
    m_categoryTree->setModel(m_model);

    m_categoryTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_categoryTree, &QTreeView::customContextMenuRequested, this, &CategoryPanel::showContextMenu);

    connect(m_categoryTree->selectionModel(), &QItemSelectionModel::selectionChanged, [this](const QItemSelection& selected) {
        if (!selected.indexes().isEmpty()) {
            int id = selected.indexes().first().data(CategoryModel::IdRole).toInt();
            emit categorySelected(id);
        }
    });

    m_mainLayout->addWidget(m_categoryTree, 1);
}

void CategoryPanel::showContextMenu(const QPoint& pos) {
    QModelIndex index = m_categoryTree->indexAt(pos);
    QMenu menu(this);

    menu.addAction("新建子分类", this, &CategoryPanel::onAddCategory);
    if (index.isValid()) {
        menu.addAction("重命名", this, &CategoryPanel::onRenameCategory);
        menu.addAction("删除分类", this, &CategoryPanel::onDeleteCategory);
    }

    menu.exec(m_categoryTree->mapToGlobal(pos));
}

void CategoryPanel::onAddCategory() {
    QModelIndex index = m_categoryTree->currentIndex();
    Category cat;
    cat.name = "新分类";
    cat.parentId = index.isValid() ? index.data(CategoryModel::IdRole).toInt() : 0;
    CategoryRepo::add(cat);
    m_model->refresh();
}

void CategoryPanel::onRenameCategory() {
    QModelIndex index = m_categoryTree->currentIndex();
    if (index.isValid()) {
        m_categoryTree->edit(index);
    }
}

void CategoryPanel::onDeleteCategory() {
    QModelIndex index = m_categoryTree->currentIndex();
    if (index.isValid()) {
        int id = index.data(CategoryModel::IdRole).toInt();
        CategoryRepo::remove(id);
        m_model->refresh();
    }
}

void CategoryPanel::initBottomToolbar() {
    QWidget* toolbar = new QWidget(this);
    toolbar->setFixedHeight(36);
    QHBoxLayout* layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(5);

    QPushButton* btnAdd = new QPushButton("+ 新建分类");
    btnAdd->setCursor(Qt::PointingHandCursor);
    connect(btnAdd, &QPushButton::clicked, this, &CategoryPanel::onAddCategory);

    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("搜索分类...");

    layout->addWidget(btnAdd);
    layout->addWidget(m_searchEdit);

    m_mainLayout->addWidget(toolbar);
}

} // namespace ArcMeta
