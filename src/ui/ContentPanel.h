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
#include "FilterPanel.h"

namespace ArcMeta {

/**
 * @brief 自定义 Role 枚举
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

class ContentPanel : public QWidget {
    Q_OBJECT

public:
    enum ViewMode { GridView, ListView };

    explicit ContentPanel(QWidget* parent = nullptr);
    ~ContentPanel() override = default;

    void setViewMode(ViewMode mode);
    bool eventFilter(QObject* obj, QEvent* event) override;

    QAbstractItemModel* model() const { return m_model; }
    QSortFilterProxyModel* getProxyModel() const { return m_proxyModel; }

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

public slots:
    void onSelectionChanged();
    void onCustomContextMenuRequested(const QPoint& pos);
    void onDoubleClicked(const QModelIndex& index);
    void loadDirectory(const QString& path, bool recursive = false);
    void search(const QString& query);
    void applyFilters(const FilterState& state);
    void applyFilters();
    void createNewItem(const QString& type);

private:
    void initUi();
    void initGridView();
    void initListView();
    void addItemsFromDirectory(const QString& path, bool recursive,
                               QMap<int, int>& r, QMap<QString, int>& c, QMap<QString, int>& t,
                               QMap<QString, int>& tp, QMap<QString, int>& cd, QMap<QString, int>& md, int& nt);

    // 2026-03-xx：新增私有处理函数
    void handleDeleteAction(const QString& path);

    QVBoxLayout* m_mainLayout = nullptr;
    QStackedWidget* m_viewStack = nullptr;
    QListView* m_gridView = nullptr;
    QTreeView* m_treeView = nullptr;
    QStandardItemModel* m_model = nullptr;
    QSortFilterProxyModel* m_proxyModel = nullptr;

    FilterState m_currentFilter;
    int m_zoomLevel = 96;
    QString m_currentPath;

    void updateGridSize();

protected:
    void wheelEvent(QWheelEvent* event) override;
};

class GridItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

} // namespace ArcMeta
