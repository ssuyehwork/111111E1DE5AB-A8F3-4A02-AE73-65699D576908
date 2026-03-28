#include "ContentPanel.h"
#include "../../SvgIcons.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QSvgRenderer>
#include <QPainter>
#include <QHeaderView>
#include <QScrollBar>
#include <QMenu>
#include <QAction>
#include <QEvent>
#include <QKeyEvent>
#include <QStyleOptionViewItem>
#include <QItemSelectionModel>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QFileIconProvider>
#include "../mft/MftReader.h"

namespace ArcMeta {

// 定义自定义角色，用于存储元数据
enum ItemRole {
    RatingRole = Qt::UserRole + 1,
    ColorRole,
    PinnedRole,
    EncryptedRole,
    PathRole
};

/**
 * @brief 构造函数，设置面板属性
 */
ContentPanel::ContentPanel(QWidget* parent)
    : QWidget(parent) {
    setMinimumWidth(200);
    setStyleSheet("QWidget { background-color: #1A1A1A; color: #EEEEEE; border: none; }");

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // 修复：实例化数据模型
    m_model = new QStandardItemModel(this);

    initUi();
}

/**
 * @brief 初始化整体 UI 结构
 */
void ContentPanel::initUi() {
    m_viewStack = new QStackedWidget(this);

    initGridView();
    initListView();

    m_viewStack->addWidget(m_gridView);
    m_viewStack->addWidget(m_treeView);
    m_viewStack->setCurrentWidget(m_gridView);

    m_mainLayout->addWidget(m_viewStack);
}

void ContentPanel::setViewMode(ViewMode mode) {
    if (mode == GridView) {
        m_viewStack->setCurrentWidget(m_gridView);
    } else {
        m_viewStack->setCurrentWidget(m_treeView);
    }
}

void ContentPanel::initGridView() {
    m_gridView = new QListView(this);
    m_gridView->setViewMode(QListView::IconMode);
    m_gridView->setMovement(QListView::Static);
    m_gridView->setSpacing(8);
    m_gridView->setResizeMode(QListView::Adjust);
    m_gridView->setWrapping(true);
    m_gridView->setIconSize(QSize(64, 64));
    m_gridView->setGridSize(QSize(96, 112));
    m_gridView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_gridView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // 修复：绑定模型
    m_gridView->setModel(m_model);
    m_gridView->setItemDelegate(new GridItemDelegate(this));
    m_gridView->viewport()->installEventFilter(this);

    m_gridView->setStyleSheet(
        "QListView { background-color: transparent; border: none; outline: none; }"
        "QListView::item { background: transparent; }"
        "QListView::item:selected { background-color: rgba(55, 138, 221, 0.2); border-radius: 4px; }"
    );

    connect(m_gridView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ContentPanel::onSelectionChanged);
    connect(m_gridView, &QListView::customContextMenuRequested,
            this, &ContentPanel::onCustomContextMenuRequested);
}

void ContentPanel::initListView() {
    m_treeView = new QTreeView(this);
    m_treeView->setSortingEnabled(true);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setExpandsOnDoubleClick(false);
    m_treeView->setRootIsDecorated(false);

    // 修复：绑定模型
    m_treeView->setModel(m_model);
    m_treeView->viewport()->installEventFilter(this);

    m_treeView->setStyleSheet(
        "QTreeView { background-color: transparent; border: none; outline: none; font-size: 12px; }"
        "QTreeView::item { height: 28px; color: #EEEEEE; padding-left: 4px; }"
        "QTreeView::item:selected { background-color: #378ADD; }"
        "QTreeView::item:hover { background-color: rgba(255, 255, 255, 0.05); }"
    );

    m_treeView->header()->setStyleSheet(
        "QHeaderView::section { background-color: #252525; color: #B0B0B0; padding-left: 10px; border: none; height: 32px; font-size: 11px; }"
    );

    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ContentPanel::onSelectionChanged);
    connect(m_treeView, &QTreeView::customContextMenuRequested,
            this, &ContentPanel::onCustomContextMenuRequested);
    connect(m_treeView, &QTreeView::doubleClicked, this, &ContentPanel::onDoubleClicked);
}

