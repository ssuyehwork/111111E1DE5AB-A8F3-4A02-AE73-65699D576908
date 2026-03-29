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
#include <QPersistentModelIndex>
#include <QThreadPool>

#include "../mft/MftReader.h"
#include "../mft/PathBuilder.h"
#include "../meta/AmMetaJson.h"
#include "../meta/MetadataManager.h"
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
            QString type = idx.data(TypeRole).toString();
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

        // 5. 日期过滤
        QFileInfo info(idx.data(PathRole).toString());
        QDate cDate = info.birthTime().date();
        QDate mDate = info.lastModified().date();
        QDate today = QDate::currentDate();

        auto matchDate = [&](const QDate& d, const QStringList& filters) {
            if (filters.isEmpty()) return true;
            for (const auto& f : filters) {
                if (f == "today" && d == today) return true;
                if (f == "yesterday" && d == today.addDays(-1)) return true;
                if (f == d.toString("yyyy-MM-dd")) return true;
            }
            return false;
        };

        if (!matchDate(cDate, currentFilter.createDates)) return false;
        if (!matchDate(mDate, currentFilter.modifyDates)) return false;

        return true;
    }

    bool lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const override {
        bool lp = source_left.data(IsLockedRole).toBool();
        bool rp = source_right.data(IsLockedRole).toBool();
        if (lp != rp) {
            return (sortOrder() == Qt::AscendingOrder) ? lp : !lp;
        }
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

    QLabel* titleLabel = new QLabel("内容区域", titleBar);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #4a90e2; border: none;");
    
    QPushButton* btnLayers = new QPushButton(titleBar);
    btnLayers->setFixedSize(24, 24);
    btnLayers->setIcon(UiHelper::getIcon("layers", QColor("#B0B0B0"), 18));
    btnLayers->setToolTip("递归显示");
    btnLayers->setStyleSheet("QPushButton { background: transparent; border: none; border-radius: 4px; } QPushButton:hover { background: rgba(255, 255, 255, 0.1); }");

    connect(btnLayers, &QPushButton::clicked, [this]() {
        if (!m_currentPath.isEmpty() && m_currentPath != "computer://") loadDirectory(m_currentPath, true);
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
    contentWrapper->addWidget(m_viewStack);
    m_mainLayout->addLayout(contentWrapper);
    
    m_gridView->installEventFilter(this);
    m_treeView->installEventFilter(this);
}

void ContentPanel::updateGridSize() {
    m_zoomLevel = qBound(32, m_zoomLevel, 128);
    m_gridView->setIconSize(QSize(m_zoomLevel, m_zoomLevel));
    m_gridView->setGridSize(QSize(m_zoomLevel + 30, m_zoomLevel + 60));
}

bool ContentPanel::eventFilter(QObject* obj, QEvent* event) {
    if ((obj == m_gridView || obj == m_gridView->viewport()) && event->type() == QEvent::Wheel) {
        QWheelEvent* w = static_cast<QWheelEvent*>(event);
        if (w->modifiers() & Qt::ControlModifier) {
            m_zoomLevel += (w->angleDelta().y() > 0 ? 8 : -8);
            updateGridSize();
            return true;
        }
    }

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* k = static_cast<QKeyEvent*>(event);
        QAbstractItemView* v = qobject_cast<QAbstractItemView*>(obj);
        if (!v) v = qobject_cast<QAbstractItemView*>(obj->parent());

        if (qobject_cast<QLineEdit*>(QApplication::focusWidget())) return false;

        if (v && v->currentIndex().isValid()) {
            QModelIndex idx = v->currentIndex();
            QString path = idx.data(PathRole).toString();

            // 1. 评级打标 (Ctrl+0..5)
            if ((k->modifiers() & Qt::ControlModifier) && (k->key() >= Qt::Key_0 && k->key() <= Qt::Key_5)) {
                int r = k->key() - Qt::Key_0;
                MetadataManager::instance().setRating(path.toStdWString(), r);
                m_proxyModel->setData(idx, r, RatingRole);
                return true;
            }
            // 2. 颜色打标 (Alt+1..9)
            if (k->modifiers() & Qt::AltModifier && k->key() >= Qt::Key_1 && k->key() <= Qt::Key_9) {
                QString c;
                switch(k->key()) {
                    case Qt::Key_1: c = "red"; break; case Qt::Key_2: c = "orange"; break; case Qt::Key_3: c = "yellow"; break;
                    case Qt::Key_4: c = "green"; break; case Qt::Key_5: c = "cyan"; break; case Qt::Key_6: c = "blue"; break;
                    case Qt::Key_7: c = "purple"; break; case Qt::Key_8: c = "gray"; break;
                }
                MetadataManager::instance().setColor(path.toStdWString(), c.toStdWString());
                m_proxyModel->setData(idx, c, ColorRole);
                return true;
            }
            // 3. 置顶 (Alt+D)
            if (k->modifiers() & Qt::AltModifier && k->key() == Qt::Key_D) {
                bool p = !idx.data(IsLockedRole).toBool();
                MetadataManager::instance().setPinned(path.toStdWString(), p);
                m_proxyModel->setData(idx, p, IsLockedRole);
                return true;
            }
            // 4. 空格预览
            if (k->key() == Qt::Key_Space) { emit requestQuickLook(path); return true; }
            // 5. Backspace 返回上一级
            if (k->key() == Qt::Key_Backspace) {
                QDir dir(m_currentPath);
                if (dir.cdUp()) emit directorySelected(dir.absolutePath());
                return true;
            }
            // 6. F2 重命名
            if (k->key() == Qt::Key_F2) { v->edit(idx); return true; }
            // 7. Delete 删除
            if (k->key() == Qt::Key_Delete) {
                handleDeleteAction(path);
                return true;
            }
            // 8. Ctrl+C 复制路径
            if (k->modifiers() & Qt::ControlModifier && k->key() == Qt::Key_C) {
                QApplication::clipboard()->setText(QDir::toNativeSeparators(path));
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ContentPanel::setViewMode(ViewMode mode) {
    m_viewStack->setCurrentWidget(mode == GridView ? (QWidget*)m_gridView : (QWidget*)m_treeView);
}

void ContentPanel::initGridView() {
    m_gridView = new QListView(this);
    m_gridView->setViewMode(QListView::IconMode);
    m_gridView->setMovement(QListView::Static);
    m_gridView->setSpacing(10);
    m_gridView->setResizeMode(QListView::Adjust);
    m_gridView->setIconSize(QSize(96, 96));
    m_gridView->setGridSize(QSize(126, 156));
    m_gridView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_gridView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_gridView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);

    m_gridView->setModel(m_proxyModel);
    m_gridView->setItemDelegate(new GridItemDelegate(this));
    m_gridView->viewport()->installEventFilter(this);

    connect(m_gridView, &QListView::doubleClicked, this, &ContentPanel::onDoubleClicked);
    connect(m_gridView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ContentPanel::onSelectionChanged);
    connect(m_gridView, &QListView::customContextMenuRequested, this, &ContentPanel::onCustomContextMenuRequested);
}

void ContentPanel::initListView() {
    m_treeView = new QTreeView(this);
    m_treeView->setSortingEnabled(true);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setRootIsDecorated(false);
    m_treeView->setModel(m_proxyModel);
    m_treeView->header()->setStyleSheet("QHeaderView::section { background: #252525; color: #B0B0B0; height: 32px; border: none; }");

    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ContentPanel::onSelectionChanged);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &ContentPanel::onCustomContextMenuRequested);
    connect(m_treeView, &QTreeView::doubleClicked, this, &ContentPanel::onDoubleClicked);
}

void ContentPanel::onCustomContextMenuRequested(const QPoint& pos) {
    QAbstractItemView* v = qobject_cast<QAbstractItemView*>(sender());
    if (!v) return;

    QModelIndex idx = v->indexAt(pos);
    QString path = idx.data(PathRole).toString();

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #2B2B2B; border: 1px solid #444444; color: #EEEEEE; padding: 4px; border-radius: 6px; }"
        "QMenu::item { height: 24px; padding: 0 10px 0 10px; border-radius: 3px; font-size: 12px; }"
        "QMenu::item:selected { background-color: #505050; }"
        "QMenu::separator { height: 1px; background: #444444; margin: 4px 8px 4px 8px; }"
    );

    if (idx.isValid()) {
        menu.addAction("打开");
        menu.addAction("在“资源管理器”中显示");
        menu.addSeparator();
        menu.addAction("重命名");
        menu.addAction("删除");
        menu.addSeparator();
        menu.addAction("属性");
    } else {
        menu.addAction("新建文件夹");
        menu.addAction("新建文本文件");
        menu.addSeparator();
        menu.addAction("刷新");
    }

    QAction* act = menu.exec(v->viewport()->mapToGlobal(pos));
    if (!act) return;

    QString text = act->text();
    if (text == "打开") onDoubleClicked(idx);
    else if (text == "在“资源管理器”中显示") {
        QStringList args; args << "/select," << QDir::toNativeSeparators(path);
        QProcess::startDetached("explorer", args);
    }
    else if (text == "重命名") v->edit(idx);
    else if (text == "删除") handleDeleteAction(path);
    else if (text == "新建文件夹") createNewItem("folder");
    else if (text == "新建文本文件") createNewItem("txt");
    else if (text == "刷新") loadDirectory(m_currentPath);
    else if (text == "属性") {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_INVOKEIDLIST; sei.lpVerb = L"properties";
        std::wstring wpath = path.toStdWString(); sei.lpFile = wpath.c_str(); sei.nShow = SW_SHOW;
        ShellExecuteExW(&sei);
    }
}

void ContentPanel::handleDeleteAction(const QString& path) {
    if (path.isEmpty()) return;
    std::wstring wpath = path.toStdWString() + L'\0' + L'\0';
    SHFILEOPSTRUCTW fileOp = { 0 };
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = wpath.c_str();
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
    if (SHFileOperationW(&fileOp) == 0) loadDirectory(m_currentPath);
}

void ContentPanel::onSelectionChanged() {
    QAbstractItemView* v = (m_viewStack->currentWidget() == m_gridView) ? (QAbstractItemView*)m_gridView : (QAbstractItemView*)m_treeView;
    QStringList paths;
    for (const auto& idx : v->selectionModel()->selectedIndexes()) if (idx.column() == 0) paths << idx.data(PathRole).toString();
    emit selectionChanged(paths);
}

void ContentPanel::onDoubleClicked(const QModelIndex& index) {
    QString p = index.data(PathRole).toString();
    if (QFileInfo(p).isDir()) emit directorySelected(p);
    else QDesktopServices::openUrl(QUrl::fromLocalFile(p));
}

void ContentPanel::loadDirectory(const QString& path, bool recursive) {
    m_model->clear();
    m_model->setHorizontalHeaderLabels({"名称", "大小", "类型", "修改时间"});
    m_currentPath = path;

    if (path.isEmpty() || path == "computer://") {
        const auto drives = QDir::drives();
        for (const auto& d : drives) {
            auto* item = new QStandardItem(UiHelper::getIcon("folder", QColor("#B0B0B0")), d.absolutePath());
            item->setData(d.absolutePath(), PathRole);
            item->setData("folder", TypeRole);
            m_model->appendRow({item, new QStandardItem("-"), new QStandardItem("磁盘分区"), new QStandardItem("-")});

            QPersistentModelIndex pIdx(item->index());
            QString dPath = d.absolutePath();
            QThreadPool::globalInstance()->start([this, pIdx, dPath]() {
                QIcon icon = QFileIconProvider().icon(QFileInfo(dPath));
                QMetaObject::invokeMethod(this, [this, pIdx, icon]() {
                    if (pIdx.isValid()) m_model->itemFromIndex(pIdx)->setIcon(icon);
                }, Qt::QueuedConnection);
            });
        }
        return;
    }

    QMap<int, int> r, c, t, tp, cd, md; int nt = 0;
    addItemsFromDirectory(path, recursive, r, c, t, tp, cd, md, nt);
    applyFilters();
    if (nt > 0) t["__none__"] = nt;
    emit directoryStatsReady(r, c, t, tp, cd, md);
}

void ContentPanel::addItemsFromDirectory(const QString& path, bool recursive, QMap<int, int>& r, QMap<QString, int>& c, QMap<QString, int>& t, QMap<QString, int>& tp, QMap<QString, int>& cd, QMap<QString, int>& md, int& nt) {
    QDir dir(path);
    if (!dir.exists()) return;

    QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
    QIcon placeholder = UiHelper::getIcon("folder", QColor("#B0B0B0"));

    for (const auto& info : entries) {
        QString fullPath = info.absoluteFilePath();
        if (info.fileName() == ".am_meta.json") continue;

        auto* nameItem = new QStandardItem(placeholder, info.fileName());
        nameItem->setData(fullPath, PathRole);
        nameItem->setData(info.isDir() ? "folder" : "file", TypeRole);

        RuntimeMeta rm = MetadataManager::instance().getMeta(fullPath.toStdWString());
        nameItem->setData(rm.rating, RatingRole);
        nameItem->setData(QString::fromStdWString(rm.color), ColorRole);
        nameItem->setData(rm.pinned, IsLockedRole);
        nameItem->setData(rm.tags, TagsRole);

        r[rm.rating]++;
        c[QString::fromStdWString(rm.color)]++;
        for (const auto& tag : rm.tags) t[tag]++;
        if (rm.tags.isEmpty()) nt++;
        tp[info.isDir() ? "folder" : info.suffix().toUpper()]++;

        m_model->appendRow({ nameItem,
            new QStandardItem(info.isDir() ? "-" : QString::number(info.size()/1024)+" KB"),
            new QStandardItem(info.isDir() ? "文件夹" : info.suffix().toUpper()+" 文件"),
            new QStandardItem(info.lastModified().toString("yyyy-MM-dd HH:mm"))
        });

        QPersistentModelIndex pIdx(nameItem->index());
        QThreadPool::globalInstance()->start([this, pIdx, fullPath]() {
            QIcon realIcon = QFileIconProvider().icon(QFileInfo(fullPath));
            QMetaObject::invokeMethod(this, [this, pIdx, realIcon]() {
                if (pIdx.isValid()) m_model->itemFromIndex(pIdx)->setIcon(realIcon);
            }, Qt::QueuedConnection);
        });

        if (recursive && info.isDir()) addItemsFromDirectory(fullPath, true, r, c, t, tp, cd, md, nt);
    }
}

void ContentPanel::search(const QString& query) {
    if (query.isEmpty()) return;
    m_model->clear();
    m_model->setHorizontalHeaderLabels({"名称", "路径", "类型", "修改时间"});

    auto results = MftReader::instance().search(query.toStdWString());
    QIcon placeholder = UiHelper::getIcon("folder", QColor("#B0B0B0"));

    int count = 0;
    for (const auto& entry : results) {
        if (++count > 1000) break;
        std::wstring fwPath = PathBuilder::getPath(entry.volume, entry.frn);
        QString fPath = QString::fromStdWString(fwPath);
        if (fPath.isEmpty()) continue;

        QFileInfo info(fPath);
        auto* nameItem = new QStandardItem(placeholder, info.fileName());
        nameItem->setData(fPath, PathRole);
        nameItem->setData(entry.isDir() ? "folder" : "file", TypeRole);

        RuntimeMeta rm = MetadataManager::instance().getMeta(fwPath);
        nameItem->setData(rm.rating, RatingRole);
        nameItem->setData(QString::fromStdWString(rm.color), ColorRole);
        nameItem->setData(rm.pinned, IsLockedRole);

        m_model->appendRow({ nameItem, new QStandardItem(fPath), new QStandardItem(entry.isDir() ? "文件夹" : "文件"), new QStandardItem("-") });

        QPersistentModelIndex pIdx(nameItem->index());
        QThreadPool::globalInstance()->start([this, pIdx, fPath]() {
            QIcon icon = QFileIconProvider().icon(QFileInfo(fPath));
            QMetaObject::invokeMethod(this, [this, pIdx, icon]() {
                if (pIdx.isValid()) m_model->itemFromIndex(pIdx)->setIcon(icon);
            }, Qt::QueuedConnection);
        });
    }
}

void ContentPanel::applyFilters(const FilterState& state) {
    static_cast<FilterProxyModel*>(m_proxyModel)->currentFilter = state;
    applyFilters();
}

void ContentPanel::applyFilters() {
    static_cast<FilterProxyModel*>(m_proxyModel)->updateFilter();
}

void ContentPanel::createNewItem(const QString& type) {
    if (m_currentPath.isEmpty() || m_currentPath == "computer://") return;
    QString n = (type == "folder" ? "新建文件夹" : "未命名") + (type == "txt" ? ".txt" : "");
    QString p = m_currentPath + "/" + n;
    int c = 1; while (QFileInfo::exists(p)) p = m_currentPath + "/" + n + QString(" (%1)").arg(c++);
    if (type == "folder" ? QDir(m_currentPath).mkdir(n) : QFile(p).open(QIODevice::WriteOnly)) loadDirectory(m_currentPath);
}

// --- Delegate ---

void GridItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QRect cardRect = option.rect.adjusted(4, 4, -4, -4);
    bool sel = (option.state & QStyle::State_Selected);
    bool hov = (option.state & QStyle::State_MouseOver);

    painter->setPen(sel ? QPen(QColor("#3498db"), 2) : QPen(QColor("#333333"), 1));
    painter->setBrush(sel ? QColor("#282828") : (hov ? QColor("#2A2A2A") : QColor("#2D2D2D")));
    painter->drawRoundedRect(cardRect, 8, 8);

    int iconSize = static_cast<int>(option.decorationSize.width() * 0.65);
    QRect iconRect(cardRect.left() + (cardRect.width() - iconSize) / 2, cardRect.top() + 20, iconSize, iconSize);
    index.data(Qt::DecorationRole).value<QIcon>().paint(painter, iconRect);

    int rY = iconRect.bottom() + 10;
    int r = index.data(RatingRole).toInt();
    int sX = cardRect.left() + (cardRect.width() - 80) / 2;
    for (int i = 0; i < 5; ++i) {
        painter->drawPixmap(QRect(sX + 16 + i * 13, rY, 12, 12), UiHelper::getPixmap(i < r ? "star_filled" : "star", QSize(12, 12), i < r ? QColor("#EF9F27") : QColor("#444444")));
    }

    QRect nRect(cardRect.left() + 6, rY + 18, cardRect.width() - 12, 18);
    QString col = index.data(ColorRole).toString();
    if (!col.isEmpty()) { painter->setPen(Qt::NoPen); painter->setBrush(QColor(col)); painter->drawRoundedRect(nRect, 2, 2); painter->setPen(Qt::white); }
    else painter->setPen(QColor("#CCCCCC"));
    painter->drawText(nRect.adjusted(4, 0, -4, 0), Qt::AlignCenter | Qt::ElideRight, index.data(Qt::DisplayRole).toString());

    painter->restore();
}

