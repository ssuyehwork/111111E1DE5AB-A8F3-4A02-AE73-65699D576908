#ifndef CATEGORY_PANEL_H
#define CATEGORY_PANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <QTreeView>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QMenu>
#include "CategoryModel.h"

namespace ArcMeta {

class CategoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit CategoryPanel(QWidget* parent = nullptr);

signals:
    void categorySelected(int categoryId);

private slots:
    void showContextMenu(const QPoint& pos);
    void onAddCategory();
    void onRenameCategory();
    void onDeleteCategory();

private:
    void initStatisticsArea();
    void initCategoryTree();
    void initBottomToolbar();

    QVBoxLayout* m_mainLayout;
    QListWidget* m_statsList;
    QTreeView* m_categoryTree;
    CategoryModel* m_model;
    QLineEdit* m_searchEdit;
};

} // namespace ArcMeta

#endif // CATEGORY_PANEL_H
