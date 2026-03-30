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
        
        if (!currentFilter.ratings.isEmpty()) {
            int r = idx.data(RatingRole).toInt();
            if (!currentFilter.ratings.contains(r)) return false;
        }

        if (!currentFilter.colors.isEmpty()) {
            QString c = idx.data(ColorRole).toString();
            if (!currentFilter.colors.contains(c)) return false;
        }

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

        return true;
    }

    bool lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const override {
        bool leftPinned = source_left.data(IsLockedRole).toBool();
        bool rightPinned = source_right.data(IsLockedRole).toBool();

        if (leftPinned != rightPinned) {
            if (sortOrder() == Qt::AscendingOrder) return leftPinned;
            else return !leftPinned;
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

    // 2026-03-xx 按照用户要求：初始化异步加载定时器
    m_loadTimer = new QTimer(this);
    m_loadTimer->setInterval(10);
    connect(m_loadTimer, &QTimer::timeout, this, &ContentPanel::processLoadBatch);

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
    
    m_gridView->installEventFilter(this);
}

void ContentPanel::updateGridSize() {
    m_zoomLevel = qBound(32, m_zoomLevel, 128);
    m_gridView->setIconSize(QSize(m_zoomLevel, m_zoomLevel));
    int cardW = m_zoomLevel + 30;
    int cardH = m_zoomLevel + 60;
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

        if (qobject_cast<QLineEdit*>(QApplication::focusWidget())) return false;

        if (view) {
            // Ctrl + 0..5 (星级)
            if ((keyEvent->modifiers() & Qt::ControlModifier) && (keyEvent->key() >= Qt::Key_0 && keyEvent->key() <= Qt::Key_5)) {
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
            // Alt + D (置顶)
            if ((keyEvent->modifiers() & Qt::AltModifier) && (keyEvent->key() == Qt::Key_D)) {
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
            if (keyEvent->key() == Qt::Key_Space) {
                QModelIndex idx = view->currentIndex();
                if (idx.isValid()) emit requestQuickLook(idx.data(PathRole).toString());
                return true;
            }
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                onDoubleClicked(view->currentIndex());
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
    } else QWidget::wheelEvent(event);
}

void ContentPanel::setViewMode(ViewMode mode) {
    if (mode == GridView) m_viewStack->setCurrentWidget(m_gridView);
    else m_viewStack->setCurrentWidget(m_treeView);
}

void ContentPanel::initGridView() {
    m_gridView = new QListView(this);
    m_gridView->setViewMode(QListView::IconMode);
    m_gridView->setMovement(QListView::Static);
    m_gridView->setResizeMode(QListView::Adjust);
    m_gridView->setIconSize(QSize(96, 96));
    m_gridView->setGridSize(QSize(126, 156));
    m_gridView->setSpacing(10);
    m_gridView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_gridView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_gridView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);

    m_gridView->setModel(m_proxyModel);
    m_gridView->setItemDelegate(new GridItemDelegate(this));
    m_gridView->viewport()->installEventFilter(this);

    connect(m_gridView, &QListView::doubleClicked, this, &ContentPanel::onDoubleClicked);
    connect(m_gridView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ContentPanel::onSelectionChanged);
    connect(m_gridView, &QListView::customContextMenuRequested, this, &ContentPanel::onCustomContextMenuRequested);

    m_gridView->setStyleSheet(
        "QListView { background-color: transparent; border: none; outline: none; }"
        "QListView::item:selected { background-color: rgba(55, 138, 221, 0.2); border-radius: 6px; }"
    );
}

void ContentPanel::initListView() {
    m_treeView = new QTreeView(this);
    m_treeView->setSortingEnabled(true);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setRootIsDecorated(false);
    m_treeView->setModel(m_proxyModel);
    m_treeView->viewport()->installEventFilter(this);

    m_treeView->setStyleSheet(
        "QTreeView { background-color: transparent; border: none; outline: none; font-size: 12px; }"
        "QTreeView::item { height: 28px; color: #EEEEEE; padding-left: 4px; }"
        "QTreeView::item:selected { background-color: #378ADD; }"
    );
    m_treeView->header()->setStyleSheet("QHeaderView::section { background-color: #252525; color: #B0B0B0; padding-left: 10px; border: none; height: 32px; font-size: 11px; }");

    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ContentPanel::onSelectionChanged);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &ContentPanel::onCustomContextMenuRequested);
    connect(m_treeView, &QTreeView::doubleClicked, this, &ContentPanel::onDoubleClicked);
}