bool ContentPanel::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Space) {
            QAbstractItemView* view = qobject_cast<QAbstractItemView*>(obj->parent());
            if (view) {
                QModelIndex index = view->currentIndex();
                if (index.isValid()) {
                    QString path = index.data(PathRole).toString();
                    emit requestQuickLook(path);
                }
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ContentPanel::onCustomContextMenuRequested(const QPoint& pos) {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #2B2B2B; border: 1px solid #444444; color: #EEEEEE; padding: 4px; }"
        "QMenu::item { height: 26px; padding: 0 24px 0 32px; border-radius: 4px; font-size: 12px; }"
        "QMenu::item:selected { background-color: #378ADD; }"
        "QMenu::separator { height: 1px; background: #444444; margin: 4px 8px 4px 8px; }"
    );

    menu.addAction("打开");
    menu.addAction("用系统默认程序打开");
    menu.addAction("在资源管理器中显示");
    menu.addSeparator();
    menu.addMenu("归类到...");
    menu.addAction("添加到收藏夹");
    menu.addSeparator();
    menu.addAction("置顶 / 取消置顶");
    QMenu* cryptoMenu = menu.addMenu("加密操作");
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
    if (view) menu.exec(view->viewport()->mapToGlobal(pos));
}

void ContentPanel::onSelectionChanged() {
    QItemSelectionModel* selectionModel = nullptr;
    if (m_viewStack->currentWidget() == m_gridView) {
        selectionModel = m_gridView->selectionModel();
    } else {
        selectionModel = m_treeView->selectionModel();
    }

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
        loadDirectory(path); // 钻取目录
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

/**
 * @brief 实现目录加载逻辑（开箱即用：调用 MftReader 索引层）
 */
void ContentPanel::loadDirectory(const QString& path) {
    m_model->clear();
    m_model->setHorizontalHeaderLabels({"名称", "大小", "类型", "修改时间"});

    auto entries = MftReader::instance().getChildren(path.toStdWString());
    QFileIconProvider iconProvider;

    for (const auto& entry : entries) {
        QString fileName = QString::fromStdWString(entry.name);
        QString fullPath = path + "/" + fileName;
        QFileInfo info(fullPath);

        QList<QStandardItem*> row;
        auto* nameItem = new QStandardItem(iconProvider.icon(info), fileName);
        nameItem->setData(fullPath, PathRole);
        nameItem->setData(entry.isDir() ? "folder" : "file", Qt::UserRole); // 模拟 type

        row << nameItem;
        row << new QStandardItem(entry.isDir() ? "-" : QString::number(info.size() / 1024) + " KB");
        row << new QStandardItem(entry.isDir() ? "文件夹" : info.suffix().toUpper() + " 文件");
        row << new QStandardItem(info.lastModified().toString("yyyy-MM-dd HH:mm"));

        m_model->appendRow(row);
    }
}

// --- Delegate ---

void GridItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    if (option.state & QStyle::State_Selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(55, 138, 221, 50));
        painter->drawRoundedRect(option.rect.adjusted(2, 2, -2, -2), 4, 4);
    }

    QRect iconRect(option.rect.left() + (option.rect.width() - 64) / 2,
                   option.rect.top() + 8, 64, 64);

    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    icon.paint(painter, iconRect);

    int rating = index.data(RatingRole).toInt();
    if (rating > 0) {
        QString stars = QString("★").repeated(rating) + QString("☆").repeated(5 - rating);
        QRect starRect(option.rect.left(), iconRect.bottom() + 2, option.rect.width(), 14);
        painter->setPen(QColor("#EF9F27"));
        QFont starFont = painter->font();
        starFont.setPointSize(11);
        painter->setFont(starFont);
        painter->drawText(starRect, Qt::AlignCenter, stars);
    }

    if (index.data(PinnedRole).toBool()) {
        QRect pinRect(iconRect.left(), iconRect.top(), 16, 16);
        QIcon pinIcon = ContentPanel::getSvgIcon("pin", QColor("#EF9F27"));
        pinIcon.paint(painter, pinRect);
    }

    QString colorName = index.data(ColorRole).toString();
    if (!colorName.isEmpty()) {
        QColor dotColor;
        if (colorName == "red") dotColor = QColor("#E24B4A");
        else if (colorName == "orange") dotColor = QColor("#EF9F27");
        else if (colorName == "yellow") dotColor = QColor("#FAC775");
        else if (colorName == "green") dotColor = QColor("#639922");
        else if (colorName == "cyan") dotColor = QColor("#1D9E75");
        else if (colorName == "blue") dotColor = QColor("#378ADD");
        else if (colorName == "purple") dotColor = QColor("#7F77DD");
        else if (colorName == "gray") dotColor = QColor("#5F5E5A");

        if (dotColor.isValid()) {
            painter->setBrush(dotColor);
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(iconRect.right() - 8 - 2, iconRect.bottom() - 8 - 2, 8, 8);
        }
    }

    if (index.data(EncryptedRole).toBool()) {
        QRect lockRect(iconRect.right() - 16, iconRect.bottom() - 16, 16, 16);
        QIcon lockIcon = ContentPanel::getSvgIcon("lock", QColor("#FAC775"));
        lockIcon.paint(painter, lockRect);
    }

    QRect textRect(option.rect.left() + 4, option.rect.bottom() - 32 - 4, option.rect.width() - 8, 32);
    QString name = index.data(Qt::DisplayRole).toString();
    painter->setPen(QColor("#EEEEEE"));
    QFont textFont = painter->font();
    textFont.setPointSize(12);
    painter->setFont(textFont);
    painter->drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap | Qt::ElideRight, name);

    painter->restore();
}

QSize GridItemDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const {
    return QSize(96, 112);
}

/**
 * @brief 静态辅助方法，供外部和 Delegate 使用
 */
QIcon ContentPanel::getSvgIcon(const QString& key, const QColor& color) {
    if (!SvgIcons::icons.contains(key)) return QIcon();
    QString svgDataStr = SvgIcons::icons[key];
    svgDataStr.replace("currentColor", color.name());
    QSvgRenderer renderer(svgDataStr.toUtf8());
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    return QIcon(pixmap);
}

} // namespace ArcMeta
