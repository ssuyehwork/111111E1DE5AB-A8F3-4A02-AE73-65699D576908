#include "ContentPanel.h"
#include "FileItemDelegate.h"
#include "CryptoWorkflow.h"
#include "BatchRenameDialog.h"
#include "../mft/ParallelSearcher.h"
#include "../mft/PathBuilder.h"
#include "../db/CategoryItemRepo.h"
#include "../db/FavoritesRepo.h"
#include <QHeaderView>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <windows.h>
#include <shellapi.h>

namespace ArcMeta {

ContentPanel::ContentPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_viewStack = new QStackedWidget(this);

    m_model = new QFileSystemModel(this);
    m_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    m_model->setRootPath(QDir::rootPath());

    m_proxyModel = new FileFilterProxyModel();
    m_proxyModel->setSourceModel(m_model);

    initListView();
    initGridView();

    m_viewStack->addWidget(m_gridView);
    m_viewStack->addWidget(m_treeView);

    layout->addWidget(m_viewStack);

    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ContentPanel::onSelectionChanged);
    connect(m_gridView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ContentPanel::onSelectionChanged);

    connect(m_treeView, &QTreeView::doubleClicked, [this](const QModelIndex& index) {
        QString path = m_model->filePath(m_proxyModel->mapToSource(index));
        CryptoWorkflow::handleFileOpen(path, this);
    });
    connect(m_gridView, &QListView::doubleClicked, [this](const QModelIndex& index) {
        QString path = m_model->filePath(m_proxyModel->mapToSource(index));
        CryptoWorkflow::handleFileOpen(path, this);
    });

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &ContentPanel::showContextMenu);
}

void ContentPanel::showContextMenu(const QPoint& pos) {
    QModelIndexList selected = m_viewStack->currentIndex() == 0 ?
        m_gridView->selectionModel()->selectedIndexes() :
        m_treeView->selectionModel()->selectedIndexes();

    if (selected.isEmpty()) return;

    QString path = m_model->filePath(m_proxyModel->mapToSource(selected.first()));
    QMenu menu(this);

    menu.addAction("打开", [path]() { QDesktopServices::openUrl(QUrl::fromLocalFile(path)); });
    menu.addAction("在资源管理器中显示", [path]() {
        QStringList args;
        args << "/select," << QDir::toNativeSeparators(path);
        QProcess::startDetached("explorer", args);
    });
    menu.addSeparator();
    menu.addAction("添加到收藏夹", [path]() {
        Favorite fav;
        fav.path = path;
        fav.name = QFileInfo(path).fileName();
        fav.type = QFileInfo(path).isDir() ? "folder" : "file";
        FavoritesRepo::add(fav);
    });
    menu.addSeparator();

    if (path.endsWith(".amenc")) {
        menu.addAction("解除加密", [path, this]() { CryptoWorkflow::handleFileOpen(path, this); });
    } else {
        menu.addAction("加密保护", [path, this]() { CryptoWorkflow::encryptFile(path, this); });
    }

    menu.addSeparator();
    if (selected.size() > 1) {
        menu.addAction("批量重命名 (Ctrl+Shift+S)", [this, selected]() {
            QStringList paths;
            for (auto& idx : selected) paths << m_model->filePath(m_proxyModel->mapToSource(idx));
            BatchRenameDialog dlg(paths, this);
            dlg.exec();
        });
        menu.addSeparator();
    }

    menu.addAction("重命名", [this, selected]() {
        if (m_viewStack->currentIndex() == 0) m_gridView->edit(selected.first());
        else m_treeView->edit(selected.first());
    });
    menu.addAction("删除", [path]() { QFile::moveToTrash(path); });
    menu.addSeparator();
    menu.addAction("复制路径", [path]() { QApplication::clipboard()->setText(path); });
    menu.addAction("属性", [path]() {
        std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_INVOKEIDLIST;
        sei.lpVerb = L"properties";
        sei.lpFile = wpath.c_str();
        sei.nShow = SW_SHOW;
        ShellExecuteExW(&sei);
    });

    menu.exec(mapToGlobal(pos));
}

void ContentPanel::onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected) {
    Q_UNUSED(deselected);
    if (!selected.indexes().isEmpty()) {
        QModelIndex index = selected.indexes().first();
        QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
        QString path = m_model->filePath(sourceIndex);
        emit itemSelected(path);
    }
}

void ContentPanel::setViewMode(bool isGrid) {
    m_viewStack->setCurrentIndex(isGrid ? 0 : 1);
}

void ContentPanel::refreshItem(const QString& filePath) {
    QModelIndex sourceIndex = m_model->index(filePath);
    if (sourceIndex.isValid()) {
        QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
        // 触发视图重绘该行/该项
        emit m_proxyModel->dataChanged(proxyIndex, proxyIndex);
    }
}

void ContentPanel::initListView() {
    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_proxyModel);
    m_treeView->setDragEnabled(true);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

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
    m_gridView->setDragEnabled(true);
    m_gridView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    m_gridView->setModel(m_proxyModel);
    m_gridView->setItemDelegate(new FileItemDelegate(this));
}

void ContentPanel::setRootPath(const QString& path) {
    m_treeView->setModel(m_proxyModel);
    m_gridView->setModel(m_proxyModel);

    QModelIndex sourceIndex = m_model->index(path);
    if (sourceIndex.isValid()) {
        QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
        m_treeView->setRootIndex(proxyIndex);
        m_gridView->setRootIndex(proxyIndex);
    }
}

void ContentPanel::applyFilter(const FilterState& state) {
    m_proxyModel->setFilterState(state);
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
    std::vector<DWORDLONG> frns;
    if (categoryId >= 0) {
        frns = CategoryItemRepo::getItems(categoryId);
    }

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
