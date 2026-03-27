#ifndef NAV_PANEL_H
#define NAV_PANEL_H

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QVBoxLayout>

namespace ArcMeta {

class NavPanel : public QWidget {
    Q_OBJECT
public:
    explicit NavPanel(QWidget* parent = nullptr);

signals:
    // 当用户在树中选中某个目录时触发
    void directorySelected(const QString& path);

private slots:
    void onItemClicked(const QModelIndex& index);

private:
    QTreeView* m_treeView;
    QFileSystemModel* m_model;
};

} // namespace ArcMeta

#endif // NAV_PANEL_H
