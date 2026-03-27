#ifndef FAVORITES_PANEL_H
#define FAVORITES_PANEL_H

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>

namespace ArcMeta {

class FavoritesPanel : public QWidget {
    Q_OBJECT
public:
    explicit FavoritesPanel(QWidget* parent = nullptr);

private:
    void initList();
    void initDropArea();

    QVBoxLayout* m_mainLayout;
    QListWidget* m_listWidget;
    QLabel* m_dropHint;
};

} // namespace ArcMeta

#endif // FAVORITES_PANEL_H
