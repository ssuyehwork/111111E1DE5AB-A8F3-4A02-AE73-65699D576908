#pragma once

#include <QWidget>
#include <QListView>
#include <QTreeView>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QStyledItemDelegate>

namespace ArcMeta {

/**
 * @brief 内容面板（面板四）：核心业务展示区
 * 支持网格视图（QListView）与列表视图（QTreeView）切换
 */
class ContentPanel : public QWidget {
    Q_OBJECT

public:
    enum ViewMode {
        GridView,
        ListView
    };

    explicit ContentPanel(QWidget* parent = nullptr);
    ~ContentPanel() override = default;

    /**
     * @brief 切换视图模式
     */
    void setViewMode(ViewMode mode);

    /**
     * @brief 拦截空格键（红线：物理拦截 QEvent::KeyPress 且为 Key_Space）
     */
    bool eventFilter(QObject* obj, QEvent* event) override;

    /**
     * @brief 获取渲染后的图标
     */
    static QIcon getSvgIcon(const QString& key, const QColor& color);

signals:
    /**
     * @brief 请求 QuickLook 预览信号
     * @param path 物理路径
     */
    void requestQuickLook(const QString& path);

    /**
     * @brief 选中项发生变化时通知元数据面板刷新
     * @param paths 选中条目的物理路径列表
     */
    void selectionChanged(const QStringList& paths);

    /**
     * @brief 请求切换到指定目录
     */
    void directorySelected(const QString& path);

    /**
     * @brief 加载并显示目录内容
     */
    void loadDirectory(const QString& path);

    /**
     * @brief 全局/本地搜索
     */
    void search(const QString& query);

    /**
     * @brief 应用当前筛选器
     */
    void applyFilters();

private:
    void initUi();
    void initGridView();
    void initListView();
    void setupContextMenu();

    QVBoxLayout* m_mainLayout = nullptr;
    QStackedWidget* m_viewStack = nullptr;

    // 视图组件
    QListView* m_gridView = nullptr;
    QTreeView* m_treeView = nullptr;
    QStandardItemModel* m_model = nullptr;

public slots:
    void onSelectionChanged();
    void onCustomContextMenuRequested(const QPoint& pos);
    void onDoubleClicked(const QModelIndex& index);
};

/**
 * @brief 自定义 Delegate：处理网格视图下的图标、星级、颜色圆点及角标叠加
 */
class GridItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

} // namespace ArcMeta
