#include "ContentPanel.h"
#include "../../SvgIcons.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QSvgRenderer>
#include <QPainter>
#include <QHeaderView>
#include <QScrollBar>
#include <QStyle>
#include <QLabel>
#include <QAction>
#include <QMenu>
#include <QAbstractItemView>
#include <QStandardItem>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QStyleOptionViewItem>
#include <QItemSelectionModel>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QFileIconProvider>
#include <QApplication>
#include <QProcess>
#include <QClipboard>
#include <QMimeData>
#include <QLineEdit>
#include <QInputDialog>
#include <windows.h>
#include <shellapi.h>
#include "../mft/MftReader.h"
#include "../mft/PathBuilder.h"
#include "../meta/AmMetaJson.h"
#include "UiHelper.h"

namespace ArcMeta {

/**
 * @brief 内部代理类：专门处理高级筛选逻辑
 */
class FilterProxyModel : public QSortFilterProxyModel {
public:
    explicit FilterProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

    FilterState currentFilter;

    void updateFilter() {
        invalidate();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);

        // 1. 评级过滤
        if (!currentFilter.ratings.isEmpty()) {
            int r = idx.data(RatingRole).toInt();
            if (!currentFilter.ratings.contains(r)) return false;
        }

        // 2. 颜色过滤
        if (!currentFilter.colors.isEmpty()) {
            QString c = idx.data(ColorRole).toString();
            if (!currentFilter.colors.contains(c)) return false;
        }

        // 3. 标签过滤
        if (!currentFilter.tags.isEmpty()) {
            QStringList itemTags = idx.data(TagsRole).toStringList();
            bool matchTag = false;
            for (const QString& fTag : currentFilter.tags) {
                if (fTag == "__none__") {
                    if (itemTags.isEmpty()) { matchTag = true; break; }
                } else {
                    if (itemTags.contains(fTag)) { matchTag = true; break; }
                }
            }
            if (!matchTag) return false;
        }

        // 4. 类型过滤
        if (!currentFilter.types.isEmpty()) {
            QString type = idx.data(TypeRole).toString(); // "folder" or "file"
            QString ext = QFileInfo(idx.data(PathRole).toString()).suffix().toUpper();
            bool matchType = false;
            for (const QString& fType : currentFilter.types) {
                if (fType == "folder") {
                    if (type == "folder") { matchType = true; break; }
                } else {
                    if (ext == fType.toUpper()) { matchType = true; break; }
                }
            }
            if (!matchType) return false;
        }

        // 5. 创建日期过滤
        if (!currentFilter.createDates.isEmpty()) {
            QDate d = QFileInfo(idx.data(PathRole).toString()).birthTime().date();
            QDate today = QDate::currentDate();
            QString dStr = d.toString("yyyy-MM-dd");
            bool matchDate = false;
            for (const QString& fDate : currentFilter.createDates) {
                if (fDate == "today" && d == today) { matchDate = true; break; }
                if (fDate == "yesterday" && d == today.addDays(-1)) { matchDate = true; break; }
                if (fDate == dStr) { matchDate = true; break; }
            }
            if (!matchDate) return false;
        }

        // 6. 修改日期过滤
        if (!currentFilter.modifyDates.isEmpty()) {
            QDate d = QFileInfo(idx.data(PathRole).toString()).lastModified().date();
            QDate today = QDate::currentDate();
            QString dStr = d.toString("yyyy-MM-dd");
            bool matchDate = false;
            for (const QString& fDate : currentFilter.modifyDates) {
                if (fDate == "today" && d == today) { matchDate = true; break; }
                if (fDate == "yesterday" && d == today.addDays(-1)) { matchDate = true; break; }
                if (fDate == dStr) { matchDate = true; break; }
            }
            if (!matchDate) return false;
        }

        return true;
    }

    bool lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const override {
        // 核心红线：置顶优先规则
        bool leftPinned = source_left.data(IsLockedRole).toBool();
        bool rightPinned = source_right.data(IsLockedRole).toBool();

        if (leftPinned != rightPinned) {
            // 在 lessThan(left, right) 中，返回 true 表示 left < right (升序时 left 排在 right 之前)
            // 如果我们想要 Pinned 的永远在最前面，无论升序降序，我们需要特殊处理。
            // 但 Qt 的排序是：lessThan 结果决定相对顺序，升序直接用 lessThan，降序取反。
            // 因此，为了实现“置顶永远在前”，必须根据 sortOrder 动态返回。

            if (sortOrder() == Qt::AscendingOrder)
                return leftPinned; // 升序时，Pinned 为 "小"，排在前
            else
                return !leftPinned; // 降序时，Pinned 为 "大"，排在前
        }

        // 如果置顶状态相同，则走默认逻辑
        return QSortFilterProxyModel::lessThan(source_left, source_right);
    }
};


ContentPanel::ContentPanel(QWidget* parent)
    : QWidget(parent) {
    setMinimumWidth(200);
    setStyleSheet("QWidget { background-color: #1A1A1A; color: #EEEEEE; border: none; }");

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_model = new QStandardItemModel(this);
    m_proxyModel = new FilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);

    m_zoomLevel = 96;

    initUi();
}

