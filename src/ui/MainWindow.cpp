#include "MainWindow.h"
#include <QSplitter>
#include <QHBoxLayout>

namespace ArcMeta {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("ArcMeta - 原生图标增强版");
    resize(1600, 900);
    setMinimumSize(1280, 720);

    initLayout();
}

MainWindow::~MainWindow() {}

void MainWindow::initLayout() {
    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    m_navPanel = new NavPanel(splitter);
    m_contentPanel = new ContentPanel(splitter);

    splitter->addWidget(m_navPanel);
    splitter->addWidget(m_contentPanel);

    m_navPanel->setMinimumWidth(250);
    m_contentPanel->setMinimumWidth(500);

    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);
    setCentralWidget(centralWidget);
}

} // namespace ArcMeta
