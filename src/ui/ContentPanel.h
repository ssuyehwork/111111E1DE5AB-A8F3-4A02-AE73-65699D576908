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
#include <QTimer>
#include <QFileInfoList>
#include "FilterPanel.h"

namespace ArcMeta {

/**
 * @brief 自定义 Role 枚举，用于 QStandardItemModel 数据存取
 */
enum ItemRole {
    RatingRole = Qt::UserRole + 1,
    ColorRole,
    PinnedRole,
    EncryptedRole,
    PathRole,
    IsLockedRole,
    TagsRole,
    TypeRole
};

/**
 * @brief 内容面板（面板四）：核心业务展示区
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

    void setViewMode(ViewMode mode);
    bool eventFilter(QObject* obj, QEvent* event) override;

    QAbstractItemModel* model() const { return m_model; }
    QSortFilterProxyModel* getProxyModel() const { return m_proxyModel; }
    QModelIndexList getSelectedIndexes() const {
        return (m_viewStack->currentWidget() == m_gridView) ? 
                m_gridView->selectionModel()->selectedIndexes() : 
                m_treeView->selectionModel()->selectedIndexes();
    }

signals:
    void requestQuickLook(const QString& path);
    void selectionChanged(const QStringList& paths);
    void directorySelected(const QString& path);
    void directoryStatsReady(
        const QMap<int, int>&     ratingCounts,
        const QMap<QString, int>& colorCounts,
        const QMap<QString, int>& tagCounts,
        const QMap<QString, int>& typeCounts,
        const QMap<QString, int>& createDateCounts,
        const QMap<QString, int>& modifyDateCounts);

private slots:
    void processLoadBatch(); // 分帧加载核心槽

private:
    void initUi();
    void initGridView();
    void initListView();

    QVBoxLayout* m_mainLayout = nullptr;
    QStackedWidget* m_viewStack = nullptr;
    QListView* m_gridView = nullptr;
    QTreeView* m_treeView = nullptr;
    QStandardItemModel* m_model = nullptr;
    QSortFilterProxyModel* m_proxyModel = nullptr;

    FilterState m_currentFilter;
    int m_zoomLevel = 64;
    QString m_currentPath;
    void updateGridSize();

    // 异步加载管理
    QTimer* m_loadTimer = nullptr;
    QFileInfoList m_loadQueue;
    struct MftLoadEntry {
        std::wstring name;
        std::wstring volume;
        unsigned long long frn;
        bool isDir;
    };
    QList<MftLoadEntry> m_mftLoadQueue;
    bool m_isSearching = false;

    // 统计累加器
    QMap<int, int>     m_accRating;
    QMap<QString, int> m_accColor;
    QMap<QString, int> m_accTag;
    QMap<QString, int> m_accType;
    QMap<QString, int> m_accCreateDate;
    QMap<QString, int> m_accModifyDate;
    int m_accNoTagCount = 0;

    void addItemsToQueue(const QString& path, bool recursive);

public slots:
    void onSelectionChanged();
    void onCustomContextMenuRequested(const QPoint& pos);
    void onDoubleClicked(const QModelIndex& index);
    void loadDirectory(const QString& path, bool recursive = false);
    void search(const QString& query);
    void applyFilters(const FilterState& state);
    void applyFilters();
    void createNewItem(const QString& type);

protected:
    void wheelEvent(QWheelEvent* event) override;
};

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
