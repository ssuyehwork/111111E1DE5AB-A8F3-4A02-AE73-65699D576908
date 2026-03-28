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
#include "../meta/AmMetaJson.h"
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
        auto* fsModel = qobject_cast<QFileSystemModel*>(sourceModel());
        if (!fsModel) return true;

        QModelIndex index = fsModel->index(sourceRow, 0, sourceParent);
        QString path = fsModel->filePath(index);
        QString fileName = fsModel->fileName(index);
        QString dirPath = QFileInfo(path).absolutePath();

        // 排除元数据文件
        if (fileName == ".am_meta.json" || fileName == ".am_meta.json.tmp") return false;

        // 获取该文件的元数据进行匹配
        AmMeta meta = AmMetaJson::load(dirPath);
        int rating = 0;
        QString color;
        QStringList tags;
        bool pinned = false;
        bool encrypted = fileName.endsWith(".amenc");

        if (fsModel->isDir(index)) {
            rating = meta.folder.rating;
            color = meta.folder.color;
            tags = meta.folder.tags;
            pinned = meta.folder.pinned;
        } else if (meta.items.contains(fileName)) {
            ItemMetadata item = meta.items.value(fileName);
            rating = item.rating;
            color = item.color;
            tags = item.tags;
            pinned = item.pinned;
            encrypted = item.encrypted || encrypted;
        }

        return FilterEngine::match(m_state, rating, color, tags, pinned, encrypted);
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
    void refreshItem(const QString& filePath);

private slots:
    void onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void showContextMenu(const QPoint& pos);

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
