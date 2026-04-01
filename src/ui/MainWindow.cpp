#include "MainWindow.h"
#include "../core/CoreController.h"
#include "BreadcrumbBar.h"
#include "CategoryPanel.h"
#include "NavPanel.h"
#include "ContentPanel.h"
#include "MetaPanel.h"
#include "FilterPanel.h"
#include "QuickLookWindow.h"
#include "ToolTipOverlay.h"
#include "../../SvgIcons.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSvgRenderer>
#include <QPainter>
#include <QIcon>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCursor>
#include <QApplication>
#include <QSettings>
#include <QCloseEvent>
#include <QMenu>
#include <QAction>
#include "UiHelper.h"
#include <QFileInfo>
#include <QDir>
#include "../meta/AmMetaJson.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

namespace ArcMeta {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    resize(1600, 900);
    setMinimumSize(1200, 720);
    setWindowTitle("ArcMeta");
    setAttribute(Qt::WA_TranslucentBackground);

    // 从设置读取置顶状态
    QSettings settings("ArcMeta团队", "ArcMeta");
    m_isPinned = settings.value("MainWindow/AlwaysOnTop", false).toBool();
    
    // 设置基础窗口标志 (保持无边框)
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);

    if (m_isPinned) {
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
    }

    // 应用全局样式
    QString qss = R"(
        QMainWindow { background-color: transparent; }

        /* 核心架构样式 (Memories.md 规范) */
        QFrame#DialogContainer {
            background-color: #1e1e1e;
            border: 1px solid #333333;
            border-radius: 12px;
        }

        QWidget#TitleBar {
            background-color: #252526;
            border-bottom: 1px solid #333333;
            border-top-left-radius: 12px;
            border-top-right-radius: 12px;
        }

        #SidebarContainer, #ListContainer, #EditorContainer, #MetadataContainer, #FilterContainer {
            background-color: #1E1E1E;
            border-right: 1px solid #333333;
            border-radius: 0px;
        }
        #FilterContainer { border-right: none; }

        /* 全局滚动条美化 */
        QScrollBar:vertical { border: none; background: transparent; width: 4px; margin: 0px; }
        QScrollBar::handle:vertical { background: #333333; min-height: 20px; border-radius: 2px; }
        QScrollBar::handle:vertical:hover { background: #444444; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }

        QScrollBar:horizontal { border: none; background: transparent; height: 4px; margin: 0px; }
        QScrollBar::handle:horizontal { background: #333333; min-width: 20px; border-radius: 2px; }
        QScrollBar::handle:horizontal:hover { background: #444444; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }

        /* 规范对齐：QLineEdit 圆角 6px (AGENTS.md) */
        QLineEdit {
            background: #1A1A1A;
            border: 1px solid #333333;
            border-radius: 6px;
            color: #EEEEEE;
            padding-left: 8px;
        }
        QLineEdit:focus { border: 1px solid #378ADD; }

        /* 规范对齐：复选框指示器 */
        QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid #444; border-radius: 2px; background: #1A1A1A; }
        QCheckBox::indicator:checked {
            border: 1px solid #378ADD; background: #1A1A1A;
            image: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCIgZmlsbD0ibm9uZSIgc3Ryb2tlPSIjMzc4QUREIiBzdHJva2Utd2lkdGg9IjMuNSIgc3Ryb2tlLWxpbmVjYXA9InJvdW5kIiBzdHJva2UtbGluZWpvaW49InJvdW5kIj48cG9seWxpbmUgcG9pbnRzPSIyMCA2IDkgMTcgNCAxMiI+PC9wb2x5bGluZT48L3N2Zz4=);
        }
    )";
    setStyleSheet(qss);

    initUi();

    // 启动时和顶级目录均显示“此电脑”（磁盘分区列表）
    navigateTo("computer://");

    // 2026-03-xx 按照用户要求：启动后将定焦到“此电脑”导航项，而非搜索框
    // 使用 QTimer 确保在窗口完全显示后执行定焦，防止被其他组件抢占
    QTimer::singleShot(100, [this]() {
        m_navPanel->selectPath("computer://");
    });

}

void MainWindow::initUi() {
    initBaseLayout();
    initToolbar();
    setupSplitters();
    setupCustomTitleBarButtons();
    
    // 设置默认权重分配: 230 | 230 | 600 | 230 | 230
    QList<int> sizes;
    sizes << 230 << 230 << 600 << 230 << 230;
    m_mainSplitter->setSizes(sizes);

    // 核心红线：建立各面板间的信号联动 (Data Linkage)
    
    // 1. 导航/收藏/内容面板 双击跳转 -> 统一路径调度
    connect(m_navPanel, &NavPanel::directorySelected, this, [this](const QString& path) {
        navigateTo(path);
    });

    connect(m_contentPanel, &ContentPanel::directorySelected, this, [this](const QString& path) {
        navigateTo(path);
    });

    // 1a. 分类选择 -> 内容面板执行检索或筛选
    connect(m_categoryPanel, &CategoryPanel::categorySelected, [this](const QString& name) {
        m_pathStack->setCurrentWidget(m_pathEdit);
        m_pathEdit->setText("分类: " + name);
        m_contentPanel->search(name); 
    });

    // 2. 内容面板选中项改变 -> 元数据面板刷新
    // 2026-03-xx 按照高性能要求，优先从模型 Role 读取元数据缓存，避免频繁磁盘 IO
    connect(m_contentPanel, &ContentPanel::selectionChanged, [this](const QStringList& paths) {
        if (paths.isEmpty()) {
            m_metaPanel->updateInfo("-", "-", "-", "-", "-", "-", "-", false);
            m_metaPanel->setRating(0);
            m_metaPanel->setColor(L"");
            m_metaPanel->setPinned(false);
            m_metaPanel->setTags(QStringList());
            if (m_statusRight) m_statusRight->setText("");
        } else {
            // 2026-03-xx 高性能优化：优先从模型缓存中读取元数据，避免频繁磁盘访问
            auto indexes = m_contentPanel->getSelectedIndexes();
            if (indexes.isEmpty()) return;
            
            QModelIndex idx = indexes.first();
            QString path = paths.first();
            QFileInfo info(path);
            
            // 基础信息展示
            m_metaPanel->updateInfo(
                info.fileName().isEmpty() ? path : info.fileName(), 
                info.isDir() ? "文件夹" : info.suffix().toUpper() + " 文件",
                info.isDir() ? "-" : QString::number(info.size() / 1024) + " KB",
                info.birthTime().toString("yyyy-MM-dd"),
                info.lastModified().toString("yyyy-MM-dd"),
                info.lastRead().toString("yyyy-MM-dd"),
                info.absoluteFilePath(),
                idx.data(EncryptedRole).toBool()
            );

            // 应用缓存中的元数据状态
            m_metaPanel->setRating(idx.data(RatingRole).toInt());
            m_metaPanel->setColor(idx.data(ColorRole).toString().toStdWString());
            m_metaPanel->setPinned(idx.data(IsLockedRole).toBool());
            m_metaPanel->setTags(idx.data(TagsRole).toStringList());
        }
        // 状态栏右侧显示已选数量
        if (m_statusRight) {
            m_statusRight->setText(paths.isEmpty() ? "" : QString("已选 %1 个项目").arg(paths.size()));
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

    // 5a. 目录装载完成 -> FilterPanel 动态填充 (六参数版本)
    connect(m_contentPanel, &ContentPanel::directoryStatsReady,
        [this](const QMap<int,int>& r, const QMap<QString,int>& c,
               const QMap<QString,int>& t, const QMap<QString,int>& tp,
               const QMap<QString,int>& cd, const QMap<QString,int>& md) {
            m_filterPanel->populate(r, c, t, tp, cd, md);
        });

    // 5b. FilterPanel 勾选变化 -> 内容面板过滤
    connect(m_filterPanel, &FilterPanel::filterChanged, [this](const FilterState& state) {
        m_contentPanel->applyFilters(state);
        updateStatusBar(); // 筛选后立即更新底栏可见项目总数
    });

    // 6. 工具栏路径跳转
    connect(m_pathEdit, &QLineEdit::returnPressed, [this]() {
        QString input = m_pathEdit->text();
        if (QDir(input).exists()) {
            navigateTo(input);
        } else if (input == "computer://" || input == "此电脑") {
            navigateTo("computer://");
        } else {
            // 如果路径无效，恢复为当前实际路径
            m_pathEdit->setText(QDir::toNativeSeparators(m_currentPath));
            m_pathStack->setCurrentWidget(m_breadcrumbBar);
        }
    });

    // 7. 工具栏极速搜索对接 (MFT 并行引擎)
    connect(m_searchEdit, &QLineEdit::textEdited, [this](const QString& text) {
        if (text.isEmpty()) {
            m_contentPanel->loadDirectory(m_pathEdit->text());
        } else if (text.length() >= 2) {
            m_contentPanel->search(text);
        }
    });

    // 物理还原：焦点切换监听逻辑，驱动 1px 翠绿高亮线
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget* old, QWidget* now) {
        Q_UNUSED(old);
        // 重置所有面板高亮
        if (m_navPanel)      m_navPanel->setFocusHighlight(false);
        if (m_contentPanel)  m_contentPanel->setFocusHighlight(false);
        if (m_metaPanel)     m_metaPanel->setFocusHighlight(false);
        if (m_filterPanel)   m_filterPanel->setFocusHighlight(false);

        if (!now) return;

        // 递归查找焦点所属面板
        QWidget* p = now;
        while (p) {
            if (p == m_navPanel)      { m_navPanel->setFocusHighlight(true); break; }
            if (p == m_contentPanel)  { m_contentPanel->setFocusHighlight(true); break; }
            if (p == m_metaPanel)     { m_metaPanel->setFocusHighlight(true); break; }
            if (p == m_filterPanel)   { m_filterPanel->setFocusHighlight(true); break; }
            p = p->parentWidget();
        }
    });

    // 8. 响应元数据面板自己的星级/颜色变更
    connect(m_metaPanel, &MetaPanel::metadataChanged, [this](int rating, const std::wstring& color) {
        auto indexes = m_contentPanel->getSelectedIndexes();
        for (const auto& idx : indexes) {
            QString path = idx.data(PathRole).toString(); 
            if(path.isEmpty()) continue;
            
            QFileInfo info(path);
            ArcMeta::AmMetaJson meta(info.absolutePath().toStdWString());
            meta.load();

            if (rating != -1) {
                // MainWindow 拿到的 idx 是由 ContentPanel 视图通过 getSelectedIndexes() 返回的
                // 而这些视图现在关联的是 ProxyModel，所以必须通过模型自身的 setData
                m_contentPanel->getProxyModel()->setData(idx, rating, RatingRole);
                meta.items()[info.fileName().toStdWString()].rating = rating;
            }
            if (color != L"__NO_CHANGE__") {
                m_contentPanel->getProxyModel()->setData(idx, QString::fromStdWString(color), ColorRole);
                meta.items()[info.fileName().toStdWString()].color = color;
            }
            meta.save();
        }
    });
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    // 1. Alt+Q: 切换窗口置顶状态
    if (event->key() == Qt::Key_Q && (event->modifiers() & Qt::AltModifier)) {
        m_btnPin->setChecked(!m_btnPin->isChecked());
        event->accept();
        return;
    }

    // 2. Ctrl+F: 聚焦搜索过滤框
    if (event->key() == Qt::Key_F && (event->modifiers() & Qt::ControlModifier)) {
        m_searchEdit->setFocus(Qt::ShortcutFocusReason);
        m_searchEdit->selectAll();
        event->accept();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

// 2026-03-xx 按照用户要求：物理拦截事件以实现自定义 ToolTipOverlay 的显隐控制
bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::HoverEnter) {
        QString text = watched->property("tooltipText").toString();
        if (!text.isEmpty()) {
            // 物理级别禁绝原生 ToolTip，强制调用 ToolTipOverlay
            ToolTipOverlay::instance()->showText(QCursor::pos(), text);
        }
    } else if (event->type() == QEvent::HoverLeave || event->type() == QEvent::MouseButtonPress) {
        ToolTipOverlay::hideTip();
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::initBaseLayout() {
    // 1. 外层布局 (20px 呼吸感)
    m_outerLayout = new QVBoxLayout();
    m_outerLayout->setContentsMargins(20, 20, 20, 20);
    m_outerLayout->setSpacing(0);

    // 2. 主容器
    m_container = new QFrame();
    m_container->setObjectName("DialogContainer");

    m_mainLayout = new QVBoxLayout(m_container);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // 3. 标题栏
    m_titleBar = new QWidget(m_container);
    m_titleBar->setObjectName("TitleBar");
    m_titleBar->setFixedHeight(32);

    m_titleLayout = new QHBoxLayout(m_titleBar);
    m_titleLayout->setContentsMargins(12, 0, 2, 0);
    m_titleLayout->setSpacing(5);

    m_mainLayout->addWidget(m_titleBar);

    // 4. 内容区
    m_contentArea = new QWidget(m_container);
    m_contentArea->setObjectName("DialogContentArea");
    m_mainLayout->addWidget(m_contentArea, 1);

    m_outerLayout->addWidget(m_container);

    QWidget* centralC = new QWidget(this);
    centralC->setLayout(m_outerLayout);
    setCentralWidget(centralC);
}

void MainWindow::initToolbar() {
    auto createBtn = [this](const QString& iconKey, const QString& tip) {
        QPushButton* btn = new QPushButton(m_titleBar);
        btn->setFixedSize(32, 28); // 极致精简宽度
        
        QIcon icon = UiHelper::getIcon(iconKey, QColor("#EEEEEE"));
        btn->setIcon(icon);
        btn->setIconSize(QSize(18, 18));
        
        btn->setToolTip(tip);
        // 极致精简样式：无边框，仅悬停可见背景
        btn->setStyleSheet(
            "QPushButton { background: transparent; border: none; border-radius: 4px; }"
            "QPushButton:hover { background: rgba(255, 255, 255, 0.1); }"
            "QPushButton:pressed { background: rgba(255, 255, 255, 0.2); }"
            "QPushButton:disabled { opacity: 0.3; }"
        );
        return btn;
    };

    m_btnBack = createBtn("nav_prev", "");
    m_btnBack->setProperty("tooltipText", "后退");
    m_btnBack->installEventFilter(this);

    m_btnForward = createBtn("nav_next", "");
    m_btnForward->setProperty("tooltipText", "前进");
    m_btnForward->installEventFilter(this);

    m_btnUp = createBtn("arrow_up", "");
    m_btnUp->setProperty("tooltipText", "上级");
    m_btnUp->installEventFilter(this);

    connect(m_btnBack, &QPushButton::clicked, this, &MainWindow::onBackClicked);
    connect(m_btnForward, &QPushButton::clicked, this, &MainWindow::onForwardClicked);
    connect(m_btnUp, &QPushButton::clicked, this, &MainWindow::onUpClicked);

    // --- 路径地址栏重构 (Stack: Breadcrumb + QLineEdit) ---
    m_pathStack = new QStackedWidget(m_titleBar);
    // 2026-03-xx 按照用户最新要求：地址栏高度还原为 32px
    m_pathStack->setFixedHeight(28); // 在 32px 标题栏中略缩以垂直居中
    m_pathStack->setMinimumWidth(300);
    m_pathStack->setStyleSheet("QStackedWidget { background: #1E1E1E; border: 1px solid #444444; border-radius: 4px; }");

    // A. 面包屑视图
    m_breadcrumbBar = new BreadcrumbBar(m_pathStack);
    m_pathStack->addWidget(m_breadcrumbBar);

    // B. 编辑视图
    m_pathEdit = new QLineEdit(m_pathStack);
    m_pathEdit->setPlaceholderText("输入路径...");
    m_pathEdit->setFixedHeight(34);
    m_pathEdit->setStyleSheet("QLineEdit { background: transparent; border: none; color: #EEEEEE; padding-left: 8px; }");
    m_pathStack->addWidget(m_pathEdit);

    m_pathStack->setCurrentWidget(m_breadcrumbBar);

    // 交互逻辑
    connect(m_breadcrumbBar, &BreadcrumbBar::blankAreaClicked, [this]() {
        m_pathEdit->setText(QDir::toNativeSeparators(m_currentPath));
        m_pathStack->setCurrentWidget(m_pathEdit);
        m_pathEdit->setFocus();
        m_pathEdit->selectAll();
    });
    connect(m_pathEdit, &QLineEdit::editingFinished, [this]() {
        // 只有在失去焦点或按回车后切回面包屑 (如果不是由于 confirm 跳转)
        if (m_pathStack->currentWidget() == m_pathEdit) {
            m_pathStack->setCurrentWidget(m_breadcrumbBar);
        }
    });
    connect(m_breadcrumbBar, &BreadcrumbBar::pathClicked, [this](const QString& path) {
        navigateTo(path);
    });

    m_searchEdit = new QLineEdit(m_titleBar);
    m_searchEdit->setPlaceholderText("过滤内容...");
    m_searchEdit->setMinimumWidth(230);
    m_searchEdit->setFixedHeight(28);
    m_searchEdit->addAction(UiHelper::getIcon("search", QColor("#888888")), QLineEdit::LeadingPosition);

    m_titleLayout->addWidget(m_btnBack);
    m_titleLayout->addWidget(m_btnForward);
    m_titleLayout->addWidget(m_btnUp);
    m_titleLayout->addWidget(m_pathStack, 1);
    m_titleLayout->addWidget(m_searchEdit);
}



void MainWindow::setupSplitters() {
    QVBoxLayout* bodyLayout = new QVBoxLayout(m_contentArea);
    bodyLayout->setContentsMargins(5, 5, 5, 5);
    bodyLayout->setSpacing(0);

    m_mainSplitter = new QSplitter(Qt::Horizontal, m_contentArea);
    m_mainSplitter->setHandleWidth(5); 
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setStyleSheet("QSplitter { background: transparent; border: none; } QSplitter::handle { background: transparent; }");

    m_categoryPanel = new CategoryPanel(this);
    m_categoryPanel->setObjectName("SidebarContainer");
    
    m_navPanel = new NavPanel(this);
    m_navPanel->setObjectName("ListContainer");
    
    m_contentPanel = new ContentPanel(this);
    m_contentPanel->setObjectName("EditorContainer");
    
    m_metaPanel = new MetaPanel(this);
    m_metaPanel->setObjectName("MetadataContainer");
    
    m_filterPanel = new FilterPanel(this);
    m_filterPanel->setObjectName("FilterContainer");

    m_mainSplitter->addWidget(m_categoryPanel);
    m_mainSplitter->addWidget(m_navPanel);
    m_mainSplitter->addWidget(m_contentPanel);
    m_mainSplitter->addWidget(m_metaPanel);
    m_mainSplitter->addWidget(m_filterPanel);

    bodyLayout->addWidget(m_mainSplitter);

    // --- 4. 底部状态栏 ---
    QWidget* statusBar = new QWidget(m_contentArea);
    statusBar->setFixedHeight(28);
    statusBar->setStyleSheet("QWidget { background-color: #252525; border-top: 1px solid #333333; }");
    QHBoxLayout* statusL = new QHBoxLayout(statusBar);
    statusL->setContentsMargins(12, 0, 12, 0);
    statusL->setSpacing(0);

    m_statusLeft = new QLabel("就绪中...", statusBar);
    m_statusLeft->setStyleSheet("font-size: 11px; color: #B0B0B0; background: transparent;");

    // 绑定 CoreController 状态到状态栏
    auto updateStatus = [this](const QString& text) {
        m_statusLeft->setText(text);
        if (CoreController::instance().isIndexing()) {
            m_statusLeft->setStyleSheet("font-size: 11px; color: #4FACFE; background: transparent; font-weight: bold;");
        } else {
            m_statusLeft->setStyleSheet("font-size: 11px; color: #B0B0B0; background: transparent;");
        }
    };
    connect(&CoreController::instance(), &CoreController::statusTextChanged, this, updateStatus);
    connect(&CoreController::instance(), &CoreController::isIndexingChanged, this, [this, updateStatus]() {
        updateStatus(CoreController::instance().statusText());
    });
    updateStatus(CoreController::instance().statusText());

    m_statusCenter = new QLabel("", statusBar);
    m_statusCenter->setAlignment(Qt::AlignCenter);
    m_statusCenter->setStyleSheet("font-size: 11px; color: #888888; background: transparent;");

    m_statusRight = new QLabel("", statusBar);
    m_statusRight->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statusRight->setStyleSheet("font-size: 11px; color: #B0B0B0; background: transparent;");

    statusL->addWidget(m_statusLeft, 1);
    statusL->addWidget(m_statusCenter, 1);
    statusL->addWidget(m_statusRight, 1);

    bodyLayout->addWidget(statusBar);
}

/**
 * @brief 实现符合架构规范的系统按钮组
 */
void MainWindow::setupCustomTitleBarButtons() {
    auto createTitleBtn = [this](const QString& iconKey, const QString& hoverColor = "rgba(255, 255, 255, 0.1)") {
        QPushButton* btn = new QPushButton(m_titleBar);
        btn->setFixedSize(28, 28);
        
        QIcon icon = UiHelper::getIcon(iconKey, QColor("#EEEEEE"));
        btn->setIcon(icon);
        btn->setIconSize(QSize(18, 18));
        
        btn->setStyleSheet(QString(
            "QPushButton { background: transparent; border: none; border-radius: 4px; padding: 0; }"
            "QPushButton:hover { background: %1; }"
            "QPushButton:pressed { background: rgba(255, 255, 255, 0.2); }"
        ).arg(hoverColor));
        return btn;
    };

    m_btnCreate = createTitleBtn("add");
    m_btnCreate->setProperty("tooltipText", "新建...");
    QMenu* createMenu = new QMenu(m_btnCreate);
    createMenu->setStyleSheet(
        "QMenu { background-color: #2B2B2B; border: 1px solid #444444; color: #EEEEEE; padding: 4px; border-radius: 6px; }"
        "QMenu::item { height: 24px; padding: 0 20px 0 10px; border-radius: 3px; font-size: 12px; }"
        "QMenu::item:selected { background-color: #505050; }"
        "QMenu::right-arrow { image: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCIgZmlsbD0ibm9uZSIgc3Ryb2tlPSIjRUVFRUVFIiBzdHJva2Utd2lkdGg9IjIiIHN0cm9rZS1saW5lY2FwPSJyb3VuZCIgc3Ryb2tlLWxpbmVqb2luPSJyb3VuZCI+PHBvbHlsaW5lIHBvaW50cz0iOSAxOCAxNSAxMiA5IDYiPjwvcG9seWxpbmU+PC9zdmc+); width: 12px; height: 12px; right: 8px; }"
    );
    
    QAction* actNewFolder = createMenu->addAction(UiHelper::getIcon("folder", QColor("#EEEEEE")), "创建文件夹");
    QAction* actNewMd     = createMenu->addAction(UiHelper::getIcon("text", QColor("#EEEEEE")), "创建 Markdown");
    QAction* actNewTxt    = createMenu->addAction(UiHelper::getIcon("text", QColor("#EEEEEE")), "创建纯文本文件 (txt)");
    
    // 2026-03-xx 按照用户要求修正居中对齐：
    // 不再使用 setMenu，避免按钮进入“菜单模式”从而为指示器预留空间导致图标偏左。
    // 采用手动 popup 方式展示菜单。
    connect(m_btnCreate, &QPushButton::clicked, [this, createMenu]() {
        createMenu->popup(m_btnCreate->mapToGlobal(QPoint(0, m_btnCreate->height())));
    });

    auto handleCreate = [this](const QString& type) {
        m_contentPanel->createNewItem(type);
    };
    connect(actNewFolder, &QAction::triggered, [handleCreate](){ handleCreate("folder"); });
    connect(actNewMd,     &QAction::triggered, [handleCreate](){ handleCreate("md"); });
    connect(actNewTxt,    &QAction::triggered, [handleCreate](){ handleCreate("txt"); });

    m_btnPin = createTitleBtn(m_isPinned ? "pin_vertical" : "pin_tilted");
    m_btnPin->setProperty("tooltipText", "置顶窗口");
    m_btnPin->installEventFilter(this);
    m_btnPin->setCheckable(true);
    m_btnPin->setChecked(m_isPinned);
    if (m_isPinned) {
        m_btnPin->setIcon(UiHelper::getIcon("pin_vertical", QColor("#FF551C")));
    }

    m_minBtn = createTitleBtn("minimize");
    m_minBtn->setProperty("tooltipText", "最小化");
    m_minBtn->installEventFilter(this);

    m_maxBtn = createTitleBtn("maximize");
    m_maxBtn->setProperty("tooltipText", "最大化/还原");
    m_maxBtn->installEventFilter(this);

    m_closeBtn = createTitleBtn("close", "#e81123");
    m_closeBtn->setProperty("tooltipText", "关闭");
    m_closeBtn->installEventFilter(this);

    m_btnCreate->installEventFilter(this);

    // 添加顺序 (Memories.md 规范：从右向左为 关闭 -> 最大化 -> 最小化 -> 置顶)
    m_titleLayout->addSpacing(10);
    m_titleLayout->addWidget(m_btnCreate);
    m_titleLayout->addStretch(); // 弹簧
    m_titleLayout->addWidget(m_btnPin);
    m_titleLayout->addWidget(m_minBtn);
    m_titleLayout->addWidget(m_maxBtn);
    m_titleLayout->addWidget(m_closeBtn);

    // 绑定基础逻辑
    connect(m_minBtn, &QPushButton::clicked, this, &MainWindow::showMinimized);
    connect(m_maxBtn, &QPushButton::clicked, this, &MainWindow::toggleMaximize);
    connect(m_closeBtn, &QPushButton::clicked, this, &MainWindow::close);
    connect(m_btnPin, &QPushButton::toggled, this, &MainWindow::onPinToggled);

    // 补全系统按钮的事件过滤器安装 (ToolTip 迁移)
    m_minBtn->installEventFilter(this);
    m_maxBtn->installEventFilter(this);
    m_closeBtn->installEventFilter(this);
    m_btnPin->installEventFilter(this);
}

void MainWindow::navigateTo(const QString& path, bool record) {
    if (path.isEmpty()) return;
    
    // 处理虚拟路径 "computer://" —— 此电脑（磁盘分区列表）
    if (path == "computer://") {
        m_currentPath = "computer://";
        if (record) {
            if (m_history.isEmpty() || m_history.last() != path) {
                m_history.append(path);
                m_historyIndex = m_history.size() - 1;
            }
        }
        m_pathEdit->setText("此电脑");
        m_breadcrumbBar->setPath("computer://");
        m_pathStack->setCurrentWidget(m_breadcrumbBar);
        m_contentPanel->loadDirectory(""); 
        int driveCount = static_cast<int>(QDir::drives().count());
        m_statusLeft->setText(QString("%1 个分区").arg(driveCount));
        m_statusCenter->setText("此电脑");
        updateNavButtons();
        return;
    }

    QString normPath = QDir::toNativeSeparators(path);
    m_currentPath = normPath;

    if (record) {
            if (m_historyIndex < static_cast<int>(m_history.size()) - 1) {
            m_history = m_history.mid(0, m_historyIndex + 1);
        }
        if (m_history.isEmpty() || m_history.last() != normPath) {
            m_history.append(normPath);
                m_historyIndex = static_cast<int>(m_history.size()) - 1;
        }
    }
    
    m_pathEdit->setText(normPath);
    m_breadcrumbBar->setPath(normPath);
    m_pathStack->setCurrentWidget(m_breadcrumbBar);
    m_contentPanel->loadDirectory(normPath);
    updateNavButtons();
    updateStatusBar();
}

void MainWindow::onBackClicked() {
    if (m_historyIndex > 0) {
        m_historyIndex--;
        navigateTo(m_history[m_historyIndex], false);
    }
}

void MainWindow::onForwardClicked() {
    if (m_historyIndex < m_history.size() - 1) {
        m_historyIndex++;
        navigateTo(m_history[m_historyIndex], false);
    }
}

void MainWindow::onUpClicked() {
    QDir dir(m_currentPath);
    if (dir.cdUp()) {
        navigateTo(dir.absolutePath());
    }
}

void MainWindow::updateNavButtons() {
    m_btnBack->setEnabled(m_historyIndex > 0);
    m_btnForward->setEnabled(m_historyIndex < m_history.size() - 1);
    
    bool atRoot = (m_currentPath == "computer://" || (QDir(m_currentPath).isRoot()));
    m_btnUp->setEnabled(!atRoot && !m_currentPath.isEmpty());
}

void MainWindow::updateStatusBar() {
    if (!m_statusLeft || !m_statusCenter || !m_statusRight) return;
    
    // 修正：显示经过过滤后的可见项目总数
    int visibleCount = m_contentPanel->getProxyModel()->rowCount();
    m_statusLeft->setText(QString("%1 个项目").arg(visibleCount));
    m_statusCenter->setText(m_currentPath == "computer://" ? "此电脑" : m_currentPath);
    m_statusRight->setText(""); // 选中时由 selectionChanged 更新
}

void MainWindow::toggleMaximize() {
    if (isMaximized()) showNormal();
    else showMaximized();
}

void MainWindow::onPinToggled(bool checked) {
    if (m_isPinned == checked) return;
    m_isPinned = checked;

#ifdef Q_OS_WIN
    HWND hwnd = (HWND)winId();
    SetWindowPos(hwnd, checked ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
#else
    setWindowFlag(Qt::WindowStaysOnTopHint, checked);
    show();
#endif

    if (m_btnPin) {
        m_btnPin->setIcon(UiHelper::getIcon(checked ? "pin_vertical" : "pin_tilted",
                          checked ? QColor("#FF551C") : QColor("#EEEEEE")));
    }

    QSettings settings("ArcMeta团队", "ArcMeta");
    settings.setValue("MainWindow/AlwaysOnTop", m_isPinned);
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::WindowStateChange) {
        bool max = isMaximized();
        m_maxBtn->setIcon(UiHelper::getIcon(max ? "restore" : "maximize", QColor("#EEEEEE")));

        m_outerLayout->setContentsMargins(max ? 0 : 20, max ? 0 : 20, max ? 0 : 20, max ? 0 : 20);
        m_container->setStyleSheet(
            QString("QFrame#DialogContainer { background-color: #1e1e1e; border: %1; border-radius: %2; }")
            .arg(max ? "none" : "1px solid #333333")
            .arg(max ? "0px" : "12px")
        );
        m_titleBar->setStyleSheet(
            QString("QWidget#TitleBar { background-color: #252526; border-bottom: 1px solid #333333; "
                    "border-top-left-radius: %1; border-top-right-radius: %1; }")
            .arg(max ? "0px" : "12px")
        );
    }
    QMainWindow::changeEvent(event);
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_NCHITTEST) {
        int x = GET_X_LPARAM(msg->lParam);
        int y = GET_Y_LPARAM(msg->lParam);
        QPoint pos = mapFromGlobal(QPoint(x, y));

        ResizeEdge edge = getEdge(pos);
        if (edge != None && !isMaximized()) {
            switch (edge) {
                case Top:         *result = HTTOP; break;
                case Bottom:      *result = HTBOTTOM; break;
                case Left:        *result = HTLEFT; break;
                case Right:       *result = HTRIGHT; break;
                case TopLeft:     *result = HTTOPLEFT; break;
                case TopRight:    *result = HTTOPRIGHT; break;
                case BottomLeft:  *result = HTBOTTOMLEFT; break;
                case BottomRight: *result = HTBOTTOMRIGHT; break;
                default: break;
            }
            return true;
        }

        if (m_titleBar && m_titleBar->geometry().contains(pos)) {
            QWidget* child = childAt(pos);
            if (child && (child->inherits("QPushButton") || child->inherits("QToolButton") || child->inherits("QLineEdit") || child->inherits("QStackedWidget"))) {
                return false;
            }
            *result = HTCAPTION;
            return true;
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

MainWindow::ResizeEdge MainWindow::getEdge(const QPoint& pos) {
    int border = 5;
    bool t = pos.y() <= border;
    bool b = pos.y() >= height() - border;
    bool l = pos.x() <= border;
    bool r = pos.x() >= width() - border;

    if (t && l) return TopLeft;
    if (t && r) return TopRight;
    if (b && l) return BottomLeft;
    if (b && r) return BottomRight;
    if (t) return Top;
    if (b) return Bottom;
    if (l) return Left;
    if (r) return Right;
    return None;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    QSettings settings("ArcMeta团队", "ArcMeta");
    settings.setValue("MainWindow/LastPath", m_currentPath);
    QMainWindow::closeEvent(event);
}

} // namespace ArcMeta
