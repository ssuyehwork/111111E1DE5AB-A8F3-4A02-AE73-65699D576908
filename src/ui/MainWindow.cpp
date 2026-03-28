#include "MainWindow.h"
#include "CategoryPanel.h"
#include "NavPanel.h"
#include "FavoritesPanel.h"
#include "ContentPanel.h"
#include "MetaPanel.h"
#include "FilterPanel.h"
#include "QuickLookWindow.h"
#include "../../SvgIcons.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSvgRenderer>
#include <QPainter>
#include <QIcon>

namespace ArcMeta {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    resize(1600, 900);
    setMinimumSize(1280, 720);
    setWindowTitle("ArcMeta");
    setStyleSheet("QMainWindow { background-color: #1A1A1A; }");

    initUi();
}

void MainWindow::initUi() {
    initToolbar();
    setupSplitters();
    setupCustomTitleBarButtons();
    
    // 设置默认权重分配: 230 | 200 | 200 | 弹性 | 240 | 230
    QList<int> sizes;
    sizes << 230 << 200 << 200 << 500 << 240 << 230;
    m_mainSplitter->setSizes(sizes);

    // 核心红线：建立各面板间的信号联动 (Data Linkage)
    
    // 1. 导航/收藏选择 -> 内容面板刷新
    connect(m_navPanel, &NavPanel::directorySelected, [this](const QString& path) {
        m_pathEdit->setText(path);
        m_contentPanel->loadDirectory(path);
    });
    
    connect(m_favoritesPanel, &FavoritesPanel::favoriteSelected, [this](const QString& path) {
        m_pathEdit->setText(path);
    });

    // 2. 内容面板选中项改变 -> 元数据面板刷新
    connect(m_contentPanel, &ContentPanel::selectionChanged, [this](const QStringList& paths) {
        if (paths.isEmpty()) {
            m_metaPanel->updateInfo("-", "-", "-", "-", "-", "-", "-", false);
        } else {
            QFileInfo info(paths.first());
            m_metaPanel->updateInfo(
                info.fileName(), 
                info.isDir() ? "文件夹" : info.suffix().toUpper() + " 文件",
                info.isDir() ? "-" : QString::number(info.size() / 1024) + " KB",
                info.birthTime().toString("yyyy-MM-dd"),
                info.lastModified().toString("yyyy-MM-dd"),
                info.lastRead().toString("yyyy-MM-dd"),
                info.absoluteFilePath(),
                false
            );
        }
    });

    // 3. 内容面板请求预览 -> QuickLook
    connect(m_contentPanel, &ContentPanel::requestQuickLook, [](const QString& path) {
        QuickLookWindow::instance().previewFile(path);
    });

    // 4. QuickLook 打标 -> 元数据面板同步
    connect(&QuickLookWindow::instance(), &QuickLookWindow::ratingRequested, [this](int rating) {
        m_metaPanel->setRating(rating);
    });

    // 5. 筛选面板变更 -> 内容面板过滤
    connect(m_filterPanel, &FilterPanel::filterChanged, [this]() {
        m_contentPanel->applyFilters();
    });

    // 6. 工具栏路径跳转
    connect(m_pathEdit, &QLineEdit::returnPressed, [this]() {
        QString path = m_pathEdit->text();
        m_contentPanel->loadDirectory(path);
    });

    // 7. 工具栏极速搜索对接 (MFT 并行引擎)
    connect(m_searchEdit, &QLineEdit::textEdited, [this](const QString& text) {
        if (text.isEmpty()) {
            m_contentPanel->loadDirectory(m_pathEdit->text());
        } else if (text.length() >= 2) {
            m_contentPanel->search(text);
        }
    });
}