void ContentPanel::initUi() {
    QWidget* titleBar = new QWidget(this);
    titleBar->setStyleSheet("background: #252526; border-bottom: 1px solid #333;");
    titleBar->setFixedHeight(38);
    QHBoxLayout* titleL = new QHBoxLayout(titleBar);
    titleL->setContentsMargins(12, 0, 12, 0);

    QLabel* titleLabel = new QLabel("内容（文件夹 / 文件）", titleBar);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #4a90e2; border: none;");

    QPushButton* btnLayers = new QPushButton(titleBar);
    btnLayers->setFixedSize(24, 24);
    btnLayers->setIcon(UiHelper::getIcon("layers", QColor("#B0B0B0"), 18));
    btnLayers->setToolTip("递归显示子目录所有文件");
    btnLayers->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 4px; }"
        "QPushButton:hover { background: rgba(255, 255, 255, 0.1); }"
        "QPushButton:pressed { background: rgba(255, 255, 255, 0.2); }"
    );
    connect(btnLayers, &QPushButton::clicked, [this]() {
        if (!m_currentPath.isEmpty() && m_currentPath != "computer://") {
            loadDirectory(m_currentPath, true);
        }
    });

    titleL->addWidget(titleLabel);
    titleL->addStretch();
    titleL->addWidget(btnLayers);

    m_mainLayout->addWidget(titleBar);

    m_viewStack = new QStackedWidget(this);
    
    initGridView();
    initListView();

    m_viewStack->addWidget(m_gridView);
    m_viewStack->addWidget(m_treeView);
    m_viewStack->setCurrentWidget(m_gridView);

    QVBoxLayout* contentWrapper = new QVBoxLayout();
    contentWrapper->setContentsMargins(10, 10, 10, 10);
    contentWrapper->setSpacing(0);
    contentWrapper->addWidget(m_viewStack);
    
    m_mainLayout->addLayout(contentWrapper);
    
    // 快捷键拦截提升至控件本身，而非 viewport()，确保焦点行为一致
    m_gridView->installEventFilter(this);
}

void ContentPanel::updateGridSize() {
    m_zoomLevel = qBound(32, m_zoomLevel, 128);
    m_gridView->setIconSize(QSize(m_zoomLevel, m_zoomLevel));
    
    int cardW = m_zoomLevel + 30; // 用户指定: zoomLevel+30
    int cardH = m_zoomLevel + 60; // 遵循规范: zoomLevel+60
    m_gridView->setGridSize(QSize(cardW, cardH));
}

