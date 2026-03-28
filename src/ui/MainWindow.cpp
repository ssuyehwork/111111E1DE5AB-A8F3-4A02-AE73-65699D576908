#include "ToolTipOverlay.h"
#include "MainWindow.h"
#include <algorithm>
#include <utility>
#include <QDebug>
#include <QListView>
#include <QTreeView>
#include "StringUtils.h"
#include "../core/DatabaseManager.h"
#include "IconHelper.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QMenu>
#include <QShortcut>
#include <QItemSelection>
#include <QSettings>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include <QFile>
#include <QCoreApplication>
#include <QFileDialog>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QMimeData>
#include "FileItemDelegate.h"
#include "../meta/AmMetaJson.h"
#include "../core/ShortcutManager.h"
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QSlider>
#include <QCheckBox>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent, Qt::FramelessWindowHint) {
    setWindowTitle("KingPenguin - 超级资源管理器");
    setAcceptDrops(true);
    resize(1200, 800);
    setMouseTracking(true);
    initUI();
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    connect(m_searchTimer, &QTimer::timeout, this, &MainWindow::refreshData);
    restoreLayout(); 
    setupShortcuts();
    connect(&ShortcutManager::instance(), &ShortcutManager::shortcutsChanged, this, &MainWindow::updateShortcuts);
    installEventFilter(this);
}

