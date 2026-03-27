#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QWidget>
#include <QToolBar>
#include <QLineEdit>
#include <QPushButton>
#include "NavPanel.h"
#include "ContentPanel.h"
#include "CategoryPanel.h"
#include "MetaPanel.h"
#include "FavoritesPanel.h"
#include "FilterPanel.h"
#include "HeaderBar.h"
#include "ToolTipOverlay.h"

namespace ArcMeta {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void initLayout();
    void initToolBar();

    HeaderBar* m_headerBar;

    // 六大面板
    CategoryPanel* m_categoryPanel;
    NavPanel* m_navPanel;
    FavoritesPanel* m_favoritesPanel;
    ContentPanel* m_contentPanel;
    MetaPanel* m_metaPanel;
    FilterPanel* m_filterPanel;

    QSplitter* m_mainSplitter;

    // 工具栏组件
    QLineEdit* m_pathEdit;
    QLineEdit* m_searchEdit;
};

} // namespace ArcMeta

#endif // MAIN_WINDOW_H