bool ContentPanel::eventFilter(QObject* obj, QEvent* event) {
    if ((obj == m_gridView || obj == m_gridView->viewport()) && event->type() == QEvent::Wheel) {
        QWheelEvent* wEvent = static_cast<QWheelEvent*>(event);
        if (wEvent->modifiers() & Qt::ControlModifier) {
            int delta = wEvent->angleDelta().y();
            if (delta > 0) m_zoomLevel += 8;
            else m_zoomLevel -= 8;
            updateGridSize();
            return true;
        }
    }

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        QAbstractItemView* view = qobject_cast<QAbstractItemView*>(obj);
        if (!view) view = qobject_cast<QAbstractItemView*>(obj->parent());

        // 核心红线：如果焦点在 QLineEdit 上，说明正在重命名，直接放行
        if (qobject_cast<QLineEdit*>(QApplication::focusWidget())) {
            return false;
        }

        if (view) {
            // 1. Ctrl + 0..5 (星级)
            if ((keyEvent->modifiers() & Qt::ControlModifier) && 
                (keyEvent->key() >= Qt::Key_0 && keyEvent->key() <= Qt::Key_5)) {
                
                int rating = keyEvent->key() - Qt::Key_0;
                auto indexes = view->selectionModel()->selectedIndexes();
                for (const auto& idx : indexes) {
                    if (idx.column() == 0) {
                        m_proxyModel->setData(idx, rating, RatingRole);
                        QString path = idx.data(PathRole).toString();
                        if (!path.isEmpty()) {
                            QFileInfo info(path);
                            AmMetaJson meta(info.absolutePath().toStdWString());
                            meta.load();
                            meta.items()[info.fileName().toStdWString()].rating = rating;
                            meta.save();
                        }
                    }
                }
                return true;
            }

            // 2. Alt + D (置顶/取消置顶)
            if (((keyEvent->modifiers() & Qt::AltModifier) || (keyEvent->modifiers() & (Qt::AltModifier | Qt::WindowShortcut))) && 
                (keyEvent->key() == Qt::Key_D)) {
                auto indexes = view->selectionModel()->selectedIndexes();
                for (const QModelIndex& idx : indexes) {
                    if (idx.column() == 0) {
                        QString itemPath = idx.data(PathRole).toString();
                        if (!itemPath.isEmpty()) {
                            QFileInfo info(itemPath);
                            ArcMeta::AmMetaJson meta(info.absolutePath().toStdWString());
                            meta.load();
                            bool current = meta.items()[info.fileName().toStdWString()].pinned;
                            meta.items()[info.fileName().toStdWString()].pinned = !current;
                            meta.save();
                            // 同步模型显示 (IsLockedRole)
                            m_proxyModel->setData(idx, !current, IsLockedRole);
                        }
                    }
                }
                return true;
            }

            // 3. Alt + 1..9 (颜色打标)
            if ((keyEvent->modifiers() & Qt::AltModifier) && 
                (keyEvent->key() >= Qt::Key_1 && keyEvent->key() <= Qt::Key_9)) {
                
                QString color;
                switch (keyEvent->key()) {
                    case Qt::Key_1: color = "red"; break;
                    case Qt::Key_2: color = "orange"; break;
                    case Qt::Key_3: color = "yellow"; break;
                    case Qt::Key_4: color = "green"; break;
                    case Qt::Key_5: color = "cyan"; break;
                    case Qt::Key_6: color = "blue"; break;
                    case Qt::Key_7: color = "purple"; break;
                    case Qt::Key_8: color = "gray"; break;
                    case Qt::Key_9: color = ""; break;
                }

                auto indexes = view->selectionModel()->selectedIndexes();
                for (const auto& idx : indexes) {
                    if (idx.column() == 0) {
                        m_proxyModel->setData(idx, color, ColorRole);
                        QString path = idx.data(PathRole).toString();
                        if (!path.isEmpty()) {
                            QFileInfo info(path);
                            AmMetaJson meta(info.absolutePath().toStdWString());
                            meta.load();
                            meta.items()[info.fileName().toStdWString()].color = color.toStdWString();
                            meta.save();
                        }
                    }
                }
                return true;
            }

            if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
                if (keyEvent->key() == Qt::Key_C) {
                    QStringList paths;
                    auto indexes = view->selectionModel()->selectedIndexes();
                    for (const auto& idx : indexes) if (idx.column() == 0) paths << QDir::toNativeSeparators(idx.data(PathRole).toString());
                    if (!paths.isEmpty()) QApplication::clipboard()->setText(paths.join("\r\n"));
                    return true;
                }
                if (keyEvent->key() == Qt::Key_R) {
                    // 批量重命名 (Ctrl+Shift+R) 被触发
                    return true;
                }
            }

            if (keyEvent->key() == Qt::Key_F2) {
                view->edit(view->currentIndex());
                return true;
            }
            if (keyEvent->key() == Qt::Key_Delete) {
                onCustomContextMenuRequested(view->mapFromGlobal(QCursor::pos()));
                return true;
            }
            
            if (keyEvent->modifiers() & Qt::ControlModifier) {
                if (keyEvent->key() == Qt::Key_C && !(keyEvent->modifiers() & Qt::ShiftModifier)) {
                    QList<QUrl> urls;
                    auto indexes = view->selectionModel()->selectedIndexes();
                    for (const auto& idx : indexes) if (idx.column() == 0) urls << QUrl::fromLocalFile(idx.data(PathRole).toString());
                    if (!urls.isEmpty()) {
                        QMimeData* mime = new QMimeData();
                        mime->setUrls(urls);
                        QApplication::clipboard()->setMimeData(mime);
                    }
                    return true;
                }
                if (keyEvent->key() == Qt::Key_V) {
                    const QMimeData* mime = QApplication::clipboard()->mimeData();
                    if (mime && mime->hasUrls()) {
                        QList<QUrl> urls = mime->urls();
                        std::wstring fromPaths;
                        for (const QUrl& url : urls) {
                            fromPaths += QDir::toNativeSeparators(url.toLocalFile()).toStdWString() + L'\0';
                        }
                        if (!fromPaths.empty()) {
                            fromPaths += L'\0';
                            std::wstring toPath = m_currentPath.toStdWString() + L'\0' + L'\0';
                            SHFILEOPSTRUCTW fileOp = { 0 };
                            fileOp.wFunc = FO_COPY;
                            fileOp.pFrom = fromPaths.c_str();
                            fileOp.pTo = toPath.c_str();
                            fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
                            if (SHFileOperationW(&fileOp) == 0) loadDirectory(m_currentPath);
                        }
                    }
                    return true;
                }
            }

            if (keyEvent->key() == Qt::Key_Space) {
                QModelIndex idx = view->currentIndex();
                if (idx.isValid()) emit requestQuickLook(idx.data(PathRole).toString());
                return true;
            }
            if (keyEvent->key() == Qt::Key_Backspace) {
                QDir dir(m_currentPath);
                if (dir.cdUp()) emit directorySelected(dir.absolutePath());
                return true;
            }
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                onDoubleClicked(view->currentIndex());
                return true;
            }
            if (keyEvent->modifiers() & Qt::ControlModifier && keyEvent->key() == Qt::Key_Backslash) {
                setViewMode(m_viewStack->currentIndex() == 0 ? ListView : GridView);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ContentPanel::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        int delta = event->angleDelta().y();
        if (delta > 0) m_zoomLevel += 8;
        else m_zoomLevel -= 8;
        updateGridSize();
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

void ContentPanel::setViewMode(ViewMode mode) {
    if (mode == GridView) m_viewStack->setCurrentWidget(m_gridView);
    else m_viewStack->setCurrentWidget(m_treeView);
}

void ContentPanel::initGridView() {
    m_gridView = new QListView(this);
    m_gridView->setViewMode(QListView::IconMode);
    m_gridView->setMovement(QListView::Static);
    m_gridView->setSpacing(8);
    m_gridView->setResizeMode(QListView::Adjust);
    m_gridView->setWrapping(true);
    m_gridView->setIconSize(QSize(96, 96));
    m_gridView->setGridSize(QSize(126, 156)); // 遵循规范: W=96+30, H=96+60
    m_gridView->setSpacing(10);
    m_gridView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_gridView->setContextMenuPolicy(Qt::CustomContextMenu);

    // 禁用双击编辑，将双击权归还给“打开”操作
    m_gridView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);

    m_gridView->setModel(m_proxyModel);
    m_gridView->setItemDelegate(new GridItemDelegate(this));
    m_gridView->viewport()->installEventFilter(this);

    // 显式连接双击打开信号
    connect(m_gridView, &QListView::doubleClicked, this, &ContentPanel::onDoubleClicked);

    m_gridView->setStyleSheet(
        "QListView { background-color: transparent; border: none; outline: none; }"
        "QListView::item { background: transparent; }"
        "QListView::item:selected { background-color: rgba(55, 138, 221, 0.2); border-radius: 4px; }"
        "QListView QLineEdit { background-color: #2D2D2D; color: #FFFFFF; border: 1px solid #378ADD; border-radius: 2px; padding: 2px; selection-background-color: #378ADD; selection-color: #FFFFFF; }"
    );

    connect(m_gridView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ContentPanel::onSelectionChanged);
    connect(m_gridView, &QListView::customContextMenuRequested, this, &ContentPanel::onCustomContextMenuRequested);
}

