#include "MainWindow.h"
#include "IconHelper.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QEvent>
#include <QHelpEvent>
#include <QShortcut>
#include <QApplication>
#include "../db/ConfigRepo.h"

namespace ArcMeta {

MainWindow::MainWindow(FileIndex& index, QWidget* parent)
    : QMainWindow(parent), m_index(index) {
    setWindowTitle("ArcMeta");

    resize(1600, 900);
    setMinimumSize(1280, 720);

    qApp->installEventFilter(this);

    m_headerBar = new HeaderBar(this);

    initToolBar();
    initLayout();

    m_quickLook = new QuickLook(this);

    new QShortcut(QKeySequence("F2"), this, [](){ });
    new QShortcut(QKeySequence("Space"), this, [this](){
        m_quickLook->preview(m_pathEdit->text());
    });
    new QShortcut(QKeySequence("Ctrl+L"), this, [this](){ m_pathEdit->setFocus(); });
    new QShortcut(QKeySequence("Ctrl+F"), this, [this](){ m_searchEdit->setFocus(); });

    QWidget* central = new QWidget(this);
    QVBoxLayout* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    centralLayout->addWidget(m_headerBar);
    centralLayout->addWidget(m_mainSplitter, 1);

    m_scratchPad = new ScratchPad(this);
    m_scratchPad->setFixedHeight(120);
    centralLayout->addWidget(m_scratchPad);

    QFrame* bottomLine = new QFrame(this);
    bottomLine->setFrameShape(QFrame::HLine);
    bottomLine->setFixedHeight(1);
    bottomLine->setStyleSheet("background-color: #333333; border: none;");
    centralLayout->addWidget(bottomLine);

    setCentralWidget(central);
    setWindowFlags(Qt::FramelessWindowHint);
}

MainWindow::~MainWindow() {}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::ToolTip) {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
        QString text = watched->property("toolTip").toString();
        if (text.isEmpty()) {
            if (QWidget* w = qobject_cast<QWidget*>(watched)) {
                text = w->toolTip();
            }
        }

        if (!text.isEmpty()) {
            int timeout = (watched->parent() == m_headerBar) ? 2000 : 700;
            ToolTipOverlay::instance().showTip(helpEvent->globalPos(), text, timeout);
        }
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::initToolBar() {
    QToolBar* toolbar = addToolBar("MainToolbar");
    toolbar->setFixedHeight(40);
    toolbar->setMovable(false);
    toolbar->setFloatable(false);

    toolbar->setStyleSheet("QToolBar { spacing: 8px; padding: 0 12px; border-bottom: 1px solid #333333; }");

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

    m_pathEdit = new QLineEdit();
    m_pathEdit->setMinimumWidth(200);
    m_pathEdit->setPlaceholderText("输入路径...");
    toolbar->addWidget(m_pathEdit);

    m_searchEdit = new QLineEdit();
    m_searchEdit->setFixedWidth(200);
    m_searchEdit->setPlaceholderText("全盘并行搜索...");
    toolbar->addWidget(m_searchEdit);

    QPushButton* btnViewMode = new QPushButton("视图");
    btnViewMode->setFixedSize(40, 28);
    connect(btnViewMode, &QPushButton::clicked, [this]() {
        static bool isGrid = true;
        isGrid = !isGrid;
        m_contentPanel->setViewMode(isGrid);
    });
    toolbar->addWidget(btnViewMode);
}

void MainWindow::initLayout() {
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setHandleWidth(3);
    m_mainSplitter->setStyleSheet("QSplitter::handle { background-color: #333333; }");

    m_categoryPanel = new CategoryPanel(this);
    m_navPanel = new NavPanel(this);
    m_favoritesPanel = new FavoritesPanel(this);
    m_contentPanel = new ContentPanel(this);
    m_metaPanel = new MetaPanel(this);
    m_filterPanel = new FilterPanel(this);

    m_categoryPanel->setMinimumWidth(200);
    m_navPanel->setMinimumWidth(200);
    m_favoritesPanel->setMinimumWidth(200);
    m_contentPanel->setMinimumWidth(200);
    m_metaPanel->setMinimumWidth(200);
    m_filterPanel->setMinimumWidth(200);

    m_mainSplitter->addWidget(m_categoryPanel);
    m_mainSplitter->addWidget(m_navPanel);
    m_mainSplitter->addWidget(m_favoritesPanel);
    m_mainSplitter->addWidget(m_contentPanel);
    m_mainSplitter->addWidget(m_metaPanel);
    m_mainSplitter->addWidget(m_filterPanel);

    connect(m_navPanel, &NavPanel::directorySelected, m_contentPanel, &ContentPanel::setRootPath);
    connect(m_navPanel, &NavPanel::directorySelected, [this](const QString& path) {
        m_pathEdit->setText(path);
    });

    connect(m_categoryPanel, &CategoryPanel::categorySelected, [this](int id) {
        m_contentPanel->showCategory(m_index, id);
    });

    connect(m_contentPanel, &ContentPanel::itemSelected, m_metaPanel, &MetaPanel::setTargetFile);
    connect(m_contentPanel, &ContentPanel::itemSelected, [this](const QString& path) {
        m_pathEdit->setText(path);
    });

    connect(m_searchEdit, &QLineEdit::textChanged, [this](const QString& text) {
        m_contentPanel->performSearch(m_index, text);
    });

    connect(m_filterPanel, &FilterPanel::filterChanged, m_contentPanel, &ContentPanel::applyFilter);

    // 建立元数据修改后的实时联动信号
    connect(m_metaPanel, &MetaPanel::metadataUpdated, m_contentPanel, &ContentPanel::refreshItem);

    QList<int> sizes = ConfigRepo::loadPanelWidths();
    if (sizes.isEmpty()) {
        sizes << 230 << 200 << 200 << 485 << 240 << 230;
    }
    m_mainSplitter->setSizes(sizes);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    ConfigRepo::savePanelWidths(m_mainSplitter->sizes());
    QMainWindow::closeEvent(event);
}

} // namespace ArcMeta
