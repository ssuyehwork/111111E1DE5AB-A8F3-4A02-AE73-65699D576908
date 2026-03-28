#include "FavoritesPanel.h"
#include "../../SvgIcons.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QMimeData>
#include <QFileInfo>
#include <QIcon>
#include <QFileIconProvider>
#include <QSvgRenderer>
#include <QPainter>

namespace ArcMeta {

/**
 * @brief 构造函数，设置面板属性
 */
FavoritesPanel::FavoritesPanel(QWidget* parent)
    : QWidget(parent) {
    // 设置面板宽度（遵循文档：收藏面板 200px）
    setFixedWidth(200);
    setAcceptDrops(true); // 开启拖拽接受
    setStyleSheet("QWidget { background-color: #1E1E1E; color: #EEEEEE; border: none; }");

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 12);
    m_mainLayout->setSpacing(0);

    initUi();
}

/**
 * @brief 初始化整体 UI 结构
 */
void FavoritesPanel::initUi() {
    // 面板标题
    QLabel* titleLabel = new QLabel("快速收藏", this);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #888; padding: 10px 12px; background: #252525;");
    m_mainLayout->addWidget(titleLabel);

    // 分组标题
    QLabel* groupHeader = new QLabel("收藏夹", this);
    groupHeader->setStyleSheet(
        "font-size: 11px; font-weight: 500; color: #5F5E5A; "
        "padding: 8px 10px 4px 10px;"
    );
    m_mainLayout->addWidget(groupHeader);

    // 收藏列表
    m_listWidget = new QListWidget(this);
    m_listWidget->setSpacing(2);
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listWidget->setStyleSheet(
        "QListWidget { background: transparent; border: none; outline: none; }"
        "QListWidget::item { height: 32px; padding: 0 10px 0 10px; color: #EEEEEE; border-radius: 4px; }"
        "QListWidget::item:hover { background-color: rgba(255, 255, 255, 0.05); }"
        "QListWidget::item:selected { background-color: #378ADD; }"
    );

    connect(m_listWidget, &QListWidget::itemClicked, this, &FavoritesPanel::onItemClicked);
    connect(m_listWidget, &QListWidget::customContextMenuRequested, 
            this, &FavoritesPanel::onCustomContextMenuRequested);

    m_mainLayout->addWidget(m_listWidget, 1);

    // 初始化底部拖拽区域
    initDropZone();
}

/**
 * @brief 初始化底部的“拖拽放置区域”（遵循 60px 虚线边框规范）
 */
void FavoritesPanel::initDropZone() {
    m_dropZone = new QWidget(this);
    m_dropZone->setFixedHeight(60);
    
    // 设置虚线边框与圆角 (圆角 6px)
    m_dropZone->setStyleSheet(
        "QWidget { "
        "  border: 1px dashed #444444; "
        "  border-radius: 6px; "
        "  margin: 0 10px 0 10px; "
        "  background: rgba(255, 255, 255, 0.02); "
        "}"
    );

    QVBoxLayout* dropLayout = new QVBoxLayout(m_dropZone);
    dropLayout->setContentsMargins(0, 0, 0, 0);
    dropLayout->setAlignment(Qt::AlignCenter);

    m_dropLabel = new QLabel("拖拽文件夹或文件到此处", m_dropZone);
    m_dropLabel->setStyleSheet("font-size: 11px; color: #5F5E5A; border: none; background: transparent;");
    m_dropLabel->setWordWrap(true);
    m_dropLabel->setAlignment(Qt::AlignCenter);

    dropLayout->addWidget(m_dropLabel);
    
    m_mainLayout->addWidget(m_dropZone);
}

/**
 * @brief 添加一个新的收藏条目（图标 + 名称 + 物理路径关联）
 */
void FavoritesPanel::addFavoriteItem(const QIcon& icon, const QString& name, const QString& path) {
    QListWidgetItem* item = new QListWidgetItem(icon, name, m_listWidget);
    item->setData(Qt::UserRole, path); // 将物理路径存储在 UserRole 中
    m_listWidget->addItem(item);
}

/**
 * @brief 处理拖拽进入事件
 */
void FavoritesPanel::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls() || event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist")) {
        event->acceptProposedAction();
    }
}

/**
 * @brief 处理拖拽放下事件（接收外部资源管理器或内部条目）
 */
void FavoritesPanel::dropEvent(QDropEvent* event) {
    const QMimeData* mime = event->mimeData();
    
    // 外部拖拽：从 Windows 资源管理器处理 text/uri-list
    if (mime->hasUrls()) {
        QList<QUrl> urls = mime->urls();
        for (const QUrl& url : urls) {
            QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                QFileInfo info(path);
                QFileIconProvider provider;
                addFavoriteItem(provider.icon(info), info.fileName(), path);
            }
        }
        event->acceptProposedAction();
    }
}

/**
 * @brief 处理点击事件，发出选定信号
 */
void FavoritesPanel::onItemClicked(QListWidgetItem* item) {
    QString path = item->data(Qt::UserRole).toString();
    emit favoriteSelected(path);
}

/**
 * @brief 处理收藏项右键菜单
 */
void FavoritesPanel::onCustomContextMenuRequested(const QPoint& pos) {
    QListWidgetItem* item = m_listWidget->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #2B2B2B; border: 1px solid #444444; color: #EEEEEE; padding: 4px; }"
        "QMenu::item { height: 26px; padding: 0 24px 0 24px; border-radius: 4px; font-size: 12px; }"
        "QMenu::item:selected { background-color: #378ADD; }"
    );

    QAction* removeAct = menu.addAction("从收藏夹移除");
    
    QAction* selectedAction = menu.exec(m_listWidget->viewport()->mapToGlobal(pos));
    if (selectedAction == removeAct) {
        delete m_listWidget->takeItem(m_listWidget->row(item));
    }
}

} // namespace ArcMeta
