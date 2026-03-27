#ifndef CONTENT_PANEL_H
#define CONTENT_PANEL_H

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QVBoxLayout>

namespace ArcMeta {

class ContentPanel : public QWidget {
    Q_OBJECT
public:
    explicit ContentPanel(QWidget* parent = nullptr);

public slots:
    // 切换到指定目录并显示内容
    void setRootPath(const QString& path);

private:
    void initListView();

    QTreeView* m_treeView;
    QFileSystemModel* m_model;
};

} // namespace ArcMeta

#endif // CONTENT_PANEL_H