void ContentPanel::onCustomContextMenuRequested(const QPoint& pos) {
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #2B2B2B; border: 1px solid #444444; color: #EEEEEE; padding: 4px; border-radius: 6px; }");
    menu.addAction("打开");
    menu.addAction("复制路径");
    menu.addAction("重命名");

    QAbstractItemView* view = qobject_cast<QAbstractItemView*>(sender());
    if (!view) return;
    QAction* act = menu.exec(view->viewport()->mapToGlobal(pos));
    if (!act) return;

    QModelIndex idx = view->indexAt(pos);
    QString path = idx.data(PathRole).toString();

    if (act->text() == "复制路径") QApplication::clipboard()->setText(QDir::toNativeSeparators(path));
    else if (act->text() == "重命名") view->edit(idx);
    else if (act->text() == "打开") onDoubleClicked(idx);
}

void ContentPanel::onSelectionChanged() {
    QItemSelectionModel* sel = (m_viewStack->currentWidget() == m_gridView) ? m_gridView->selectionModel() : m_treeView->selectionModel();
    if (!sel) return;
    QStringList paths;
    for (const QModelIndex& index : sel->selectedIndexes()) if (index.column() == 0) paths << index.data(PathRole).toString();
    emit selectionChanged(paths);
}

void ContentPanel::onDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) return;
    QString path = index.data(PathRole).toString();
    if (QFileInfo(path).isDir()) emit directorySelected(path);
    else QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void ContentPanel::loadDirectory(const QString& path, bool recursive) {
    m_loadTimer->stop();
    m_loadQueue.clear();
    m_mftLoadQueue.clear();
    m_isSearching = false;
    m_model->clear();
    m_model->setHorizontalHeaderLabels({"名称", "大小", "类型", "修改时间"});
    m_accRating.clear(); m_accColor.clear(); m_accTag.clear(); m_accType.clear();
    m_accCreateDate.clear(); m_accModifyDate.clear(); m_accNoTagCount = 0;

    if (path.isEmpty() || path == "computer://") {
        m_currentPath = "computer://";
        QFileIconProvider ip;
        for (const QFileInfo& d : QDir::drives()) {
            auto* it = new QStandardItem(ip.icon(d), d.absolutePath());
            it->setData(d.absolutePath(), PathRole);
            it->setData("folder", TypeRole);
            QList<QStandardItem*> row;
            row << it << new QStandardItem("-") << new QStandardItem("磁盘分区") << new QStandardItem("-");
            m_model->appendRow(row);
            m_accType["folder"]++;
        }
        emit directoryStatsReady(m_accRating, m_accColor, m_accTag, m_accType, m_accCreateDate, m_accModifyDate);
        return;
    }
    m_currentPath = path;
    addItemsToQueue(path, recursive);
    m_loadTimer->start();
}

void ContentPanel::addItemsToQueue(const QString& path, bool recursive) {
    QDir dir(path);
    if (!dir.exists()) return;
    QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
    m_loadQueue.append(entries);
    if (recursive) {
        for (const QFileInfo& info : entries) if (info.isDir()) addItemsToQueue(info.absoluteFilePath(), true);
    }
}