QSize GridItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const {
    auto* v = qobject_cast<const QListView*>(option.widget);
    return v ? v->gridSize() : QSize(96, 156);
}

bool GridItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (event->type() == QEvent::MouseButtonPress && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
        QPoint p = static_cast<QMouseEvent*>(event)->pos();
        int rY = option.rect.top() + 20 + (int)(option.decorationSize.width() * 0.65) + 10;
        int sX = option.rect.left() + (option.rect.width() - 80) / 2;
        for (int i = 0; i < 5; ++i) {
            if (QRect(sX + 16 + i * 13, rY, 12, 12).contains(p)) {
                MetadataManager::instance().setRating(index.data(PathRole).toString().toStdWString(), i + 1);
                model->setData(index, i + 1, RatingRole); return true;
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QWidget* GridItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const {
    QLineEdit* e = new QLineEdit(parent);
    e->setStyleSheet("background: #2D2D2D; color: white; border: 1px solid #378ADD;");
    return e;
}

void GridItemDelegate::setEditorData(QWidget* e, const QModelIndex& i) const { ((QLineEdit*)e)->setText(i.data(Qt::EditRole).toString()); }

void GridItemDelegate::setModelData(QWidget* e, QAbstractItemModel* m, const QModelIndex& i) const {
    QString v = ((QLineEdit*)e)->text(), o = i.data(PathRole).toString();
    QFileInfo f(o); QString n = f.absolutePath() + "/" + v;
    if (QFile::rename(o, n)) {
        m->setData(i, v, Qt::EditRole); m->setData(i, n, PathRole);
        AmMetaJson::renameItem(f.absolutePath(), f.fileName(), v);
    }
}

void GridItemDelegate::updateEditorGeometry(QWidget* e, const QStyleOptionViewItem& o, const QModelIndex&) const {
    e->setGeometry(o.rect.left() + 6, o.rect.bottom() - 25, o.rect.width() - 12, 18);
}

} // namespace ArcMeta