void ContentPanel::initListView() {
    m_treeView = new QTreeView(this);
    m_treeView->setSortingEnabled(true);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setExpandsOnDoubleClick(false);
    m_treeView->setRootIsDecorated(false);

    m_treeView->setModel(m_proxyModel);
    m_treeView->viewport()->installEventFilter(this);

    m_treeView->setStyleSheet(
        "QTreeView { background-color: transparent; border: none; outline: none; font-size: 12px; }"
        "QTreeView::item { height: 28px; color: #EEEEEE; padding-left: 4px; }"
        "QTreeView::item:selected { background-color: #378ADD; }"
        "QTreeView::item:hover { background-color: rgba(255, 255, 255, 0.05); }"
        "QTreeView QLineEdit { background-color: #2D2D2D; color: #FFFFFF; border: 1px solid #378ADD; border-radius: 2px; padding: 2px; selection-background-color: #378ADD; selection-color: #FFFFFF; }"
    );

    m_treeView->header()->setStyleSheet(
        "QHeaderView::section { background-color: #252525; color: #B0B0B0; padding-left: 10px; border: none; height: 32px; font-size: 11px; }"
    );

    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ContentPanel::onSelectionChanged);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &ContentPanel::onCustomContextMenuRequested);
    connect(m_treeView, &QTreeView::doubleClicked, this, &ContentPanel::onDoubleClicked);
}

void ContentPanel::onCustomContextMenuRequested(const QPoint& pos) {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #2B2B2B; border: 1px solid #444444; color: #EEEEEE; padding: 4px; border-radius: 4px; }"
        "QMenu::item { height: 22px; padding: 0 10px 0 10px; border-radius: 3px; font-size: 12px; }"
        "QMenu::item:selected { background-color: #378ADD; }"
        "QMenu::separator { height: 1px; background: #444444; margin: 4px 8px 4px 8px; }"
    );

    menu.addAction("打开");
    menu.addAction("用系统默认程序打开");
    menu.addAction("在资源管理器中显示");
    menu.addSeparator();

    QMenu* newMenu = menu.addMenu("新建...");
    newMenu->setIcon(UiHelper::getIcon("ruler_spacing", QColor("#EEEEEE")));
    QAction* actNewFolder = newMenu->addAction(UiHelper::getIcon("folder", QColor("#EEEEEE")), "创建文件夹");
    QAction* actNewMd     = newMenu->addAction(UiHelper::getIcon("text", QColor("#EEEEEE")), "创建 Markdown");
    QAction* actNewTxt    = newMenu->addAction(UiHelper::getIcon("text", QColor("#EEEEEE")), "创建纯文本文件 (txt)");
    menu.addSeparator();
    
    QMenu* categorizeMenu = menu.addMenu("归类到...");
    categorizeMenu->addAction("（暂无分类）"); // 占位，待后续对接数据库加载
    
    menu.addSeparator();
    
    menu.addAction("置顶 / 取消置顶");
    QMenu* cryptoMenu = menu.addMenu("加密保护 / 解除加密 / 修改密码");
    cryptoMenu->addAction("加密保护");
    cryptoMenu->addAction("解除加密");
    cryptoMenu->addAction("修改密码");
    
    menu.addSeparator();
    menu.addAction("批量重命名 (Ctrl+Shift+R)");
    menu.addSeparator();
    
    menu.addAction("重命名");
    menu.addAction("复制");
    menu.addAction("剪切");
    menu.addAction("粘贴");
    menu.addAction("删除（移入回收站）");
    menu.addSeparator();
    
    menu.addAction("复制路径");
    menu.addAction("属性");

    QAbstractItemView* view = qobject_cast<QAbstractItemView*>(sender());
    if (!view) return;
    
    QAction* selectedAction = menu.exec(view->viewport()->mapToGlobal(pos));
    if (!selectedAction) return;

    QString actionText = selectedAction->text();
    QModelIndex currentIndex = view->indexAt(pos);
    QString path = currentIndex.data(PathRole).toString();

    if (actionText == "在资源管理器中显示") {
        QStringList args;
        args << "/select," << QDir::toNativeSeparators(path);
        QProcess::startDetached("explorer", args);
    } else if (selectedAction == actNewFolder) {
        createNewItem("folder");
    } else if (selectedAction == actNewMd) {
        createNewItem("md");
    } else if (selectedAction == actNewTxt) {
        createNewItem("txt");
    } else if (actionText == "复制路径") {
        QApplication::clipboard()->setText(QDir::toNativeSeparators(path));
    } else if (actionText == "重命名") {
        view->edit(currentIndex);
    } else if (actionText.startsWith("删除")) {
        std::wstring wpath = path.toStdWString() + L'\0' + L'\0';
        SHFILEOPSTRUCTW fileOp = { 0 };
        fileOp.wFunc = FO_DELETE;
        fileOp.pFrom = wpath.c_str();
        fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
        if (SHFileOperationW(&fileOp) == 0) loadDirectory(m_currentPath);
    } else if (actionText == "置顶 / 取消置顶") {
        auto indexes = view->selectionModel()->selectedIndexes();
        for (const QModelIndex& idx : indexes) {
            if (idx.column() == 0) {
                QString itemPath = idx.data(PathRole).toString();
                if (!itemPath.isEmpty()) {
                    QFileInfo info(itemPath);
                    ArcMeta::AmMetaJson meta(info.absolutePath().toStdWString());
                    meta.load();
                    bool current = meta.items()[info.fileName().toStdWString()].pinned;
                    meta.items()[info.fileName().toStdWString()].pinned = !current;
                    meta.save();
                }
            }
        }
    } else if (actionText == "属性") {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_INVOKEIDLIST;
        sei.lpVerb = L"properties";
        std::wstring wpath = path.toStdWString();
        sei.lpFile = wpath.c_str();
        sei.nShow = SW_SHOW;
        ShellExecuteExW(&sei);
    } else if (actionText == "打开" || actionText == "用系统默认程序打开") {
        onDoubleClicked(currentIndex);
    }
}

