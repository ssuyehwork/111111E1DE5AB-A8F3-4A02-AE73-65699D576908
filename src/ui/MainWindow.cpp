#include "MainWindow.h"
#include "IconHelper.h"
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

    toolbar->setStyleSheet("QToolBar { spacing: 8px; padding: 0 12px; border-bottom: 1px solid #333333; }");

    // 1. 导航按钮
    QPushButton* btnBack = new QPushButton();
    btnBack->setIcon(IconHelper::getIcon("arrow_left"));
    btnBack->setFixedSize(60, 28);
    QPushButton* btnForward = new QPushButton();
    btnForward->setIcon(IconHelper::getIcon("arrow_right"));
    btnForward->setFixedSize(60, 28);
    QPushButton* btnUp = new QPushButton();
    btnUp->setIcon(IconHelper::getIcon("arrow_up"));
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

    // 4. 视图切换
    QPushButton* btnViewMode = new QPushButton("视图");
    btnViewMode->setFixedSize(40, 28);
    toolbar->addWidget(btnViewMode);
}

void MainWindow::initLayout() {
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setHandleWidth(3);
    m_mainSplitter->setStyleSheet("QSplitter::handle { background-color: #333333; }");

    // 实例化所有真实面板
    m_categoryPanel = new CategoryPanel(this);
    m_navPanel = new NavPanel(this);
    m_favoritesPanel = new FavoritesPanel(this);
    m_contentPanel = new ContentPanel(this);
    m_metaPanel = new MetaPanel(this);
    m_filterPanel = new FilterPanel(this);

    // 设置最小宽度
    m_categoryPanel->setMinimumWidth(200);
    m_navPanel->setMinimumWidth(200);
    m_favoritesPanel->setMinimumWidth(200);
    m_contentPanel->setMinimumWidth(200);
    m_metaPanel->setMinimumWidth(200);
    m_filterPanel->setMinimumWidth(200);

    // 添加到 Splitter
    m_mainSplitter->addWidget(m_categoryPanel);
    m_mainSplitter->addWidget(m_navPanel);
    m_mainSplitter->addWidget(m_favoritesPanel);
    m_mainSplitter->addWidget(m_contentPanel);
    m_mainSplitter->addWidget(m_metaPanel);
    m_mainSplitter->addWidget(m_filterPanel);

    // 建立导航联动
    connect(m_navPanel, &NavPanel::directorySelected, m_contentPanel, &ContentPanel::setRootPath);
    connect(m_navPanel, &NavPanel::directorySelected, [this](const QString& path) {
        m_pathEdit->setText(path);
    });

    // 建立内容与元数据联动
    connect(m_contentPanel, &ContentPanel::itemSelected, m_metaPanel, &MetaPanel::setTargetFile);

    // 设置初始宽度分配
    QList<int> sizes;
    sizes << 230 << 200 << 200 << 485 << 240 << 230;
    m_mainSplitter->setSizes(sizes);

    setCentralWidget(m_mainSplitter);
}

} // namespace ArcMeta