void MainWindow::initToolbar() {
    m_toolbar = addToolBar("MainToolbar");
    m_toolbar->setFixedHeight(40);
    m_toolbar->setMovable(false);
    m_toolbar->setStyleSheet("QToolBar { background-color: #252525; border: none; padding-left: 12px; padding-right: 12px; spacing: 8px; }");

    auto createBtn = [this](const QString& iconKey, const QString& tip) {
        QPushButton* btn = new QPushButton(this);
        btn->setFixedSize(60, 28);
        QString svg = SvgIcons::icons[iconKey];
        svg.replace("currentColor", "#EEEEEE");
        QSvgRenderer renderer(svg.toUtf8());
        QPixmap pix(18, 18); pix.fill(Qt::transparent);
        QPainter p(&pix); renderer.render(&p);
        btn->setIcon(QIcon(pix));
        btn->setToolTip(tip);
        btn->setStyleSheet("QPushButton { background: #333333; border: 1px solid #444444; border-radius: 4px; } QPushButton:hover { background: #444444; }");
        return btn;
    };

    m_btnBack = createBtn("nav_prev", "后退");
    m_btnForward = createBtn("nav_next", "前进");
    m_btnUp = createBtn("arrow_up", "上级");

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText("输入路径...");
    m_pathEdit->setMinimumWidth(200);
    m_pathEdit->setFixedHeight(28);
    m_pathEdit->setStyleSheet("QLineEdit { background: #1A1A1A; border: 1px solid #444444; border-radius: 4px; color: #EEEEEE; padding-left: 8px; }");

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("过滤内容...");
    m_searchEdit->setFixedWidth(200);
    m_searchEdit->setFixedHeight(28);
    m_searchEdit->setStyleSheet("QLineEdit { background: #1A1A1A; border: 1px solid #444444; border-radius: 4px; color: #EEEEEE; padding-left: 8px; }");

    m_toolbar->addWidget(m_btnBack);
    m_toolbar->addWidget(m_btnForward);
    m_toolbar->addWidget(m_btnUp);
    m_toolbar->addWidget(m_pathEdit);
    m_toolbar->addWidget(m_searchEdit);
}

void MainWindow::setupSplitters() {
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setHandleWidth(3);
    m_mainSplitter->setStyleSheet("QSplitter::handle { background-color: #333333; }");

    m_categoryPanel = new CategoryPanel(this);
    m_navPanel = new NavPanel(this);
    m_favoritesPanel = new FavoritesPanel(this);
    m_contentPanel = new ContentPanel(this);
    m_metaPanel = new MetaPanel(this);
    m_filterPanel = new FilterPanel(this);

    m_mainSplitter->addWidget(m_categoryPanel);
    m_mainSplitter->addWidget(m_navPanel);
    m_mainSplitter->addWidget(m_favoritesPanel);
    m_mainSplitter->addWidget(m_contentPanel);
    m_mainSplitter->addWidget(m_metaPanel);
    m_mainSplitter->addWidget(m_filterPanel);

    setCentralWidget(m_mainSplitter);
}

/**
 * @brief 实现符合 funcBtnStyle 规范的自定义按钮组
 */
void MainWindow::setupCustomTitleBarButtons() {
    QWidget* titleBarBtns = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(titleBarBtns);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto createTitleBtn = [this](const QString& iconKey, const QString& hoverColor = "rgba(255, 255, 255, 0.1)") {
        QPushButton* btn = new QPushButton(this);
        btn->setFixedSize(24, 24); // 固定 24x24px
        
        QString svg = SvgIcons::icons[iconKey];
        svg.replace("currentColor", "#EEEEEE");
        QSvgRenderer renderer(svg.toUtf8());
        QPixmap pix(18, 18); pix.fill(Qt::transparent); // 图标 18x18px
        QPainter p(&pix); renderer.render(&p);
        
        btn->setIcon(QIcon(pix));
        btn->setStyleSheet(QString(
            "QPushButton { background: transparent; border: none; border-radius: 4px; padding: 0; }"
            "QPushButton:hover { background: %1; }"
            "QPushButton:pressed { background: rgba(255, 255, 255, 0.2); }"
        ).arg(hoverColor));
        return btn;
    };

    m_btnPinTop = createTitleBtn("pin");
    m_btnMin = createTitleBtn("minimize");
    m_btnMax = createTitleBtn("maximize");
    m_btnClose = createTitleBtn("close", "#e81123"); // 关闭按钮悬停红色

    layout->addWidget(m_btnPinTop);
    layout->addWidget(m_btnMin);
    layout->addWidget(m_btnMax);
    layout->addWidget(m_btnClose);

    // 将按钮组添加到工具栏最右侧 (QToolBar 不支持 addStretch，改用弹簧 Widget)
    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);
    m_toolbar->addWidget(titleBarBtns);

    // 逻辑：置顶切换
    connect(m_btnPinTop, &QPushButton::clicked, [this]() {
        bool isTop = (windowFlags() & Qt::WindowStaysOnTopHint);
        setWindowFlags(isTop ? (windowFlags() & ~Qt::WindowStaysOnTopHint) : (windowFlags() | Qt::WindowStaysOnTopHint));
        show(); // 改变 flag 后需重新 show
    });
}

} // namespace ArcMeta
