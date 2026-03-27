#include "ContentPanel.h"
#include "FileItemDelegate.h"
#include "../mft/ParallelSearcher.h"
#include "../mft/PathBuilder.h"
#include "../db/CategoryItemRepo.h"
#include <QHeaderView>

namespace ArcMeta {

ContentPanel::ContentPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_viewStack = new QStackedWidget(this);

    m_model = new QFileSystemModel(this);
    m_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    m_model->setRootPath(QDir::rootPath());

    initListView();
    initGridView();

    m_viewStack->addWidget(m_gridView); // Index 0: Grid (Default)
    m_viewStack->addWidget(m_treeView); // Index 1: List

    layout->addWidget(m_viewStack);

    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ContentPanel::onSelectionChanged);
    connect(m_gridView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ContentPanel::onSelectionChanged);
}

void ContentPanel::onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected) {
    Q_UNUSED(deselected);
    if (!selected.indexes().isEmpty()) {
        QModelIndex index = selected.indexes().first();
        QString path = m_model->filePath(index);
        emit itemSelected(path);
    }
}

void ContentPanel::setViewMode(bool isGrid) {
    m_viewStack->setCurrentIndex(isGrid ? 0 : 1);
}

void ContentPanel::initListView() {
    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_model);
    m_treeView->setDragEnabled(true); // 启用拖拽

    m_treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeView->setColumnWidth(1, 80);
    m_treeView->setColumnWidth(2, 90);
    m_treeView->setColumnWidth(3, 150);
    m_treeView->setSortingEnabled(true);
}

void ContentPanel::initGridView() {
    m_gridView = new QListView(this);
    m_gridView->setViewMode(QListView::IconMode);
    m_gridView->setIconSize(QSize(64, 64));
    m_gridView->setGridSize(QSize(80, 112));
    m_gridView->setSpacing(8);
    m_gridView->setResizeMode(QListView::Adjust);
    m_gridView->setMovement(QListView::Static);
    m_gridView->setDragEnabled(true); // 启用拖拽

    m_gridView->setModel(m_model);
    m_gridView->setItemDelegate(new FileItemDelegate(this));
}

void ContentPanel::setRootPath(const QString& path) {
    QModelIndex index = m_model->index(path);
    if (index.isValid()) {
        m_treeView->setModel(m_model);
        m_gridView->setModel(m_model);
        m_treeView->setRootIndex(index);
        m_gridView->setRootIndex(index);
    }
}

void ContentPanel::performSearch(const FileIndex& index, const QString& query) {
    if (query.isEmpty()) {
        setRootPath(m_model->rootPath());
        return;
    }

    std::vector<DWORDLONG> frns = ParallelSearcher::search(index, query.toStdWString());
    PathBuilder builder(index);
    QStringList results;
    for (DWORDLONG frn : frns) {
        results << builder.getFullPath(frn, "C:");
    }

    if (!m_searchResultModel) m_searchResultModel = new QStringListModel(this);
    m_searchResultModel->setStringList(results);

    m_treeView->setModel(m_searchResultModel);
    m_gridView->setModel(m_searchResultModel);

    m_treeView->setRootIndex(QModelIndex());
    m_gridView->setRootIndex(QModelIndex());
}

void ContentPanel::showCategory(const FileIndex& index, int categoryId) {
    if (categoryId < 0) return;

    std::vector<DWORDLONG> frns = CategoryItemRepo::getItems(categoryId);
    PathBuilder builder(index);
    QStringList results;
    for (DWORDLONG frn : frns) {
        results << builder.getFullPath(frn, "C:");
    }

    if (!m_searchResultModel) m_searchResultModel = new QStringListModel(this);
    m_searchResultModel->setStringList(results);

    m_treeView->setModel(m_searchResultModel);
    m_gridView->setModel(m_searchResultModel);

    m_treeView->setRootIndex(QModelIndex());
    m_gridView->setRootIndex(QModelIndex());
}

} // namespace ArcMeta