void ContentPanel::processLoadBatch() {
    int processed = 0;
    const int batchSize = 100;
    QFileIconProvider ip;
    QDate today = QDate::currentDate();
    QDate yesterday = today.addDays(-1);
    auto dateKey = [&](const QDate& d) -> QString {
        if (d == today) return "today";
        if (d == yesterday) return "yesterday";
        return d.toString("yyyy-MM-dd");
    };

    while (processed < batchSize) {
        QString fileName, fullPath, suffix;
        bool isDir = false;
        QFileInfo info;

        if (m_isSearching) {
            if (m_mftLoadQueue.isEmpty()) break;
            auto entry = m_mftLoadQueue.takeFirst();
            fileName = QString::fromStdWString(entry.name);
            if (fileName == ".am_meta.json") continue;
            fullPath = QString::fromStdWString(PathBuilder::getPath(entry.volume, entry.frn));
            info = QFileInfo(fullPath);
            isDir = entry.isDir;
            suffix = info.suffix().toUpper();
        } else {
            if (m_loadQueue.isEmpty()) break;
            info = m_loadQueue.takeFirst();
            fileName = info.fileName();
            if (fileName == ".am_meta.json") continue;
            fullPath = info.absoluteFilePath();
            isDir = info.isDir();
            suffix = info.suffix().toUpper();
        }

        auto* nameItem = new QStandardItem(ip.icon(info), fileName);
        nameItem->setData(fullPath, PathRole);
        nameItem->setData(isDir ? "folder" : "file", TypeRole);

        RuntimeMeta rm = MetadataManager::instance().getMeta(fullPath.toStdWString());
        nameItem->setData(rm.rating, RatingRole);
        nameItem->setData(QString::fromStdWString(rm.color), ColorRole);
        nameItem->setData(rm.pinned, IsLockedRole);
        nameItem->setData(rm.tags, TagsRole);

        m_accRating[rm.rating]++;
        m_accColor[QString::fromStdWString(rm.color)]++;
        if (rm.tags.isEmpty()) m_accNoTagCount++;
        else { for (const auto& t : rm.tags) m_accTag[t]++; }
        m_accType[isDir ? "folder" : suffix]++;
        m_accCreateDate[dateKey(info.birthTime().date())]++;
        m_accModifyDate[dateKey(info.lastModified().date())]++;

        QList<QStandardItem*> row;
        row << nameItem;
        if (m_isSearching) row << new QStandardItem(fullPath) << new QStandardItem(isDir ? "文件夹" : suffix + " 文件");
        else row << new QStandardItem(isDir ? "-" : QString::number(info.size() / 1024) + " KB") << new QStandardItem(isDir ? "文件夹" : suffix + " 文件");
        row << new QStandardItem(info.lastModified().toString("yyyy-MM-dd HH:mm"));
        m_model->appendRow(row);
        processed++;
    }

    if ((!m_isSearching && m_loadQueue.isEmpty()) || (m_isSearching && m_mftLoadQueue.isEmpty())) {
        m_loadTimer->stop();
        if (m_accNoTagCount > 0) m_accTag["__none__"] = m_accNoTagCount;
        emit directoryStatsReady(m_accRating, m_accColor, m_accTag, m_accType, m_accCreateDate, m_accModifyDate);
        applyFilters();
    }
}

void ContentPanel::search(const QString& query) {
    if (query.isEmpty()) return;
    m_loadTimer->stop(); m_loadQueue.clear(); m_mftLoadQueue.clear();
    m_isSearching = true; m_model->clear();
    m_model->setHorizontalHeaderLabels({"名称", "路径", "类型", "修改时间"});
    m_accRating.clear(); m_accColor.clear(); m_accTag.clear(); m_accType.clear();
    m_accCreateDate.clear(); m_accModifyDate.clear(); m_accNoTagCount = 0;

    auto results = MftReader::instance().search(query.toStdWString());
    int count = 0;
    for (const auto& entry : results) {
        if (++count > 1000) break;
        m_mftLoadQueue.append({entry.name, entry.volume, entry.frn, entry.isDir()});
    }
    m_loadTimer->start();
}

void ContentPanel::applyFilters(const FilterState& state) { m_currentFilter = state; applyFilters(); }
void ContentPanel::applyFilters() { static_cast<FilterProxyModel*>(m_proxyModel)->currentFilter = m_currentFilter; static_cast<FilterProxyModel*>(m_proxyModel)->updateFilter(); }
void ContentPanel::createNewItem(const QString& type) { if (m_currentPath.isEmpty() || m_currentPath == "computer://") return; loadDirectory(m_currentPath); }

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
    painter->setPen(Qt::NoPen); painter->setBrush(badgeColor); painter->drawRoundedRect(extRect, 2, 2);
    painter->setPen(QColor("#FFFFFF")); QFont extFont = painter->font(); extFont.setPointSize(8); extFont.setBold(true); painter->setFont(extFont);
    painter->drawText(extRect, Qt::AlignCenter, ext);

    int iconDrawSize = static_cast<int>(64 * 0.65);
    int totalH = iconDrawSize + 6 + 12 + 4 + 18;
    int startY = cardRect.top() + (cardRect.height() - totalH) / 2 + 13;
    QRect iconRect(cardRect.left() + (cardRect.width() - iconDrawSize) / 2, startY, iconDrawSize, iconDrawSize);
    index.data(Qt::DecorationRole).value<QIcon>().paint(painter, iconRect);

    int ratingY = iconRect.bottom() + 6;
    int rating = index.data(RatingRole).toInt();
    int infoStartX = cardRect.left() + (cardRect.width() - (12 + 4 + 5 * 12 + 4)) / 2;
    UiHelper::getIcon("no_color", QColor("#555555"), 12).paint(painter, QRect(infoStartX, ratingY, 12, 12));

    int starsStartX = infoStartX + 12 + 4;
    QPixmap fStar = UiHelper::getPixmap("star_filled", QSize(12, 12), QColor("#EF9F27"));
    QPixmap eStar = UiHelper::getPixmap("star", QSize(12, 12), QColor("#444444"));
    for (int i = 0; i < 5; ++i) painter->drawPixmap(QRect(starsStartX + i * 13, ratingY, 12, 12), (i < rating) ? fStar : eStar);

    int nameY = ratingY + 12 + 4;
    QRect nameRect(cardRect.left() + 6, nameY, cardRect.width() - 12, 18);
    QString colorName = index.data(ColorRole).toString();
    if (!colorName.isEmpty()) {
        QColor dotC(colorName);
        if (!dotC.isValid()) {
            if (colorName.contains("red")) dotC = QColor("#E24B4A");
            else if (colorName.contains("orange")) dotC = QColor("#EF9F27");
            else if (colorName.contains("green")) dotC = QColor("#639922");
            else if (colorName.contains("blue")) dotC = QColor("#378ADD");
        }
        if (dotC.isValid()) { painter->setPen(Qt::NoPen); painter->setBrush(dotC); painter->drawRoundedRect(nameRect, 2, 2); painter->setPen(dotC.lightness() > 180 ? Qt::black : Qt::white); }
    } else painter->setPen(QColor("#CCCCCC"));

    painter->drawText(nameRect.adjusted(4, 0, -4, 0), Qt::AlignCenter | Qt::ElideRight, index.data(Qt::DisplayRole).toString());
    painter->restore();
}

