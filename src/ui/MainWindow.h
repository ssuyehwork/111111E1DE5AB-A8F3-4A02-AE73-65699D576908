#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QToolBar>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QHBoxLayout>

namespace ArcMeta {

class BreadcrumbBar;
class CategoryPanel;
class NavPanel;
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

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private slots:
    void onPinToggled(bool checked);
    void toggleMaximize();
    void onBackClicked();
    void onForwardClicked();
    void onUpClicked();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void initUi();
    void initBaseLayout();
    void updateNavButtons();
    void updateStatusBar();
    void navigateTo(const QString& path, bool record = true);
    void initToolbar();
    void setupSplitters();
    void setupCustomTitleBarButtons();

    // 标准架构图层 (Memories.md)
    QVBoxLayout* m_outerLayout = nullptr;
    QFrame* m_container = nullptr;
    QVBoxLayout* m_mainLayout = nullptr;
    QWidget* m_titleBar = nullptr;
    QWidget* m_contentArea = nullptr;

    // 面包屑地址栏
    BreadcrumbBar* m_breadcrumbBar = nullptr;
    QStackedWidget* m_pathStack = nullptr;

    // 六个面板
    CategoryPanel* m_categoryPanel = nullptr;
    NavPanel* m_navPanel = nullptr;
    ContentPanel* m_contentPanel = nullptr;
    MetaPanel* m_metaPanel = nullptr;
    FilterPanel* m_filterPanel = nullptr;

    QSplitter* m_mainSplitter = nullptr;
    QHBoxLayout* m_titleLayout = nullptr;

    // 工具栏组件
    QLineEdit* m_pathEdit = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_btnBack = nullptr;
    QPushButton* m_btnForward = nullptr;
    QPushButton* m_btnUp = nullptr;
    
    // 标题栏按钮组
    QPushButton* m_btnCreate = nullptr;
    QPushButton* m_btnPin = nullptr;
    QPushButton* m_minBtn = nullptr;
    QPushButton* m_maxBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    // 状态管理
    bool m_isPinned = false;
    QString m_currentPath;
    QStringList m_history;
    int m_historyIndex = -1;

    // 底部状态栏
    QLabel* m_statusLeft = nullptr;
    QLabel* m_statusCenter = nullptr;
    QLabel* m_statusRight = nullptr;

    // Windows 缩放辅助
    enum ResizeEdge {
        None = 0, Top, Bottom, Left, Right,
        TopLeft, TopRight, BottomLeft, BottomRight
    };
    ResizeEdge getEdge(const QPoint& pos);
};

} // namespace ArcMeta
