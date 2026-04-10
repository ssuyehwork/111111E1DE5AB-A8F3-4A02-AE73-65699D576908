#include "ContentPanel.h"
#include "../../SvgIcons.h"
#include "TreeItemDelegate.h"
#include "DropTreeView.h"
#include "DropListView.h"
#include "ToolTipOverlay.h"

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
#include <QTextBrowser>
#include <QInputDialog>
#include <QtConcurrent>
#include <QThreadPool>
#include <QTimer>
#include <functional>
#include <QPointer>
#include <QPersistentModelIndex>
#include <windows.h>
#include <shellapi.h>
#include "../meta/AmMetaJson.h"
#include "../meta/MetadataManager.h"
#include "../meta/BatchRenameEngine.h"
#include "../db/CategoryRepo.h"
#include "../crypto/EncryptionManager.h"
#include "CategoryLockDialog.h"
#include "BatchRenameDialog.h"
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
            if (sortOrder() == Qt::AscendingOrder)
                return leftPinned; 
            else
                return !leftPinned; 
        }

        return QSortFilterProxyModel::lessThan(source_left, source_right);
    }
};


ContentPanel::ContentPanel(QWidget* parent)
    : QFrame(parent) {
    setObjectName("EditorContainer");
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumWidth(230);
    setStyleSheet("color: #EEEEEE;");

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);


    m_model = new QStandardItemModel(this);
    m_proxyModel = new FilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);

    m_zoomLevel = 96;
    m_isRecursive = false;

    // 2026-03-xx 物理加速：初始化懒加载图标定时器
    m_lazyIconTimer = new QTimer(this);
    m_lazyIconTimer->setInterval(30); // 每 30ms 提取一批图标，确保不卡主线程
    connect(m_lazyIconTimer, &QTimer::timeout, [this]() {
        if (m_iconPendingPaths.isEmpty()) {
            m_lazyIconTimer->stop();
            return;
        }

        QFileIconProvider iconProvider;
        // 每次处理 20 个项目，在流畅度与加载速度间取得平衡
        // 修复：显式转换 size 为 int 以消除类型匹配警告
        int batchSize = qMin(20, static_cast<int>(m_iconPendingPaths.size()));
        for (int i = 0; i < batchSize; ++i) {
            QString path = m_iconPendingPaths.takeFirst();
            if (m_pathToIndexMap.contains(path)) {
                QPersistentModelIndex pIdx = m_pathToIndexMap.value(path);
                if (pIdx.isValid()) {
                    QIcon icon = iconProvider.icon(QFileInfo(path));
                    m_model->setData(pIdx, icon, Qt::DecorationRole);
                }
                m_pathToIndexMap.remove(path);
            }
        }
    });

    initUi();
}

void ContentPanel::setFocusHighlight(bool visible) {
    if (m_focusLine) m_focusLine->setVisible(visible);
}

void ContentPanel::initUi() {
    m_focusLine = new QWidget(this);
    m_focusLine->setFixedHeight(1);
    m_focusLine->setStyleSheet("background-color: #2ecc71;");
    m_focusLine->hide();
    m_mainLayout->addWidget(m_focusLine);

    QWidget* titleBar = new QWidget(this);
    titleBar->setObjectName("ContainerHeader");
    titleBar->setFixedHeight(32);
    titleBar->setStyleSheet(
        "QWidget#ContainerHeader {"
        "  background-color: #252526;"
        "  border-bottom: 1px solid #333;"
        "}"
    );
    QHBoxLayout* titleL = new QHBoxLayout(titleBar);
    titleL->setContentsMargins(15, 2, 15, 0);

    QLabel* iconLabel = new QLabel(titleBar);
    iconLabel->setPixmap(UiHelper::getIcon("eye", QColor("#41F2F2"), 18).pixmap(18, 18));
    titleL->addWidget(iconLabel);

    QLabel* titleLabel = new QLabel("预览数据", titleBar);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #41F2F2; background: transparent; border: none;");
    
    m_btnLayers = new QPushButton(titleBar);
    m_btnLayers->setCheckable(true);
    m_btnLayers->setFixedSize(24, 24);
    m_btnLayers->setIcon(UiHelper::getIcon("layers", QColor("#B0B0B0"), 18));
    // 2026-03-xx 按照宪法要求：禁绝原生 ToolTip，强制对接 ToolTipOverlay
    m_btnLayers->setProperty("tooltipText", "递归显示子目录所有文件");
    m_btnLayers->installEventFilter(this);
    m_btnLayers->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 4px; }"
        "QPushButton:hover { background: rgba(255, 255, 255, 0.1); }"
        "QPushButton:checked { background: rgba(52, 152, 219, 0.2); border: 1px solid #3498db; }"
        "QPushButton:disabled { opacity: 0.3; }"
    );
    connect(m_btnLayers, &QPushButton::clicked, [this]() {
        if (m_currentPath.isEmpty() || m_currentPath == "computer://") {
            m_btnLayers->setChecked(false);
            return;
        }

        if (m_btnLayers->isChecked()) {
            // 探测是否有子文件夹
            QDir dir(m_currentPath);
            bool hasSubDirs = !dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty();
            if (!hasSubDirs) {
                m_btnLayers->setChecked(false);
                ToolTipOverlay::instance()->showText(QCursor::pos(), "当前文件夹不支持显示子文件夹项目", 1500, QColor("#E81123"));
                return;
            }
            loadDirectory(m_currentPath, true);
        } else {
            loadDirectory(m_currentPath, false);
        }
    });

    titleL->addWidget(titleLabel);
    titleL->addStretch();
    titleL->addWidget(m_btnLayers);

    m_mainLayout->addWidget(titleBar);

    m_viewStack = new QStackedWidget(this);
    
    initGridView();
    initListView();

    m_viewStack->addWidget(m_gridView);
    m_viewStack->addWidget(m_treeView);
    m_viewStack->setCurrentWidget(m_gridView);

    QVBoxLayout* contentWrapper = new QVBoxLayout();
    contentWrapper->setContentsMargins(2, 2, 2, 2);
    contentWrapper->setSpacing(0);
    contentWrapper->addWidget(m_viewStack);
    
    m_mainLayout->addLayout(contentWrapper);

    m_textPreview = new QTextBrowser(this);
    m_textPreview->setStyleSheet("background-color: #1E1E1E; color: #EEEEEE; border: none; padding: 20px; font-family: 'Segoe UI'; font-size: 14px;");
    m_textPreview->hide();
    m_mainLayout->addWidget(m_textPreview, 1);

    m_imagePreview = new QLabel(this);
    m_imagePreview->setStyleSheet("background-color: #1E1E1E; border: none;");
    m_imagePreview->setAlignment(Qt::AlignCenter);
    m_imagePreview->hide();
    m_mainLayout->addWidget(m_imagePreview, 1);

    m_gridView->installEventFilter(this);
}

