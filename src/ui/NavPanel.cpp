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
#include <QThreadPool>
#include <QPersistentModelIndex>

namespace ArcMeta {

NavPanel::NavPanel(QWidget* parent)
    : QWidget(parent) {
    setFixedWidth(200);
    setStyleSheet("QWidget { background-color: #1E1E1E; color: #EEEEEE; border: none; }");

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    initUi();
}

void NavPanel::initUi() {
    QLabel* titleLabel = new QLabel("本地目录", this);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #4a90e2; padding: 10px 12px; background: #252526;");
    m_mainLayout->addWidget(titleLabel);

    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(true);
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(16);
    m_treeView->setExpandsOnDoubleClick(true);

    m_model = new QStandardItemModel(this);

    // 1. 占位：桌面
    QIcon desktopIcon = UiHelper::getIcon("layers", QColor("#B0B0B0"));
    QStandardItem* desktopItem = new QStandardItem(desktopIcon, "桌面");
    QString deskPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    desktopItem->setData(deskPath, Qt::UserRole + 1);
    desktopItem->appendRow(new QStandardItem("Loading..."));
    m_model->appendRow(desktopItem);

    // 2. 占位：此电脑
    QIcon computerIcon = UiHelper::getIcon("nav_next", QColor("#B0B0B0"));
    QStandardItem* computerItem = new QStandardItem(computerIcon, "此电脑");
    computerItem->setData("computer://", Qt::UserRole + 1);
    m_model->appendRow(computerItem);

    // 3. 占位：磁盘列表
    QIcon driveIcon = UiHelper::getIcon("folder", QColor("#B0B0B0"));
    const auto drives = QDir::drives();
    for (const QFileInfo& drive : drives) {
        QString driveName = drive.absolutePath();
        QStandardItem* driveItem = new QStandardItem(driveIcon, driveName);
        driveItem->setData(driveName, Qt::UserRole + 1);
        driveItem->appendRow(new QStandardItem("Loading..."));
        m_model->appendRow(driveItem);
    }

    // 2026-03-xx 架构修复：使用 QPersistentModelIndex 确保异步更新 UI Item 的内存安全
    // 捕获 QPersistentModelIndex 而非裸指针，防止在加载图标时切换模型导致崩溃
    for (int row = 0; row < m_model->rowCount(); ++row) {
        QPersistentModelIndex pIdx(m_model->index(row, 0));
        QString path = pIdx.data(Qt::UserRole + 1).toString();

        QThreadPool::globalInstance()->start([this, pIdx, path]() {
            QFileIconProvider iconProvider;
            QIcon realIcon;
            if (path == "computer://") {
                realIcon = iconProvider.icon(QFileIconProvider::Computer);
            } else {
                realIcon = iconProvider.icon(QFileInfo(path));
            }

            // 回调：校验索引有效性后才操作
            QMetaObject::invokeMethod(this, [this, pIdx, realIcon]() {
                if (pIdx.isValid()) {
                    m_model->itemFromIndex(pIdx)->setIcon(realIcon);
                }
            }, Qt::QueuedConnection);
        });
    }

    m_treeView->setModel(m_model);
    connect(m_treeView, &QTreeView::expanded, this, &NavPanel::onItemExpanded);

    m_treeView->setStyleSheet(
        "QTreeView { background-color: transparent; border: none; font-size: 12px; selection-background-color: #378ADD; outline: none; }"
        "QTreeView::item { height: 28px; padding-left: 4px; color: #EEEEEE; }"
        "QTreeView::item:hover { background-color: rgba(255, 255, 255, 0.05); }"
        "QTreeView::branch:has-children:!has-siblings:closed,"
        "QTreeView::branch:closed:has-children:has-siblings,"
        "QTreeView::branch:has-children:!has-siblings:open,"
        "QTreeView::branch:open:has-children:has-siblings { border-image: none; image: none; }"
    );

    m_treeView->verticalScrollBar()->setStyleSheet(
        "QScrollBar:vertical { background: transparent; width: 4px; }"
        "QScrollBar::handle:vertical { background: #444444; border-radius: 4px; }"
    );

    connect(m_treeView, &QTreeView::clicked, this, &NavPanel::onTreeClicked);
    m_mainLayout->addWidget(m_treeView);
}

void NavPanel::setRootPath(const QString& path) { Q_UNUSED(path); }

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

void NavPanel::onTreeClicked(const QModelIndex& index) {
    QString path = index.data(Qt::UserRole + 1).toString();
    if (!path.isEmpty()) emit directorySelected(path);
}

void NavPanel::onItemExpanded(const QModelIndex& index) {
    QStandardItem* item = m_model->itemFromIndex(index);
    if (item && item->rowCount() == 1 && item->child(0)->text() == "Loading...") {
        fetchChildDirs(item);
    }
}

void NavPanel::fetchChildDirs(QStandardItem* parent) {
    QString path = parent->data(Qt::UserRole + 1).toString();
    if (path.isEmpty() || path == "computer://") return;

    parent->removeRows(0, parent->rowCount());
    QDir dir(path);
    QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    QIcon folderIcon = UiHelper::getIcon("folder", QColor("#B0B0B0"));

    for (const QFileInfo& info : list) {
        QStandardItem* child = new QStandardItem(folderIcon, info.fileName());
        QString fullPath = info.absoluteFilePath();
        child->setData(fullPath, Qt::UserRole + 1);
        
        QDir subDir(fullPath);
        if (!subDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) {
            child->appendRow(new QStandardItem("Loading..."));
        }
        parent->appendRow(child);

        // 2026-03-xx：下层展开同样基于 QPersistentModelIndex 异步获取真实图标
        QPersistentModelIndex pIdx(child->index());
        QThreadPool::globalInstance()->start([this, pIdx, fullPath]() {
            QFileIconProvider iconProvider;
            QIcon realIcon = iconProvider.icon(QFileInfo(fullPath));
            QMetaObject::invokeMethod(this, [this, pIdx, realIcon]() {
                if (pIdx.isValid()) {
                    m_model->itemFromIndex(pIdx)->setIcon(realIcon);
                }
            }, Qt::QueuedConnection);
        });
    }
}

} // namespace ArcMeta
