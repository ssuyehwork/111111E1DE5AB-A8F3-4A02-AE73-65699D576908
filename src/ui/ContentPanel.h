#ifndef CONTENT_PANEL_H
#define CONTENT_PANEL_H

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QVBoxLayout>
#include <QStringListModel>
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

private slots:
    void onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

private:
    void initListView();

    QTreeView* m_treeView;
    QFileSystemModel* m_model;
    QStringListModel* m_searchResultModel;
};

} // namespace ArcMeta

#endif // CONTENT_PANEL_H