void ContentPanel::onSelectionChanged() {
    QItemSelectionModel* selectionModel = (m_viewStack->currentWidget() == m_gridView) ? m_gridView->selectionModel() : m_treeView->selectionModel();
    if (!selectionModel) return;

    QStringList selectedPaths;
    QModelIndexList indices = selectionModel->selectedIndexes();
    for (const QModelIndex& index : indices) {
        if (index.column() == 0) {
            QString path = index.data(PathRole).toString();
            if (!path.isEmpty()) selectedPaths.append(path);
        }
    }
    emit selectionChanged(selectedPaths);
}

void ContentPanel::onDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) return;
    QString path = index.data(PathRole).toString();
    if (path.isEmpty()) return;

    QFileInfo info(path);
    if (info.isDir()) {
        emit directorySelected(path); 
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void ContentPanel::loadDirectory(const QString& path, bool recursive) {
    m_model->clear();
    m_model->setHorizontalHeaderLabels({"名称", "大小", "类型", "修改时间"});

    // 空路径 → 展示此电脑（所有驱动器）
    if (path.isEmpty() || path == "computer://") {
        m_currentPath = "computer://";
        QFileIconProvider iconProvider;
        const auto drives = QDir::drives();
        for (const QFileInfo& drive : drives) {
            auto* item = new QStandardItem(iconProvider.icon(drive), drive.absolutePath());
            item->setData(drive.absolutePath(), PathRole);
            item->setData("folder", TypeRole);
            QList<QStandardItem*> row;
            row << item;
            row << new QStandardItem("-");
            row << new QStandardItem("磁盘分区");
            row << new QStandardItem("-");
            m_model->appendRow(row);
        }
        // 清除筛选统计
        emit directoryStatsReady({}, {}, {}, {}, {}, {});
        return;
    }

    m_currentPath = path;

    QMap<int, int>     ratingCounts;
    QMap<QString, int> colorCounts;
    QMap<QString, int> tagCounts;
    QMap<QString, int> typeCounts;
    QMap<QString, int> createDateCounts;
    QMap<QString, int> modifyDateCounts;
    int noTagCount = 0;

    addItemsFromDirectory(path, recursive, ratingCounts, colorCounts, tagCounts, typeCounts, createDateCounts, modifyDateCounts, noTagCount);

    applyFilters();

    if (noTagCount > 0) tagCounts["__none__"] = noTagCount;
    emit directoryStatsReady(ratingCounts, colorCounts, tagCounts, typeCounts,
                              createDateCounts, modifyDateCounts);
}

void ContentPanel::addItemsFromDirectory(const QString& path, bool recursive,
                                       QMap<int, int>& ratingCounts,
                                       QMap<QString, int>& colorCounts,
                                       QMap<QString, int>& tagCounts,
                                       QMap<QString, int>& typeCounts,
                                       QMap<QString, int>& createDateCounts,
                                       QMap<QString, int>& modifyDateCounts,
                                       int& noTagCount)
{
    QDir dir(path);
    if (!dir.exists()) return;

    QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
    QFileIconProvider iconProvider;

    // 预读当前目录的元数据配置图谱
    ArcMeta::AmMetaJson meta(path.toStdWString());
    meta.load();
    const auto& metaItems = meta.items();

    QDate today    = QDate::currentDate();
    QDate yesterday = today.addDays(-1);

    auto dateKey = [&](const QDate& d) -> QString {
        if (d == today)     return "today";
        if (d == yesterday) return "yesterday";
        return d.toString("yyyy-MM-dd");
    };

    for (const QFileInfo& info : entries) {
        QString fileName = info.fileName();
        if (fileName == ".am_meta.json" || fileName == ".am_meta.json.tmp") continue;

        QString fullPath = info.absoluteFilePath();
        QList<QStandardItem*> row;
        auto* nameItem = new QStandardItem(iconProvider.icon(info), fileName);
        nameItem->setData(fullPath, PathRole);
        nameItem->setData(info.isDir() ? "folder" : "file", TypeRole);

        // 元数据反填 + 统计
        int     itemRating = 0;
        QString itemColor;
        bool    hasTags = false;
        QStringList itemTags;

        auto it = metaItems.find(fileName.toStdWString());
        if (it != metaItems.end()) {
            itemRating = it->second.rating;
            itemColor  = QString::fromStdWString(it->second.color);
            nameItem->setData(itemRating, RatingRole);
            nameItem->setData(itemColor, ColorRole);
            nameItem->setData(it->second.pinned, IsLockedRole);
            hasTags = !it->second.tags.empty();
            for (const auto& tag : it->second.tags) {
                QString t = QString::fromStdWString(tag);
                itemTags << t;
                tagCounts[t]++;
            }
            nameItem->setData(itemTags, TagsRole);
        }

        // 评级统计
        ratingCounts[itemRating]++;
        // 颜色统计
        colorCounts[itemColor]++;
        // 无标签计数
        if (!hasTags) noTagCount++;
        // 文件类型统计
        typeCounts[info.isDir() ? "folder" : info.suffix().toUpper()]++;
        // 创建日期统计
        createDateCounts[dateKey(info.birthTime().date())]++;
        // 修改日期统计
        modifyDateCounts[dateKey(info.lastModified().date())]++;

        row << nameItem;
        row << new QStandardItem(info.isDir() ? "-" : QString::number(info.size() / 1024) + " KB");
        row << new QStandardItem(info.isDir() ? "文件夹" : info.suffix().toUpper() + " 文件");
        row << new QStandardItem(info.lastModified().toString("yyyy-MM-dd HH:mm"));
        m_model->appendRow(row);

        if (recursive && info.isDir()) {
            addItemsFromDirectory(fullPath, true, ratingCounts, colorCounts, tagCounts, typeCounts, createDateCounts, modifyDateCounts, noTagCount);
        }
    }
}



void ContentPanel::search(const QString& query) {
    if (query.isEmpty()) return;
    m_model->clear();
    m_model->setHorizontalHeaderLabels({"名称", "路径", "类型", "修改时间"});

    auto results = MftReader::instance().search(query.toStdWString());
    QFileIconProvider iconProvider;

    QMap<int, int>     ratingCounts;
    QMap<QString, int> colorCounts;
    QMap<QString, int> tagCounts;
    QMap<QString, int> typeCounts;
    QMap<QString, int> createDateCounts;
    QMap<QString, int> modifyDateCounts;
    int noTagCount = 0;

    QDate today = QDate::currentDate();
    QDate yesterday = today.addDays(-1);
    auto dateKey = [&](const QDate& d) -> QString {
        if (d == today)     return "today";
        if (d == yesterday) return "yesterday";
        return d.toString("yyyy-MM-dd");
    };

    int count = 0;
    for (const auto& entry : results) {
        if (++count > 1000) break;

        QString fileName = QString::fromStdWString(entry.name);
        if (fileName == ".am_meta.json" || fileName == ".am_meta.json.tmp") continue;

        std::wstring fullWPath = PathBuilder::getPath(entry.volume, entry.frn);
        QString fullPath = QString::fromStdWString(fullWPath);
        if (fullPath.isEmpty()) {
            fullPath = QString::fromStdWString(entry.volume) + "[FRN:" + QString::number(entry.frn, 16) + "]";
        }

        QFileInfo info(fullPath);
        QList<QStandardItem*> row;
        auto* nameItem = new QStandardItem(iconProvider.icon(info), fileName);
        nameItem->setData(fullPath, PathRole); 
        nameItem->setData(entry.isDir() ? "folder" : "file", TypeRole);
        
        int     itemRating = 0;
        QString itemColor;
        bool    hasTags = false;

        // --- 搜索模式下单体探针：由于不在同一目录，需单体读取所属父目录元数据 ---
        QString parentDir = info.absolutePath();
        ArcMeta::AmMetaJson meta(parentDir.toStdWString());
        meta.load();
        auto it = meta.items().find(fileName.toStdWString());
        if (it != meta.items().end()) {
            itemRating = it->second.rating;
            itemColor  = QString::fromStdWString(it->second.color);
            nameItem->setData(itemRating, RatingRole);
            nameItem->setData(itemColor, ColorRole);
            nameItem->setData(it->second.pinned, IsLockedRole);

            hasTags = !it->second.tags.empty();
            QStringList tgs;
            for (const auto& t : it->second.tags) {
                QString ts = QString::fromStdWString(t);
                tgs << ts;
                tagCounts[ts]++;
            }
            nameItem->setData(tgs, TagsRole);
        }

        // 统计累加
        ratingCounts[itemRating]++;
        colorCounts[itemColor]++;
        if (!hasTags) noTagCount++;
        typeCounts[entry.isDir() ? "folder" : info.suffix().toUpper()]++;
        createDateCounts[dateKey(info.birthTime().date())]++;
        modifyDateCounts[dateKey(info.lastModified().date())]++;

        row << nameItem;
        row << new QStandardItem(fullPath);
        row << new QStandardItem(entry.isDir() ? "文件夹" : (info.suffix().isEmpty() ? "文件" : info.suffix().toUpper() + " 文件"));
        row << new QStandardItem(info.lastModified().toString("yyyy-MM-dd HH:mm"));

        m_model->appendRow(row);
    }

    if (noTagCount > 0) tagCounts["__none__"] = noTagCount;
    emit directoryStatsReady(ratingCounts, colorCounts, tagCounts, typeCounts,
                              createDateCounts, modifyDateCounts);
}

void ContentPanel::applyFilters(const FilterState& state) {
    m_currentFilter = state;
    applyFilters();
}

void ContentPanel::applyFilters() {
    auto* proxy = static_cast<FilterProxyModel*>(m_proxyModel);
    proxy->currentFilter = m_currentFilter;
    proxy->updateFilter();
}

void ContentPanel::createNewItem(const QString& type) {
    if (m_currentPath.isEmpty() || m_currentPath == "computer://") return;

    QString baseName = (type == "folder") ? "新建文件夹" : "未命名";
    QString ext = (type == "md") ? ".md" : ((type == "txt") ? ".txt" : "");
    QString finalName = baseName + ext;
    QString fullPath = m_currentPath + "/" + finalName;

    // 自动避重
    int counter = 1;
    while (QFileInfo::exists(fullPath)) {
        finalName = baseName + QString(" (%1)").arg(counter++) + ext;
        fullPath = m_currentPath + "/" + finalName;
    }

    bool success = false;
    if (type == "folder") {
        success = QDir(m_currentPath).mkdir(finalName);
    } else {
        QFile file(fullPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.close();
            success = true;
        }
    }

    if (success) {
        loadDirectory(m_currentPath);
        // 自动进入重命名模式（查找对应项并激活编辑）
        auto results = m_model->findItems(finalName, Qt::MatchExactly, 0);
        if (!results.isEmpty()) {
            QModelIndex srcIdx = results.first()->index();
            QModelIndex proxyIdx = m_proxyModel->mapFromSource(srcIdx);
            if (proxyIdx.isValid()) {
                m_gridView->setCurrentIndex(proxyIdx);
                m_gridView->edit(proxyIdx);
            }
        }
    }
}

// --- Delegate ---

void GridItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QRect cardRect = option.rect.adjusted(4, 4, -4, -4);
    bool isSelected = (option.state & QStyle::State_Selected);
    bool isHovered = (option.state & QStyle::State_MouseOver);
    
    QColor cardBg = isSelected ? QColor("#282828") : (isHovered ? QColor("#2A2A2A") : QColor("#2D2D2D"));
    painter->setPen(isSelected ? QPen(QColor("#3498db"), 2) : QPen(QColor("#333333"), 1));
    painter->setBrush(cardBg);
    painter->drawRoundedRect(cardRect, 8, 8);

    QString path = index.data(PathRole).toString();
    QFileInfo info(path);
    QString ext = info.isDir() ? "DIR" : info.suffix().toUpper();
    if (ext.isEmpty()) ext = "FILE";
    QColor badgeColor = UiHelper::getExtensionColor(ext);

    QRect extRect(cardRect.left() + 8, cardRect.top() + 8, 36, 18);
    painter->setPen(Qt::NoPen);
    painter->setBrush(badgeColor);
    painter->drawRoundedRect(extRect, 4, 4);
    painter->setPen(QColor("#FFFFFF"));
    QFont extFont = painter->font();
    extFont.setPointSize(8);
    extFont.setBold(true);
    painter->setFont(extFont);
    painter->drawText(extRect, Qt::AlignCenter, ext);

    int baseIconSize = option.decorationSize.width();
    if (baseIconSize <= 0) baseIconSize = 64; 
    int iconDrawSize = static_cast<int>(baseIconSize * 0.65);
    
    int ratingH = 12;
    int nameH = 16;
    int gap1 = 6;
    int gap2 = 4;
    
    int totalH = iconDrawSize + gap1 + ratingH + gap2 + nameH;
    int startY = cardRect.top() + (cardRect.height() - totalH) / 2 + 13; // +5 -> +13 (下移 8px)

    QRect iconRect(cardRect.left() + (cardRect.width() - iconDrawSize) / 2, startY, iconDrawSize, iconDrawSize);
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    icon.paint(painter, iconRect);

    int ratingY = iconRect.bottom() + gap1;
    int rating = index.data(RatingRole).toInt();
    
    int starSize = 12; 
    int starSpacing = 1; // 还原间距，不再脑补加宽
    int banW = 12;
    int banGap = 4;
    int infoTotalW = banW + banGap + (5 * starSize) + (4 * starSpacing);
    int infoStartX = cardRect.left() + (cardRect.width() - infoTotalW) / 2;

    // A. 绘制“清除星级”图标 (SVG)
    QRect banRect(infoStartX, ratingY + (ratingH - banW) / 2, banW, banW);
    QIcon banIcon = UiHelper::getIcon("no_color", QColor("#555555"), banW);
    banIcon.paint(painter, banRect);

    // B. 绘制星级 (SVG)
    int starsStartX = infoStartX + banW + banGap;
    QPixmap filledStar = UiHelper::getPixmap("star_filled", QSize(starSize, starSize), QColor("#EF9F27"));
    QPixmap emptyStar = UiHelper::getPixmap("star", QSize(starSize, starSize), QColor("#444444"));

    for (int i = 0; i < 5; ++i) {
        QRect starRect(starsStartX + i * (starSize + starSpacing), ratingY + (ratingH - starSize) / 2, starSize, starSize);
        painter->drawPixmap(starRect, (i < rating) ? filledStar : emptyStar);
    }

    int nameY = ratingY + ratingH + gap2;
    QRect nameRect(cardRect.left() + 6, nameY, cardRect.width() - 12, nameH);
    
    QString colorName = index.data(ColorRole).toString();
    if (!colorName.isEmpty()) {
        QColor dotC(colorName);
        if (!dotC.isValid()) {
            if (colorName.contains("red") || colorName.contains("红")) dotC = QColor("#E24B4A");
            else if (colorName.contains("orange") || colorName.contains("橙")) dotC = QColor("#EF9F27");
            else if (colorName.contains("green") || colorName.contains("绿")) dotC = QColor("#639922");
            else if (colorName.contains("blue") || colorName.contains("蓝")) dotC = QColor("#378ADD");
        }
        if (dotC.isValid()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(dotC);
            painter->drawRoundedRect(nameRect, 4, 4);
            painter->setPen(dotC.lightness() > 180 ? Qt::black : Qt::white);
        } else {
            painter->setPen(QColor("#CCCCCC"));
        }
    } else {
        painter->setPen(QColor("#CCCCCC"));
    }

    QString name = index.data(Qt::DisplayRole).toString();
    QFont textFont = painter->font();
    textFont.setPointSize(8);
    textFont.setBold(false);
    painter->setFont(textFont);
    painter->drawText(nameRect.adjusted(4, 0, -4, 0), Qt::AlignCenter | Qt::ElideRight, name);

    painter->restore();
}