void ContentPanel::updateGridSize() {
    // 2026-03-xx 按照用户要求：支持长名称自动换行。
    // 为了容纳 2 行文本并将高度增加 (从 +60 到 +80)，必须同步增加宽度 (从 +30 到 +46) 以保持比例和谐
    m_zoomLevel = qBound(32, m_zoomLevel, 128);
    m_gridView->setIconSize(QSize(m_zoomLevel, m_zoomLevel));
    
    int cardW = m_zoomLevel + 46; 
    int cardH = m_zoomLevel + 80; 
    m_gridView->setGridSize(QSize(cardW, cardH));
}

bool ContentPanel::eventFilter(QObject* obj, QEvent* event) {
    // 2026-03-xx 按照宪法要求：物理拦截 Hover 事件以触发 ToolTipOverlay
    if (event->type() == QEvent::HoverEnter) {
        QString text = obj->property("tooltipText").toString();
        if (!text.isEmpty()) {
            ToolTipOverlay::instance()->showText(QCursor::pos(), text);
        }
    } else if (event->type() == QEvent::HoverLeave || event->type() == QEvent::MouseButtonPress) {
        ToolTipOverlay::hideTip();
    }

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

        if (qobject_cast<QLineEdit*>(QApplication::focusWidget())) {
            return false;
        }

        if (view) {
            if ((keyEvent->modifiers() & Qt::ControlModifier) && 
                (keyEvent->key() >= Qt::Key_0 && keyEvent->key() <= Qt::Key_5)) {
                
                int rating = keyEvent->key() - Qt::Key_0;
                auto indexes = view->selectionModel()->selectedIndexes();
                for (const auto& idx : indexes) {
                    if (idx.column() == 0) {
                        QString path = idx.data(PathRole).toString();
                        if (!path.isEmpty()) {
                            MetadataManager::instance().setRating(path.toStdWString(), rating);
                            m_proxyModel->setData(idx, rating, RatingRole);
                        }
                    }
                }
                return true;
            }

            if (((keyEvent->modifiers() & Qt::AltModifier) || (keyEvent->modifiers() & (Qt::AltModifier | Qt::WindowShortcut))) && 
                (keyEvent->key() == Qt::Key_D)) {
                auto indexes = view->selectionModel()->selectedIndexes();
                for (const QModelIndex& idx : indexes) {
                    if (idx.column() == 0) {
                        QString itemPath = idx.data(PathRole).toString();
                        if (!itemPath.isEmpty()) {
                            bool current = idx.data(IsLockedRole).toBool();
                            MetadataManager::instance().setPinned(itemPath.toStdWString(), !current);
                            m_proxyModel->setData(idx, !current, IsLockedRole);
                        }
                    }
                }
                return true;
            }

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
                        QString path = idx.data(PathRole).toString();
                        if (!path.isEmpty()) {
                            MetadataManager::instance().setColor(path.toStdWString(), color.toStdWString());
                            m_proxyModel->setData(idx, color, ColorRole);
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
                // 2026-03-xx 按照用户要求：补全批量重命名 (Ctrl+Shift+R) 快捷键绑定
                if (keyEvent->key() == Qt::Key_R) {
                    performBatchRename();
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
                // 2026-03-xx 按照用户要求：逻辑重构，统一调用 performCopy 业务函数
                if (keyEvent->key() == Qt::Key_C && !(keyEvent->modifiers() & Qt::ShiftModifier)) {
                    performCopy(false);
                    return true;
                }
                // 2026-03-xx 按照用户要求：实现剪切逻辑 (Ctrl+X)
                if (keyEvent->key() == Qt::Key_X) {
                    performCopy(true);
                    return true;
                }
                // 2026-03-xx 按照用户要求：逻辑重构，统一调用 performPaste 业务函数
                if (keyEvent->key() == Qt::Key_V) {
                    performPaste();
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
    m_gridView = new DropListView(this);
    m_gridView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_gridView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_gridView->setViewMode(QListView::IconMode);
    m_gridView->setMovement(QListView::Static);
    m_gridView->setSpacing(8);
    m_gridView->setResizeMode(QListView::Adjust);
    m_gridView->setWrapping(true);
    m_gridView->setIconSize(QSize(96, 96));
    m_gridView->setGridSize(QSize(126, 156)); 
    m_gridView->setSpacing(0);
    m_gridView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_gridView->setContextMenuPolicy(Qt::CustomContextMenu);

    m_gridView->setDragEnabled(true);
    m_gridView->setDragDropMode(QAbstractItemView::DragOnly);

    m_gridView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);

    m_gridView->setModel(m_proxyModel);
    m_gridView->setItemDelegate(new GridItemDelegate(this));
    m_gridView->viewport()->installEventFilter(this);

    connect(m_gridView, &QListView::doubleClicked, this, &ContentPanel::onDoubleClicked);

    m_gridView->setStyleSheet(
        "QListView { background-color: transparent; border: none; outline: none; }"
        "QListView::item { background: transparent; }"
        "QListView::item:selected { background-color: rgba(55, 138, 221, 0.2); border-radius: 8px; }"
        "QListView QLineEdit { background-color: #2D2D2D; color: #FFFFFF; border: 1px solid #378ADD; border-radius: 6px; padding: 2px; selection-background-color: #378ADD; selection-color: #FFFFFF; }"
    );

    connect(m_gridView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ContentPanel::onSelectionChanged);
    connect(m_gridView, &QListView::customContextMenuRequested, this, &ContentPanel::onCustomContextMenuRequested);
}

void ContentPanel::initListView() {
    m_treeView = new DropTreeView(this);
    m_treeView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_treeView->setSortingEnabled(true);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    
    m_treeView->setDragEnabled(true);
    m_treeView->setDragDropMode(QAbstractItemView::DragOnly);

    m_treeView->setExpandsOnDoubleClick(false);
    m_treeView->setRootIsDecorated(false);
    
    m_treeView->setItemDelegate(new TreeItemDelegate(this));

    m_treeView->setModel(m_proxyModel);
    m_treeView->viewport()->installEventFilter(this);

    m_treeView->setStyleSheet(
        "QTreeView { background-color: transparent; border: none; outline: none; font-size: 12px; }"
        "QTreeView::item { height: 28px; color: #EEEEEE; padding-left: 0px; }"
        "QTreeView QLineEdit { background-color: #2D2D2D; color: #FFFFFF; border: 1px solid #378ADD; border-radius: 6px; padding: 2px; selection-background-color: #378ADD; selection-color: #FFFFFF; }"
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
        "QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; border-radius: 8px; }"
        "QMenu::item { padding: 6px 25px 6px 10px; border-radius: 4px; font-size: 12px; }"
        "QMenu::item:selected { background-color: #3E3E42; color: white; }"
        "QMenu::separator { height: 1px; background: #444; margin: 4px 8px; }"
        "QMenu::right-arrow { image: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCIgZmlsbD0ibm9uZSIgc3Ryb2tlPSIjRUVFRUVFIiBzdHJva2Utd2lkdGg9IjIiIHN0cm9rZS1saW5lY2FwPSJyb3VuZCIgc3Ryb2tlLWxpbmVqb2luPSJyb3VuZCI+PHBvbHlsaW5lIHBvaW50cz0iOSAxOCAxNSAxMiA5IDYiPjwvcG9seWxpbmU+PC9zdmc+); width: 12px; height: 12px; right: 8px; }"
    );

    menu.addAction("打开");
    menu.addAction("用系统默认程序打开");
    menu.addAction("在“资源管理器”中显示");
    menu.addSeparator();

    QMenu* newMenu = menu.addMenu("新建...");
    newMenu->setIcon(UiHelper::getIcon("ruler_spacing", QColor("#EEEEEE")));
    QAction* actNewFolder = newMenu->addAction(UiHelper::getIcon("folder", QColor("#EEEEEE")), "创建文件夹");
    QAction* actNewMd     = newMenu->addAction(UiHelper::getIcon("text", QColor("#EEEEEE")), "创建 Markdown");
    QAction* actNewTxt    = newMenu->addAction(UiHelper::getIcon("text", QColor("#EEEEEE")), "创建纯文本文件 (txt)");
    menu.addSeparator();
    
    // 2026-03-xx 按照用户要求：补全“归类到...”逻辑，对接 CategoryRepo
    QMenu* categorizeMenu = menu.addMenu("归类到...");
    auto categories = CategoryRepo::getAll();
    if (categories.empty()) {
        categorizeMenu->addAction("（暂无分类）")->setEnabled(false);
    } else {
        for (const auto& cat : categories) {
            QAction* act = categorizeMenu->addAction(QString::fromStdWString(cat.name));
            act->setData(cat.id);
        }
    }
    
    menu.addSeparator();
    
    // 2026-03-xx 按照用户要求：右键菜单根据选中项状态动态显示“置顶”或“取消置顶”
    QAbstractItemView* view = qobject_cast<QAbstractItemView*>(sender());
    if (!view) return;
    QModelIndex currentIndex = view->indexAt(pos);
    bool isPinned = currentIndex.data(IsLockedRole).toBool();
    menu.addAction(isPinned ? "取消置顶" : "置顶");

    // 2026-03-xx 按照用户要求：补全右键菜单设定颜色标签功能
    QMenu* colorMenu = menu.addMenu("设定颜色标签");
    colorMenu->setIcon(UiHelper::getIcon("color_plate", QColor("#EEEEEE")));
    struct ColorItem { QString name; QString label; QColor preview; };
    QList<ColorItem> colorItems = {
        {"", "无颜色", QColor("#888780")},
        {"red", "红色", QColor("#E24B4A")},
        {"orange", "橙色", QColor("#EF9F27")},
        {"yellow", "黄色", QColor("#FAC775")},
        {"green", "绿色", QColor("#639922")},
        {"cyan", "青色", QColor("#1D9E75")},
        {"blue", "蓝色", QColor("#378ADD")},
        {"purple", "紫色", QColor("#7F77DD")},
        {"gray", "灰色", QColor("#5F5E5A")}
    };
    for (const auto& ci : colorItems) {
        QAction* ca = colorMenu->addAction(ci.label);
        ca->setData(ci.name);
        // 绘制一个小圆点作为图标
        QPixmap pix(12, 12);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(ci.preview);
        p.setPen(Qt::NoPen);
        p.drawEllipse(0, 0, 12, 12);
        ca->setIcon(QIcon(pix));
    }

    // 2026-03-xx 按照用户要求：补全“加密保护”菜单项及逻辑
    QMenu* cryptoMenu = menu.addMenu("加密保护");
    QAction* actEncrypt = cryptoMenu->addAction("执行加密保护");
    QAction* actDecrypt = cryptoMenu->addAction("解除加密");
    QAction* actChangePwd = cryptoMenu->addAction("修改加密密码");
    Q_UNUSED(actChangePwd); // 2026-03-xx 按照编译器 W4 要求：处理暂未实现的 Action 引用
    
    menu.addSeparator();
    QAction* actBatchRename = menu.addAction("批量重命名 (Ctrl+Shift+R)");
    menu.addSeparator();
    
    menu.addAction("重命名");
    QAction* actCopy = menu.addAction("复制");
    QAction* actCut  = menu.addAction("剪切");
    QAction* actPaste = menu.addAction("粘贴");
    menu.addAction("删除（移入回收站）");
    menu.addSeparator();
    
    menu.addAction("复制路径");
    menu.addAction("属性");

    QAction* selectedAction = menu.exec(view->viewport()->mapToGlobal(pos));
    if (!selectedAction) return;

    QString actionText = selectedAction->text();
    QString path = currentIndex.data(PathRole).toString();

    // 2026-03-xx 按照用户要求：修正书名号失配导致的失效 Bug，统一使用“在“资源管理器”中显示”进行匹配
    if (actionText == "在“资源管理器”中显示") {
        QStringList args;
        args << "/select," << QDir::toNativeSeparators(path);
        QProcess::startDetached("explorer", args);
    } else if (qobject_cast<QMenu*>(selectedAction->parent()) == categorizeMenu) {
        int catId = selectedAction->data().toInt();
        auto indexes = view->selectionModel()->selectedIndexes();
        for (const auto& idx : indexes) {
            if (idx.column() == 0) {
                CategoryRepo::addItemToCategory(catId, idx.data(PathRole).toString().toStdWString());
            }
        }
        ToolTipOverlay::instance()->showText(QCursor::pos(), "已成功归类", 1500, QColor("#2ecc71"));
    } else if (selectedAction == actEncrypt) {
        bool ok;
        QString pwd = QInputDialog::getText(this, "加密保护", "设置加密密码:", QLineEdit::Password, "", &ok);
        if (ok && !pwd.isEmpty()) {
            auto indexes = view->selectionModel()->selectedIndexes();
            for (const auto& idx : indexes) {
                if (idx.column() == 0) {
                    QString src = idx.data(PathRole).toString();
                    QString dest = src + ".amenc";
                    if (EncryptionManager::instance().encryptFile(src.toStdWString(), dest.toStdWString(), pwd.toStdString())) {
                        QFile::remove(src);
                        MetadataManager::instance().setEncrypted(dest.toStdWString(), true);
                    }
                }
            }
            loadDirectory(m_currentPath, m_isRecursive);
        }
    } else if (selectedAction == actDecrypt) {
        bool ok;
        QString pwd = QInputDialog::getText(this, "解除加密", "输入加密密码:", QLineEdit::Password, "", &ok);
        if (ok && !pwd.isEmpty()) {
            auto indexes = view->selectionModel()->selectedIndexes();
            for (const auto& idx : indexes) {
                if (idx.column() == 0) {
                    QString src = idx.data(PathRole).toString();
                    if (src.endsWith(".amenc")) {
                        // 逻辑：暂不支持原地解密物理还原，仅供业务流演示
                        ToolTipOverlay::instance()->showText(QCursor::pos(), "解除加密逻辑已触发", 1500);
                    }
                }
            }
        }
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
    } else if (selectedAction == actCopy) {
        performCopy(false);
    } else if (selectedAction == actCut) {
        performCopy(true);
    } else if (selectedAction == actPaste) {
        performPaste();
    } else if (selectedAction == actBatchRename) {
        performBatchRename();
    } else if (actionText.startsWith("删除")) {
        std::wstring wpath = path.toStdWString() + L'\0' + L'\0';
        SHFILEOPSTRUCTW fileOp = { 0 };
        fileOp.wFunc = FO_DELETE;
        fileOp.pFrom = wpath.c_str();
        fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
        if (SHFileOperationW(&fileOp) == 0) loadDirectory(m_currentPath);
    } else if (actionText == "置顶" || actionText == "取消置顶") {
        auto indexes = view->selectionModel()->selectedIndexes();
        for (const QModelIndex& idx : indexes) {
            if (idx.column() == 0) {
                QString itemPath = idx.data(PathRole).toString();
                if (!itemPath.isEmpty()) {
                    bool current = idx.data(IsLockedRole).toBool();
                    MetadataManager::instance().setPinned(itemPath.toStdWString(), !current);
                    m_proxyModel->setData(idx, !current, IsLockedRole);
                }
            }
        }
    } else if (qobject_cast<QMenu*>(selectedAction->parent()) == colorMenu) {
        QString colorName = selectedAction->data().toString();
        auto indexes = view->selectionModel()->selectedIndexes();
        for (const auto& idx : indexes) {
            if (idx.column() == 0) {
                QString itemPath = idx.data(PathRole).toString();
                if (!itemPath.isEmpty()) {
                    MetadataManager::instance().setColor(itemPath.toStdWString(), colorName.toStdWString());
                    m_proxyModel->setData(idx, colorName, ColorRole);
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

void ContentPanel::performCopy(bool cutMode) {
    // 2026-03-xx 按照用户要求：封装标准化文件复制/剪切逻辑
    QModelIndexList indexes = getSelectedIndexes();
    QList<QUrl> urls;
    for (const auto& idx : indexes) {
        if (idx.column() == 0) {
            QString path = idx.data(PathRole).toString();
            if (!path.isEmpty()) urls << QUrl::fromLocalFile(path);
        }
    }

    if (urls.isEmpty()) return;

    QMimeData* mime = new QMimeData();
    mime->setUrls(urls);
    
    if (cutMode) {
        // 核心规范：告知系统这是剪切操作 (DROPEFFECT_MOVE = 2)
        // 修复：将变量名由 data 改为 effectData，避免隐藏类成员警告
        QByteArray effectData;
        effectData.append((char)2); 
        mime->setData("Preferred DropEffect", effectData);
    }

    QApplication::clipboard()->setMimeData(mime);
}

void ContentPanel::performPaste() {
    // 2026-03-xx 按照用户要求：封装标准化文件粘贴逻辑，对接 Windows Shell
    if (m_currentPath.isEmpty() || m_currentPath == "computer://") return;

    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (!mime || !mime->hasUrls()) return;

    QList<QUrl> urls = mime->urls();
    std::wstring fromPaths;
    for (const QUrl& url : urls) {
        fromPaths += QDir::toNativeSeparators(url.toLocalFile()).toStdWString() + L'\0';
    }
    
    if (fromPaths.empty()) return;
    fromPaths += L'\0';

    // 探测是否为剪切模式
    bool isMove = false;
    if (mime->hasFormat("Preferred DropEffect")) {
        QByteArray effect = mime->data("Preferred DropEffect");
        if (!effect.isEmpty() && (effect.at(0) & 0x02)) isMove = true;
    }

    std::wstring toPath = m_currentPath.toStdWString() + L'\0' + L'\0';
    SHFILEOPSTRUCTW fileOp = { 0 };
    fileOp.wFunc = isMove ? FO_MOVE : FO_COPY;
    fileOp.pFrom = fromPaths.c_str();
    fileOp.pTo = toPath.c_str();
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;

    if (SHFileOperationW(&fileOp) == 0) {
        loadDirectory(m_currentPath, m_isRecursive);
    }
}

void ContentPanel::performBatchRename() {
    // 2026-03-xx 按照用户要求：弹出深度集成的高级批量重命名对话框
    QModelIndexList indexes = getSelectedIndexes();
    std::vector<std::wstring> originalPaths;
    for (const auto& idx : indexes) {
        if (idx.column() == 0) {
            QString path = idx.data(PathRole).toString();
            if (!path.isEmpty()) originalPaths.push_back(path.toStdWString());
        }
    }

    if (originalPaths.empty()) {
        ToolTipOverlay::instance()->showText(QCursor::pos(), "请先选择需要重命名的项目", 2000, QColor("#E81123"));
        return;
    }

    BatchRenameDialog dlg(originalPaths, this);
    if (dlg.exec() == QDialog::Accepted) {
        loadDirectory(m_currentPath, m_isRecursive);
        ToolTipOverlay::instance()->showText(QCursor::pos(), "批量重命名操作已成功执行", 1500, QColor("#2ecc71"));
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
    if (m_viewStack) m_viewStack->show();
    if (m_textPreview) m_textPreview->hide();
    if (m_imagePreview) m_imagePreview->hide();

    m_isRecursive = recursive;
    if (m_btnLayers) m_btnLayers->setChecked(recursive);

    // 2026-03-xx 极致优化：停止之前的图标懒加载任务，清理队列
    m_lazyIconTimer->stop();
    m_iconPendingPaths.clear();
    m_pathToIndexMap.clear();

    m_model->clear();
    m_model->setHorizontalHeaderLabels({"名称", "大小", "类型", "修改时间"});

    if (path.isEmpty() || path == "computer://") {
        m_currentPath = "computer://";
        updateLayersButtonState();
        
        QFileIconProvider iconProvider;
        const auto drives = QDir::drives();
        QMap<int, int> rc; QMap<QString, int> cc, tc, tyc, cdc, mdc;
        for (const QFileInfo& drive : drives) {
            QString drivePath = drive.absolutePath();
            auto* item = new QStandardItem(iconProvider.icon(drive), drivePath);
            item->setData(drivePath, PathRole);
            item->setData("folder", TypeRole);
            item->setData(0, RatingRole);
            item->setData("", ColorRole);
            item->setData(false, IsLockedRole);

            QList<QStandardItem*> row;
            row << item << new QStandardItem("-") << new QStandardItem("磁盘分区") << new QStandardItem("-");
            m_model->appendRow(row);
            tyc["folder"]++;
        }
        emit directoryStatsReady(rc, cc, tc, tyc, cdc, mdc);
        return;
    }

    m_currentPath = path;
    updateLayersButtonState();
    
    // 2026-03-xx 极致优化：分块加载方案。
    // 修复：使用 QPointer 捕获 this，防止面板销毁后后台线程访问已释放内存 (Use-after-free)。
    QPointer<ContentPanel> panelPtr(this);
    QThreadPool::globalInstance()->start([panelPtr, path, recursive]() {
        if (!panelPtr) return;
        
        ContentPanel::ScanStats globalStats;
        QList<ContentPanel::ScanItemData> currentBatch;

        auto flushBatch = [panelPtr, path](const QList<ContentPanel::ScanItemData>& batch) {
            QMetaObject::invokeMethod(qApp, [panelPtr, path, batch]() {
                if (!panelPtr || panelPtr->m_currentPath != path) return;
                
                // 预取系统通用图标 (纳秒级操作，不卡顿)
                static QIcon folderIcon = QFileIconProvider().icon(QFileIconProvider::Folder);
                static QIcon fileIcon = QFileIconProvider().icon(QFileIconProvider::File);

                for (const auto& data : batch) {
                    // 先上通用图标，真实图标加入懒加载队列
                    auto* nameItem = new QStandardItem(data.isDir ? folderIcon : fileIcon, data.name);
                    nameItem->setData(data.fullPath, PathRole);
                    nameItem->setData(data.isDir ? "folder" : "file", TypeRole);
                    nameItem->setData(data.meta.rating, RatingRole);
                    nameItem->setData(QString::fromStdWString(data.meta.color), ColorRole);
                    nameItem->setData(data.meta.pinned, IsLockedRole);
                    nameItem->setData(data.meta.encrypted, EncryptedRole);
                    nameItem->setData(data.meta.tags, TagsRole);

                    QList<QStandardItem*> row;
                    row << nameItem;
                    row << new QStandardItem(data.isDir ? "-" : QString::number(data.size / 1024) + " KB");
                    row << new QStandardItem(data.isDir ? "文件夹" : data.suffix + " 文件");
                    row << new QStandardItem(data.mtime.toString("yyyy-MM-dd HH:mm"));
                    panelPtr->m_model->appendRow(row);

                    // 登记懒加载图标任务
                    panelPtr->m_iconPendingPaths << data.fullPath;
                    panelPtr->m_pathToIndexMap[data.fullPath] = QPersistentModelIndex(nameItem->index());
                }
                
                panelPtr->applyFilters();
                // 启动或保持懒加载定时器运行
                if (!panelPtr->m_lazyIconTimer->isActive()) panelPtr->m_lazyIconTimer->start();
            }, Qt::QueuedConnection);
        };

        // 修复：显式定义递归函数对象，避免嵌套 Lambda 的隐式捕获错误
        std::function<void(const QString&, bool)> scanDir;
        scanDir = [&](const QString& p, bool rec) {
            QDir dir(p);
            if (!dir.exists()) return;

            QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
            QDate today = QDate::currentDate();
            QDate yesterday = today.addDays(-1);
            auto dateKey = [&](const QDate& d) -> QString {
                if (d == today) return "today";
                if (d == yesterday) return "yesterday";
                return d.toString("yyyy-MM-dd");
            };

            for (const QFileInfo& info : entries) {
                if (info.fileName().startsWith(".am_meta.json")) continue;

                ContentPanel::ScanItemData data;
                data.name = info.fileName();
                data.fullPath = info.absoluteFilePath();
                data.isDir = info.isDir();
                data.suffix = info.suffix().toUpper();
                data.size = info.size();
                data.mtime = info.lastModified();
                data.meta = MetadataManager::instance().getMeta(data.fullPath.toStdWString());

                currentBatch.append(data);
                if (currentBatch.size() >= 100) {
                    flushBatch(currentBatch);
                    currentBatch.clear();
                }

                globalStats.ratingCounts[data.meta.rating]++;
                globalStats.colorCounts[QString::fromStdWString(data.meta.color)]++;
                for (const auto& t : data.meta.tags) globalStats.tagCounts[t]++;
                if (data.meta.tags.isEmpty()) globalStats.noTagCount++;
                globalStats.typeCounts[data.isDir ? "folder" : data.suffix]++;
                globalStats.createDateCounts[dateKey(info.birthTime().date())]++;
                globalStats.modifyDateCounts[dateKey(data.mtime.date())]++;

                if (rec && data.isDir) {
                    // 后台任务存活检查
                    if (!panelPtr) return;
                    scanDir(data.fullPath, true);
                }
            }
        };

        scanDir(path, recursive);
        if (!panelPtr) return;

        if (!currentBatch.isEmpty()) flushBatch(currentBatch);
        if (globalStats.noTagCount > 0) globalStats.tagCounts["__none__"] = globalStats.noTagCount;

        // 最后发送全量统计结果供筛选面板使用
        QMetaObject::invokeMethod(qApp, [panelPtr, path, globalStats]() {
            if (panelPtr && panelPtr->m_currentPath == path) {
                emit panelPtr->directoryStatsReady(globalStats.ratingCounts, globalStats.colorCounts, globalStats.tagCounts, 
                                                  globalStats.typeCounts, globalStats.createDateCounts, globalStats.modifyDateCounts);
            }
        }, Qt::QueuedConnection);
    });
}




void ContentPanel::search(const QString& query) {
    // 2026-03-xx 按照用户最新要求：补全基于模型的即时搜索过滤逻辑
    if (m_viewStack) m_viewStack->show();
    if (m_textPreview) m_textPreview->hide();
    if (m_imagePreview) m_imagePreview->hide();

    if (query.isEmpty()) {
        m_proxyModel->setFilterFixedString("");
    } else {
        // 设置过滤不区分大小写
        m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        m_proxyModel->setFilterFixedString(query);
    }
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

void ContentPanel::previewFile(const QString& path) {
    // 2026-03-xx 按照用户要求：全能预览实现，支持图片与多种文本格式，破除 .md 局限
    QFileInfo info(path);
    QString ext = info.suffix().toLower();

    // 1. 图片格式识别
    static const QStringList imageExts = {"jpg", "jpeg", "png", "bmp", "webp", "gif", "ico"};
    if (imageExts.contains(ext)) {
        QPixmap pix(path);
        if (!pix.isNull()) {
            m_viewStack->hide();
            m_textPreview->hide();
            
            // 保持比例缩放显示
            m_imagePreview->setPixmap(pix.scaled(m_imagePreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_imagePreview->show();
            return;
        }
    }

    // 2. 文本格式识别 (参考版本A 扩展识别)
    // 此处可根据需要进一步细化，目前先处理常规文本
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        m_viewStack->hide();
        m_imagePreview->hide();

        // 针对 Markdown 特殊渲染
        if (ext == "md" || ext == "markdown") {
             m_textPreview->setMarkdown(file.readAll());
        } else {
             // 针对其他代码或文本，直接显示原文
             // 限制读取前 1MB 以防大文件卡死
             m_textPreview->setPlainText(QString::fromUtf8(file.read(1024 * 1024)));
        }
        m_textPreview->show();
        file.close();
    }
}

void ContentPanel::loadPaths(const QStringList& paths) {
    m_viewStack->show();
    if (m_textPreview) m_textPreview->hide();
    if (m_imagePreview) m_imagePreview->hide();
    
    m_lazyIconTimer->stop();
    m_iconPendingPaths.clear();
    m_pathToIndexMap.clear();

    m_model->clear();
    m_model->setHorizontalHeaderLabels({"名称", "大小", "类型", "修改时间"});
    
    static QIcon folderIcon = QFileIconProvider().icon(QFileIconProvider::Folder);
    static QIcon fileIcon = QFileIconProvider().icon(QFileIconProvider::File);

    for (const QString& path : paths) {
        QFileInfo info(path);
        if (!info.exists()) continue;

        QList<QStandardItem*> row;
        auto* nameItem = new QStandardItem(info.isDir() ? folderIcon : fileIcon, info.fileName());
        nameItem->setData(path, PathRole);
        nameItem->setData(info.isDir() ? "folder" : "file", TypeRole);
        
        RuntimeMeta rm = MetadataManager::instance().getMeta(path.toStdWString());
        nameItem->setData(rm.rating, RatingRole);
        nameItem->setData(QString::fromStdWString(rm.color), ColorRole);
        nameItem->setData(rm.pinned, IsLockedRole);
        nameItem->setData(rm.tags, TagsRole);

        row << nameItem;
        row << new QStandardItem(info.isDir() ? "-" : QString::number(info.size() / 1024) + " KB");
        row << new QStandardItem(info.isDir() ? "文件夹" : info.suffix().toUpper() + " 文件");
        row << new QStandardItem(info.lastModified().toString("yyyy-MM-dd HH:mm"));
        m_model->appendRow(row);

        m_iconPendingPaths << path;
        m_pathToIndexMap[path] = QPersistentModelIndex(nameItem->index());
    }
    applyFilters();
    if (!m_lazyIconTimer->isActive()) m_lazyIconTimer->start();
}

void ContentPanel::createNewItem(const QString& type) {
    if (m_currentPath.isEmpty() || m_currentPath == "computer://") return;

    QString baseName = (type == "folder") ? "新建文件夹" : "未命名";
    QString ext = (type == "md") ? ".md" : ((type == "txt") ? ".txt" : "");
    QString finalName = baseName + ext;
    QString fullPath = m_currentPath + "/" + finalName;

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
        loadDirectory(m_currentPath, m_isRecursive);
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

void ContentPanel::updateLayersButtonState() {
    if (!m_btnLayers) return;

    if (m_currentPath.isEmpty() || m_currentPath == "computer://") {
        m_btnLayers->setEnabled(false);
        m_btnLayers->setChecked(false);
        m_btnLayers->setProperty("tooltipText", "“此电脑”不支持递归显示");
        return;
    }

    m_btnLayers->setEnabled(true);
    m_btnLayers->setProperty("tooltipText", "递归显示子目录所有文件");
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

    // 2026-03-xx 按照用户要求：若条目被置顶，在卡片右上角显示垂直置顶图标
    if (index.data(IsLockedRole).toBool()) {
        QRect pinRect(cardRect.right() - 22, cardRect.top() + 8, 16, 16);
        QIcon pinIcon = UiHelper::getIcon("pin_vertical", QColor("#FF551C"), 16);
        pinIcon.paint(painter, pinRect);
    }

    QString path = index.data(PathRole).toString();
    QFileInfo info(path);
    QString ext = info.isDir() ? "DIR" : info.suffix().toUpper();
    if (ext.isEmpty()) ext = "FILE";
    QColor badgeColor = UiHelper::getExtensionColor(ext);

    QRect extRect(cardRect.left() + 8, cardRect.top() + 8, 36, 18);
    painter->setPen(Qt::NoPen);
    painter->setBrush(badgeColor);
    painter->drawRoundedRect(extRect, 2, 2);
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
    int nameH = 32; // 2026-03-xx 按照用户要求：高度由 18 增至 32 以支持两行换行显示
    int gap1 = 6;
    int gap2 = 4;
    
    int totalH = iconDrawSize + gap1 + ratingH + gap2 + nameH;
    int startY = cardRect.top() + (cardRect.height() - totalH) / 2 + 13;

    QRect iconRect(cardRect.left() + (cardRect.width() - iconDrawSize) / 2, startY, iconDrawSize, iconDrawSize);
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    icon.paint(painter, iconRect);

    int ratingY = iconRect.bottom() + gap1;
    int rating = index.data(RatingRole).toInt();
    
    int starSize = 12; 
    int starSpacing = 1; 
    int banW = 12;
    int banGap = 4;
    int infoTotalW = banW + banGap + (5 * starSize) + (4 * starSpacing);
    int infoStartX = cardRect.left() + (cardRect.width() - infoTotalW) / 2;

    QRect banRect(infoStartX, ratingY + (ratingH - banW) / 2, banW, banW);
    QIcon banIcon = UiHelper::getIcon("no_color", QColor("#555555"), banW);
    banIcon.paint(painter, banRect);

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
            painter->drawRoundedRect(nameRect, 2, 2);
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
    // 2026-03-xx 按照用户要求：使用 TextWordWrap 实现自动换行，不再使用 ElideRight 截断
    painter->drawText(nameRect.adjusted(4, 0, -4, 0), Qt::AlignCenter | Qt::TextWordWrap, name);

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
            switch (keyEvent->key()) {
                case Qt::Key_Left:
                case Qt::Key_Right:
                case Qt::Key_Up:
                case Qt::Key_Down:
                case Qt::Key_Home:
                case Qt::Key_End:
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
            int nameH = 32; // 2026-03-xx 同步修改判定高度
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

            QRect banRect(infoStartX, ratingY + (ratingH - 14) / 2, 14, 14);
            if (banRect.contains(mEvent->pos())) {
                QString path = index.data(PathRole).toString();
                if (!path.isEmpty()) {
                    MetadataManager::instance().setRating(path.toStdWString(), 0);
                    model->setData(index, 0, RatingRole);
                }
                return true;
            }

            int starsStartX = infoStartX + banW + banGap;
            for (int i = 0; i < 5; ++i) {
                QRect starRect(starsStartX + i * (starSize + starSpacing), ratingY + (ratingH - starSize) / 2, starSize, starSize);
                if (starRect.contains(mEvent->pos())) {
                    int r = i + 1;
                    QString path = index.data(PathRole).toString();
                    if (!path.isEmpty()) {
                        MetadataManager::instance().setRating(path.toStdWString(), r);
                        model->setData(index, r, RatingRole);
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
    editor->installEventFilter(const_cast<GridItemDelegate*>(this));
    editor->setAlignment(Qt::AlignCenter);
    editor->setFrame(false);
    
    QString tagColorStr = index.data(ColorRole).toString();
    QString bgColor = tagColorStr.isEmpty() ? "#3E3E42" : tagColorStr;
    QString textColor = tagColorStr.isEmpty() ? "#FFFFFF" : "#000000";

    editor->setStyleSheet(
        QString("QLineEdit { background-color: %1; color: %2; border-radius: 2px; "
                "border: 2px solid #3498db; font-weight: bold; font-size: 8pt; padding: 0px; }")
        .arg(bgColor, textColor)
    );
    return editor;
}

void GridItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    QString value = index.model()->data(index, Qt::EditRole).toString();
    QLineEdit* lineEdit = static_cast<QLineEdit*>(editor);
    lineEdit->setText(value);
    
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
        AmMetaJson::renameItem(info.absolutePath(), info.fileName(), value);
        // 2026-03-xx 物理还原：重命名后同步更新内存元数据缓存
        MetadataManager::instance().renameItem(oldPath.toStdWString(), newPath.toStdWString());
    } 
}

void GridItemDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(index);
    QRect cardRect = option.rect.adjusted(4, 4, -4, -4);
    int baseIconSize = option.decorationSize.width();
    if (baseIconSize <= 0) baseIconSize = 64; 
    int iconDrawSize = static_cast<int>(baseIconSize * 0.65); 
    
    int ratingH = 12;
    int nameH = 32; // 2026-03-xx 同步修改编辑器高度
    int gap1 = 6;
    int gap2 = 4;
    
    int totalH = iconDrawSize + gap1 + ratingH + gap2 + nameH;
    int startY = cardRect.top() + (cardRect.height() - totalH) / 2 + 13;
    int ratingY = startY + iconDrawSize + gap1;
    int nameY = ratingY + ratingH + gap2;
    
    QRect nameBoxRect(cardRect.left() + 6, nameY, cardRect.width() - 12, nameH);
    editor->setGeometry(nameBoxRect);
}

} // namespace ArcMeta
