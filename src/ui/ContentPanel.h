#ifndef CONTENT_PANEL_H
#define CONTENT_PANEL_H

#include <QWidget>
#include <QTreeView>
#include <QListView>
#include <QFileSystemModel>
#include <QVBoxLayout>
#include <QStringListModel>
#include <QStackedWidget>
#include "../mft/MftReader.h"

namespace ArcMeta {

class ContentPanel : public QWidget {
    Q_OBJECT
public:
    explicit ContentPanel(QWidget* parent = nullptr);

signals:
    // 当内容面板中的文件或文件夹被选中时触发
    void itemSelected(const QString& filePath);

public slots:
    // 切换到指定目录并显示内容
    void setRootPath(const QString& path);

    // 执行全盘并行搜索
    void performSearch(const FileIndex& index, const QString& query);

    // 显示指定分类下的文件
    void showCategory(const FileIndex& index, int categoryId);

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
    QStringListModel* m_searchResultModel;
};

} // namespace ArcMeta

#endif // CONTENT_PANEL_H