QSize GridItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const {
    auto* view = qobject_cast<const QListView*>(option.widget);
    if (view && view->gridSize().isValid()) return view->gridSize();
    return QSize(96, 112);
}

bool GridItemDelegate::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        QLineEdit* editor = qobject_cast<QLineEdit*>(obj);
        if (editor) {
            // 修复重命名：允许方向键、Home/End 在编辑框内正常导航，不触发视图切换
            switch (keyEvent->key()) {
                case Qt::Key_Left:
                case Qt::Key_Right:
                case Qt::Key_Up:
                case Qt::Key_Down:
                case Qt::Key_Home:
                case Qt::Key_End:
                    // 返回 false 表示不拦截，让 QLineEdit 自身处理
                    // 同时停止事件传播，防止 QAbstractItemView 捕获并结束编辑
                    keyEvent->accept();
                    return false;
                default:
                    break;
            }
        }
    }
    return QStyledItemDelegate::eventFilter(obj, event);
}

bool GridItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mEvent = static_cast<QMouseEvent*>(event);
        if (mEvent->button() == Qt::LeftButton) {
            int baseIconSize = option.decorationSize.width();
            if (baseIconSize <= 0) baseIconSize = 64; 
            int iconDrawSize = static_cast<int>(baseIconSize * 0.65); 
            int ratingH = 12;
            int nameH = 16;
            int gap1 = 6;
            int gap2 = 4;
            int totalH = iconDrawSize + gap1 + ratingH + gap2 + nameH;
            int startY = option.rect.top() + (option.rect.height() - totalH) / 2 + 13;
            int ratingY = startY + iconDrawSize + gap1;
            int starSize = 12; 
            int starSpacing = 1; 
            int banW = 12;
            int banGap = 4;
            int infoTotalW = banW + banGap + (5 * starSize + 4 * starSpacing);
            int infoStartX = option.rect.left() + (option.rect.width() - infoTotalW) / 2;

            // 1. 判定“清除”图标
            QRect banRect(infoStartX, ratingY + (ratingH - 14) / 2, 14, 14);
            if (banRect.contains(mEvent->pos())) {
                // 此处的 model 已经是 View 关联的 ProxyModel
                model->setData(index, 0, RatingRole);
                QString path = index.data(PathRole).toString();
                if (!path.isEmpty()) {
                    QFileInfo info(path);
                    AmMetaJson meta(info.absolutePath().toStdWString());
                    meta.load();
                    meta.items()[info.fileName().toStdWString()].rating = 0;
                    meta.save();
                }
                return true;
            }

            // 2. 判定 5 颗星星 (1..5)
            int starsStartX = infoStartX + banW + banGap;
            for (int i = 0; i < 5; ++i) {
                QRect starRect(starsStartX + i * (starSize + starSpacing), ratingY + (ratingH - starSize) / 2, starSize, starSize);
                if (starRect.contains(mEvent->pos())) {
                    int r = i + 1;
                    model->setData(index, r, RatingRole);
                    QString path = index.data(PathRole).toString();
                    if (!path.isEmpty()) {
                        QFileInfo info(path);
                        AmMetaJson meta(info.absolutePath().toStdWString());
                        meta.load();
                        meta.items()[info.fileName().toStdWString()].rating = r;
                        meta.save();
                    }
                    return true;
                }
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QWidget* GridItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(option);
    QLineEdit* editor = new QLineEdit(parent);

    // 关键点：重命名时光标行为修复
    // 默认情况下 QAbstractItemView 会在编辑状态下拦截方向键
    // 我们通过设置属性让编辑器能处理方向键
    editor->installEventFilter(const_cast<GridItemDelegate*>(this));

    editor->setAlignment(Qt::AlignCenter);
    editor->setFrame(false);
    
    // 背景色动态匹配标签色，若无标签则用默认深色
    QString tagColorStr = index.data(ColorRole).toString();
    QString bgColor = tagColorStr.isEmpty() ? "#3E3E42" : tagColorStr;
    QString textColor = tagColorStr.isEmpty() ? "#FFFFFF" : "#000000";

    editor->setStyleSheet(
        QString("QLineEdit { background-color: %1; color: %2; border-radius: 4px; "
                "border: 2px solid #3498db; font-weight: bold; font-size: 8pt; padding: 0px; }")
        .arg(bgColor, textColor)
    );
    return editor;
}

void GridItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    QString value = index.model()->data(index, Qt::EditRole).toString();
    QLineEdit* lineEdit = static_cast<QLineEdit*>(editor);
    lineEdit->setText(value);
    
    // 智能选中：选中不含扩展名的部分
    int lastDot = value.lastIndexOf('.');
    if (lastDot > 0) {
        lineEdit->setSelection(0, lastDot);
    } else {
        lineEdit->selectAll();
    }
}

void GridItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    QLineEdit* lineEdit = static_cast<QLineEdit*>(editor);
    QString value = lineEdit->text();
    if(value.isEmpty() || value == index.data(Qt::DisplayRole).toString()) return;

    QString oldPath = index.data(PathRole).toString();
    QFileInfo info(oldPath);
    QString newPath = info.absolutePath() + "/" + value;
    
    if (QFile::rename(oldPath, newPath)) {
        model->setData(index, value, Qt::EditRole);
        model->setData(index, newPath, PathRole);
        // 如果有独立的 MetaJson 辅助类，此处由于是在 ContentPanel.cpp，
        // 我们留给外部依赖扫描器去刷新，或只在这里进行纯物理重命名展示。
    } else {
        // 失败则不改模型
    }
}

void GridItemDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(index);
    // 强制编辑器位置与 paint 中计算的文件名绘制区域 NameBox 重合
    QRect cardRect = option.rect.adjusted(4, 4, -4, -4);
    int baseIconSize = option.decorationSize.width();
    if (baseIconSize <= 0) baseIconSize = 64; 
    int iconDrawSize = static_cast<int>(baseIconSize * 0.65); 
    
    int ratingH = 12;
    int nameH = 16;
    int gap1 = 6;
    int gap2 = 4;
    
    int totalH = iconDrawSize + gap1 + ratingH + gap2 + nameH;
    int startY = cardRect.top() + (cardRect.height() - totalH) / 2 + 13;
    int ratingY = startY + iconDrawSize + gap1;
    int nameY = ratingY + ratingH + gap2;
    
    // 编辑框精准覆盖文件名区域，不遮挡星级
    QRect nameBoxRect(cardRect.left() + 6, nameY, cardRect.width() - 12, nameH);
    editor->setGeometry(nameBoxRect);
}

} // namespace ArcMeta