void MainWindow::initUI() {
    auto* centralWidget = new QWidget(this);
    centralWidget->setObjectName("CentralWidget");
    centralWidget->setStyleSheet("#CentralWidget { background-color: #1E1E1E; }");
    setCentralWidget(centralWidget);
    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    m_header = new HeaderBar(this);
    if (auto* title = m_header->findChild<QLabel*>()) title->setText("超级资源管理器");
    connect(m_header, &HeaderBar::searchChanged, this, [this](const QString& text){ m_currentKeyword = text; m_currentPage = 1; m_searchTimer->start(300); });
    connect(m_header, &HeaderBar::refreshRequested, this, &MainWindow::refreshData);
    connect(m_header, &HeaderBar::stayOnTopRequested, this, [this](bool checked){
        #ifdef Q_OS_WIN
        SetWindowPos((HWND)winId(), checked ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        #else
        setWindowFlag(Qt::WindowStaysOnTopHint, checked); show();
        #endif
    });
    connect(m_header, &HeaderBar::filterRequested, this, [this](){ bool v = !m_filterWrapper->isVisible(); m_filterWrapper->setVisible(v); m_header->setFilterActive(v); });
    connect(m_header, &HeaderBar::toggleSidebar, this, [this](){ m_categoryContainer->setVisible(!m_categoryContainer->isVisible()); });
    connect(m_header, &HeaderBar::metadataToggled, this, [this](bool checked){ m_metaPanel->setVisible(checked); });
    connect(m_header, &HeaderBar::windowClose, this, &MainWindow::close);
    connect(m_header, &HeaderBar::windowMinimize, this, &MainWindow::showMinimized);
    connect(m_header, &HeaderBar::windowMaximize, this, [this](){ if (isMaximized()) showNormal(); else showMaximized(); });
    mainLayout->addWidget(m_header);
    auto* contentWidget = new QWidget(centralWidget);
    auto* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(5, 5, 5, 5);
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(5);
    splitter->setStyleSheet("QSplitter::handle { background: transparent; }");
    m_categoryContainer = new QFrame();
    m_categoryContainer->setMinimumWidth(230);
    auto* l1Layout = new QVBoxLayout(m_categoryContainer);
    m_placesTree = new QTreeWidget(); m_placesTree->setHeaderHidden(true);
    connect(m_placesTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) { QString p = item->data(0, Qt::UserRole).toString(); if (!p.isEmpty()) loadDirectory(p); });
    l1Layout->addWidget(m_placesTree);
    splitter->addWidget(m_categoryContainer);
    m_navContainer = new QFrame();
    m_navContainer->setMinimumWidth(230);
    auto* l2Layout = new QVBoxLayout(m_navContainer);
    m_navTree = new QTreeWidget(); m_navTree->setHeaderHidden(true);
    connect(m_navTree, &QTreeWidget::itemExpanded, this, &MainWindow::onItemExpanded);
    connect(m_navTree, &QTreeWidget::itemSelectionChanged, this, [this](){
        auto items = m_navTree->selectedItems();
        if (!items.isEmpty()) {
            QString p = items.first()->data(0, Qt::UserRole).toString();
            if (QFileInfo(p).isDir()) { m_contentStack->setCurrentIndex(0); loadDirectory(p); }
            else { m_contentStack->setCurrentIndex(1); QVariantMap item; item["content"] = p; m_editor->setItem(item, false); }
        }
    });
    l2Layout->addWidget(m_navTree);
    splitter->addWidget(m_navContainer);
    m_contentContainer = new QFrame();
    auto* midLayout = new QVBoxLayout(m_contentContainer);
    m_contentStack = new QStackedWidget();
    m_fileView = new QListView(); m_fileView->setViewMode(QListView::IconMode);
    m_fileModel = new QStandardItemModel(this); m_fileView->setModel(m_fileModel);
    m_fileView->setItemDelegate(new FileItemDelegate(this));
    connect(m_fileView, &QListView::doubleClicked, this, [this](const QModelIndex& idx) {
        QString p = idx.data((int)FileItemDelegate::PathRole).toString();
        if (QFileInfo(p).isDir()) loadDirectory(p); else QDesktopServices::openUrl(QUrl::fromLocalFile(p));
    });
    connect(m_fileView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](){
        auto idxs = m_fileView->selectionModel()->selectedIndexes();
        if (idxs.isEmpty()) m_metaPanel->clearSelection();
        else if (idxs.size() == 1) { QString p = idxs.first().data((int)FileItemDelegate::PathRole).toString(); QVariantMap m; m["content"] = p; m_metaPanel->setItem(m); }
        else m_metaPanel->setMultipleNotes(idxs.size());
    });
    m_contentStack->addWidget(m_fileView);
    m_editor = new Editor(); m_editor->togglePreview(true); m_contentStack->addWidget(m_editor);
    midLayout->addWidget(m_contentStack);
    m_contentStatusBar = new QWidget();
    auto* sLayout = new QHBoxLayout(m_contentStatusBar);
    m_itemCountLabel = new QLabel("0 个项目"); sLayout->addWidget(m_itemCountLabel);
    m_zoomSlider = new QSlider(Qt::Horizontal); m_zoomSlider->setRange(48, 256); m_zoomSlider->setValue(96);
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int v){ m_fileView->setIconSize(QSize(v, v)); m_fileView->setGridSize(QSize(v + 30, v + 60)); });
    sLayout->addWidget(m_zoomSlider);
    midLayout->addWidget(m_contentStatusBar);
    splitter->addWidget(m_contentContainer);
    m_metaPanel = new MetadataPanel(this); connect(m_metaPanel, &MetadataPanel::noteUpdated, this, &MainWindow::refreshData);
    splitter->addWidget(m_metaPanel);
    m_filterWrapper = new QFrame();
    auto* fLayout = new QVBoxLayout(m_filterWrapper);
    m_filterPanel = new FilterPanel(this); fLayout->addWidget(m_filterPanel);
    splitter->addWidget(m_filterWrapper);
    contentLayout->addWidget(splitter);
    mainLayout->addWidget(contentWidget);
    m_placesTree->installEventFilter(this); m_navTree->installEventFilter(this); m_fileView->installEventFilter(this);
    loadDrives();
}

void MainWindow::loadDrives() {
    m_placesTree->clear(); m_navTree->clear();
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    auto addItems = [&](QTreeWidget* tree) {
        auto* d = new QTreeWidgetItem(tree); d->setText(0, "桌面"); d->setData(0, Qt::UserRole, desktopPath);
        auto* pc = new QTreeWidgetItem(tree); pc->setText(0, "此电脑");
        for (const QStorageInfo& s : QStorageInfo::mountedVolumes()) {
            if (!s.isValid() || !s.isReady()) continue;
            auto* dr = new QTreeWidgetItem(pc); dr->setText(0, s.rootPath()); dr->setData(0, Qt::UserRole, s.rootPath());
        }
        pc->setExpanded(true);
    };
    addItems(m_placesTree); addItems(m_navTree);
}

