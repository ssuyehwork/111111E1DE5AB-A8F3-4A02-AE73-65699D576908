#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QToolBar>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

namespace ArcMeta {

class CategoryPanel;
class NavPanel;
class FavoritesPanel;
class ContentPanel;
class MetaPanel;
class FilterPanel;

/**
 * @brief 主窗口类
 * 负责六栏布局的组装、QSplitter 管理及自定义标题栏按钮
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private:
    void initUi();
    void initToolbar();
    void setupSplitters();
    void setupCustomTitleBarButtons();

    // 六个面板
    CategoryPanel* m_categoryPanel = nullptr;
    NavPanel* m_navPanel = nullptr;
    FavoritesPanel* m_favoritesPanel = nullptr;
    ContentPanel* m_contentPanel = nullptr;
    MetaPanel* m_metaPanel = nullptr;
    FilterPanel* m_filterPanel = nullptr;

    QSplitter* m_mainSplitter = nullptr;

    // 工具栏组件
    QToolBar* m_toolbar = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_btnBack = nullptr;
    QPushButton* m_btnForward = nullptr;
    QPushButton* m_btnUp = nullptr;
    
    // 标题栏按钮组 (用于 frameless 时的模拟，此处作为标准按钮展示)
    QPushButton* m_btnPinTop = nullptr;
    QPushButton* m_btnMin = nullptr;
    QPushButton* m_btnMax = nullptr;
    QPushButton* m_btnClose = nullptr;
};

} // namespace ArcMeta
