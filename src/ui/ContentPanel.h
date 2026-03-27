#ifndef CONTENT_PANEL_H
#define CONTENT_PANEL_H

#include <QWidget>
#include <QTreeView>
#include <QListView>
#include <QFileSystemModel>
#include <QVBoxLayout>
#include <QStringListModel>
#include <QStackedWidget>
#include <QSortFilterProxyModel>
#include "../mft/MftReader.h"
#include "FilterEngine.h"

namespace ArcMeta {

class FileFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    void setFilterState(const FilterState& state) {
        m_state = state;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        // 此处需要获取元数据进行匹配
        // 为了演示逻辑，暂定总是通过，实际需调用 FilterEngine::match
        return true;
    }

private:
    FilterState m_state;
};

class ContentPanel : public QWidget {
    Q_OBJECT
public:
    explicit ContentPanel(QWidget* parent = nullptr);

signals:
    void itemSelected(const QString& filePath);

public slots:
    void setRootPath(const QString& path);
    void performSearch(const FileIndex& index, const QString& query);
    void showCategory(const FileIndex& index, int categoryId);
    void applyFilter(const FilterState& state);

private slots:
    void onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

public slots:
    void setViewMode(bool isGrid);

private:
    void initListView();
    void initGridView();

    QStackedWidget* m_viewStack;
    QTreeView* m_treeView;
    QListView* m_gridView;
    QFileSystemModel* m_model;
    FileFilterProxyModel* m_proxyModel;
    QStringListModel* m_searchResultModel;
};

} // namespace ArcMeta

#endif // CONTENT_PANEL_H