QSize GridItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const {
    auto* view = qobject_cast<const QListView*>(option.widget);
    if (view && view->gridSize().isValid()) return view->gridSize();
    return QSize(126, 156);
}

bool GridItemDelegate::eventFilter(QObject* obj, QEvent* event) { return QStyledItemDelegate::eventFilter(obj, event); }
bool GridItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mEvent = static_cast<QMouseEvent*>(event);
        if (mEvent->button() == Qt::LeftButton) {
            int iconDrawSize = static_cast<int>(64 * 0.65);
            int startY = option.rect.top() + (option.rect.height() - (iconDrawSize + 6 + 12 + 4 + 18)) / 2 + 13;
            int ratingY = startY + iconDrawSize + 6;
            int infoStartX = option.rect.left() + (option.rect.width() - (12 + 4 + 5 * 12 + 4)) / 2;
            if (QRect(infoStartX, ratingY, 14, 14).contains(mEvent->pos())) {
                QString p = index.data(PathRole).toString();
                if (!p.isEmpty()) { MetadataManager::instance().setRating(p.toStdWString(), 0); model->setData(index, 0, RatingRole); }
                return true;
            }
            int starsStartX = infoStartX + 12 + 4;
            for (int i = 0; i < 5; ++i) {
                if (QRect(starsStartX + i * 13, ratingY, 12, 12).contains(mEvent->pos())) {
                    int r = i + 1; QString p = index.data(PathRole).toString();
                    if (!p.isEmpty()) { MetadataManager::instance().setRating(p.toStdWString(), r); model->setData(index, r, RatingRole); }
                    return true;
                }
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QWidget* GridItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QLineEdit* editor = new QLineEdit(parent); editor->setAlignment(Qt::AlignCenter); editor->setFrame(false); return editor;
}
void GridItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const { static_cast<QLineEdit*>(editor)->setText(index.data(Qt::EditRole).toString()); }
void GridItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    QString val = static_cast<QLineEdit*>(editor)->text(); if (val.isEmpty()) return;
    QString oldP = index.data(PathRole).toString(); QFileInfo info(oldP); QString newP = info.absolutePath() + "/" + val;
    // 2026-03-xx 物理修复：对齐头文件定义的 (QString, QString, QString) 签名
    if (QFile::rename(oldP, newP)) { model->setData(index, val, Qt::EditRole); model->setData(index, newP, PathRole); AmMetaJson::renameItem(info.absolutePath(), info.fileName(), val); }
}
void GridItemDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QRect cardRect = option.rect.adjusted(4, 4, -4, -4);
    int startY = cardRect.top() + (cardRect.height() - (static_cast<int>(64 * 0.65) + 6 + 12 + 4 + 18)) / 2 + 13;
    editor->setGeometry(QRect(cardRect.left() + 6, startY + static_cast<int>(64 * 0.65) + 6 + 12 + 4, cardRect.width() - 12, 18));
}

} // namespace ArcMeta
