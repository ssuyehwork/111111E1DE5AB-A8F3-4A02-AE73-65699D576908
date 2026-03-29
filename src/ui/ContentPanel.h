#pragma once

#include <QWidget>
#include <QListView>
#include <QTreeView>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>
#include <QStyledItemDelegate>
#include <QMap>
#include <unordered_map>
#include <map>
#include <string>
#include "FilterPanel.h"
#include "../meta/AmMetaJson.h"

namespace ArcMeta {

/**
 * @brief 自定义 Role 枚举，用于 QStandardItemModel 数据存取
 */
enum ItemRole {
    RatingRole = Qt::UserRole + 1,
    ColorRole,
    EncryptedRole,
    PathRole,
    IsLockedRole, // 对应置顶/Lock 状态
    TagsRole,
    TypeRole,
    SizeRawRole,
    MTimeRawRole,
    CTimeRawRole,
    IsDirRole,
    IsDriveRole
};

/**
 * @brief 极致性能：轻量化享元条目结构
 */
struct FileItem {
    QString name;
    QString fullPath;
    QString extension;
    bool isDir = false;
    bool isDrive = false;

    // 系统属性 (从 DB 预取)
    long long size = 0;
    double mtime = 0;
    double ctime = 0;
    double atime = 0;

    // 元数据 (从 DB 预取)
    int rating = 0;
    QString color;
    bool pinned = false;
    bool encrypted = false;
    QStringList tags;

    // 渲染字符串缓存 (预计算)
    QString sizeStr;
    QString timeStr;
    QString typeStr;
};

/**
 * @brief 极致性能：自定义享元模型，消除 QStandardItem 堆分配开销
 */
class FileModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit FileModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setItems(const QList<FileItem>& items);
    const QList<FileItem>& items() const { return m_items; }

private:
    QList<FileItem> m_items;
    mutable QMap<QString, QIcon> m_iconCache;
};

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

    // --- 业务接口 ---
    QAbstractItemModel* model() const { return m_model; }
    QSortFilterProxyModel* getProxyModel() const { return m_proxyModel; }
    QModelIndexList getSelectedIndexes() const {
        return (m_viewStack->currentWidget() == m_gridView) ? 
                m_gridView->selectionModel()->selectedIndexes() : 
                m_treeView->selectionModel()->selectedIndexes();
    }

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
    void directorySelected(const QString& path);

    /**
     * @brief 目录装载完成后发出，携带统计数据供 FilterPanel 填充
     */
    void directoryStatsReady(
        const QMap<int, int>&     ratingCounts,
        const QMap<QString, int>& colorCounts,
        const QMap<QString, int>& tagCounts,
        const QMap<QString, int>& typeCounts,
        const QMap<QString, int>& createDateCounts,
        const QMap<QString, int>& modifyDateCounts);

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
    FileModel* m_model = nullptr; // 2026-03-xx 极致性能重构：切换至自定义享元模型
    QSortFilterProxyModel* m_proxyModel = nullptr;

    // 2026-03-xx 极致性能：搜索专用元数据缓存池 (多目录分批)
    std::unordered_map<std::wstring, std::map<std::wstring, ItemMeta>> m_searchMetaCache;

    FilterState m_currentFilter;

    int m_zoomLevel = 64;
    QString m_currentPath;
    void updateGridSize();

    void addItemsFromDirectory(const QString& path, bool recursive,
                               QMap<int, int>& ratingCounts,
                               QMap<QString, int>& colorCounts,
                               QMap<QString, int>& tagCounts,
                               QMap<QString, int>& typeCounts,
                               QMap<QString, int>& createDateCounts,
                               QMap<QString, int>& modifyDateCounts,
                               int& noTagCount,
                               QList<FileItem>& outItems);

public slots:
    void onSelectionChanged();
    void onCustomContextMenuRequested(const QPoint& pos);
    void onDoubleClicked(const QModelIndex& index);

    /**
     * @brief 加载并显示目录内容
     */
    void loadDirectory(const QString& path, bool recursive = false);

    /**
     * @brief 全局/本地搜索
     */
    void search(const QString& query);

    /**
     * @brief 应用当前筛选器
     */
    void applyFilters(const FilterState& state);
    void applyFilters(); // 使用保存的状态重新应用

    /**
     * @brief 创建新条目（文件夹/Markdown/Txt）
     */
    void createNewItem(const QString& type);

protected:
    void wheelEvent(QWheelEvent* event) override;
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
    bool eventFilter(QObject* obj, QEvent* event) override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

} // namespace ArcMeta
