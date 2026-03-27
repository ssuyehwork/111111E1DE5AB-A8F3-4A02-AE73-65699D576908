#include "MainWindow.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

namespace ArcMeta {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("ArcMeta");
    resize(1600, 900);
    setMinimumSize(1280, 720);

    initToolBar();
    initLayout();
}

MainWindow::~MainWindow() {}

void MainWindow::initToolBar() {
    QToolBar* toolbar = addToolBar("MainToolbar");
    toolbar->setFixedHeight(40);
    toolbar->setMovable(false);
    toolbar->setFloatable(false);

    // 设置内边距和间距（通过样式表或布局，这里先设基础样式）
    toolbar->setStyleSheet("QToolBar { spacing: 8px; padding: 0 12px; border-bottom: 1px solid #333333; }");

    // 1. 导航按钮
    QPushButton* btnBack = new QPushButton("后退");
    btnBack->setFixedSize(60, 28);
    QPushButton* btnForward = new QPushButton("前进");
    btnForward->setFixedSize(60, 28);
    QPushButton* btnUp = new QPushButton("上级");
    btnUp->setFixedSize(60, 28);

    toolbar->addWidget(btnBack);
    toolbar->addWidget(btnForward);
    toolbar->addWidget(btnUp);

    // 2. 路径输入框（弹性）
    m_pathEdit = new QLineEdit();
    m_pathEdit->setMinimumWidth(200);
    m_pathEdit->setPlaceholderText("输入路径...");
    toolbar->addWidget(m_pathEdit);

    // 3. 搜索框（固定宽度）
    m_searchEdit = new QLineEdit();
    m_searchEdit->setFixedWidth(200);
    m_searchEdit->setPlaceholderText("在当前目录搜索...");
    toolbar->addWidget(m_searchEdit);

    // 4. 视图切换（占位）
    QPushButton* btnViewMode = new QPushButton("视图");
    btnViewMode->setFixedSize(40, 28);
    toolbar->addWidget(btnViewMode);
}

void MainWindow::initLayout() {
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setHandleWidth(3); // 规范要求 3px
    m_mainSplitter->setStyleSheet("QSplitter::handle { background-color: #333333; }");

    auto createPanel = [](const QString& name, int minWidth) {
        QWidget* panel = new QWidget();
        panel->setMinimumWidth(minWidth);
        panel->setObjectName(name);
        // 临时背景色以便于调试布局
        panel->setAutoFillBackground(true);
        return panel;
    };

    // 按顺序创建六大面板
    m_categoryPanel = createPanel("CategoryPanel", 200);
    m_navPanel = new NavPanel(this);
    m_navPanel->setMinimumWidth(200);
    m_favoritesPanel = createPanel("FavoritesPanel", 200);
    m_contentPanel = new ContentPanel(this);
    m_contentPanel->setMinimumWidth(200);
    m_metaPanel = createPanel("MetaPanel", 200);
    m_filterPanel = createPanel("FilterPanel", 200);

    m_mainSplitter->addWidget(m_categoryPanel);
    m_mainSplitter->addWidget(m_navPanel);
    m_mainSplitter->addWidget(m_favoritesPanel);
    m_mainSplitter->addWidget(m_contentPanel);
    m_mainSplitter->addWidget(m_metaPanel);

    // 建立导航联动
    connect(m_navPanel, &NavPanel::directorySelected, m_contentPanel, &ContentPanel::setRootPath);
    connect(m_navPanel, &NavPanel::directorySelected, [this](const QString& path) {
        m_pathEdit->setText(path);
    });
    m_mainSplitter->addWidget(m_filterPanel);

    // 设置初始宽度分配（合计 1600px）
    // 分类:230, 导航:200, 收藏:200, 内容:弹性, 元数据:240, 筛选:230
    // 剩余给内容面板 = 1600 - (230+200+200+240+230) - 5*3 = 485
    QList<int> sizes;
    sizes << 230 << 200 << 200 << 485 << 240 << 230;
    m_mainSplitter->setSizes(sizes);

    setCentralWidget(m_mainSplitter);
}

} // namespace ArcMeta