void MainWindow::onItemExpanded(QTreeWidgetItem* item) {
    QString p = item->data(0, Qt::UserRole).toString(); if (p.isEmpty()) return;
    qDeleteAll(item->takeChildren());
    QDir dir(p);
    for (const QFileInfo& info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        auto* c = new QTreeWidgetItem(item); c->setText(0, info.fileName()); c->setData(0, Qt::UserRole, info.absoluteFilePath());
        new QTreeWidgetItem(c);
    }
}

void MainWindow::refreshData() { if (auto* it = m_navTree->currentItem()) loadDirectory(it->data(0, Qt::UserRole).toString()); }
void MainWindow::setupShortcuts() {
    auto add = [&](const QString& id, std::function<void()> f) { new QShortcut(ShortcutManager::instance().getShortcut(id), this, f); };
    add("mw_refresh", [this](){ refreshData(); }); add("mw_pin", [this](){ doTogglePin(); }); add("mw_close", [this](){ close(); });
}

void MainWindow::doTogglePin() {
    auto idxs = m_fileView->selectionModel()->selectedIndexes();
    for (const auto& idx : idxs) {
        QString p = idx.data((int)FileItemDelegate::PathRole).toString();
        bool pinned = !idx.data((int)FileItemDelegate::IsPinnedRole).toBool();
        if (AmMetaJson::setItemMeta(p, "pinned", pinned)) m_fileModel->setData(idx, pinned, (int)FileItemDelegate::IsPinnedRole);
    }
}

void MainWindow::doSetRating(int r) {
    auto idxs = m_fileView->selectionModel()->selectedIndexes();
    for (const auto& idx : idxs) {
        QString p = idx.data((int)FileItemDelegate::PathRole).toString();
        if (AmMetaJson::setItemMeta(p, "rating", r)) m_fileModel->setData(idx, r, (int)FileItemDelegate::RatingRole);
    }
}

void MainWindow::doDeleteSelected(bool) {}
void MainWindow::doCopyTags() {}
void MainWindow::doPasteTags() {}
void MainWindow::doRepeatAction() {}
bool MainWindow::eventFilter(QObject* w, QEvent* e) {
    if (e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Tab) {
            if (w == m_placesTree) m_navTree->setFocus(); else if (w == m_navTree) m_fileView->setFocus(); else m_placesTree->setFocus();
            return true;
        }
    }
    return QMainWindow::eventFilter(w, e);
}

void MainWindow::updateToolboxStatus(bool) {}
void MainWindow::onSelectionChanged(const QItemSelection&, const QItemSelection&) {}
void MainWindow::showContextMenu(const QPoint&) {}
void MainWindow::saveLayout() {}
void MainWindow::restoreLayout() {}
void MainWindow::updateShortcuts() {}
void MainWindow::updateFocusLines() {}
bool MainWindow::event(QEvent* e) { return QMainWindow::event(e); }
void MainWindow::dragEnterEvent(QDragEnterEvent* e) { if (e->mimeData()->hasUrls()) e->acceptProposedAction(); }
void MainWindow::dragMoveEvent(QDragMoveEvent* e) { e->acceptProposedAction(); }
void MainWindow::dropEvent(QDropEvent*) {}
void MainWindow::showEvent(QShowEvent* e) { QMainWindow::showEvent(e); }
void MainWindow::keyPressEvent(QKeyEvent* e) { QMainWindow::keyPressEvent(e); }
void MainWindow::closeEvent(QCloseEvent* e) { QMainWindow::closeEvent(e); }
bool MainWindow::verifyExportPermission() { return true; }
#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray& e, void* m, qintptr* r) { return QMainWindow::nativeEvent(e, m, r); }
#endif
