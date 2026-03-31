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
#include <QGraphicsDropShadowEffect>
#include "UiHelper.h"
#include <QFileInfo>
#include <QDir>
#include "../meta/AmMetaJson.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace ArcMeta {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    resize(1200, 800);
    setMinimumSize(1000, 600);
    setWindowTitle("ArcMeta");

    // 从设置读取置顶状态
    QSettings settings("ArcMeta团队", "ArcMeta");
    m_isPinned = settings.value("MainWindow/AlwaysOnTop", false).toBool();
    
    // 设置基础窗口标志 (保持无边框)
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);

    // 初始应用置顶 (WinAPI)
    // 2026-03-xx 关键修复：构造函数内不再调用 winId() 或 SetWindowPos 避免触发窗口提前显示
    // 置顶逻辑现在改为按需由 external 或 showEvent 安全触发
    if (m_isPinned) {
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
    }

    // 应用全局样式（包括滚动条美化）
    QString qss = R"(
        QMainWindow { background-color: #1A1A1A; }

        /* 核心容器样式还原 - 强化 1 像素物理切割感 */
        #SidebarContainer, #ListContainer, #EditorContainer, #MetadataContainer, #FilterContainer {
            background-color: #1E1E1E;
            border: 1px solid #333333;
            border-radius: 0px;
        }

        /* 容器标题栏样式 (还原旧版 #252526 实色背景与下边框) */
        /* 2026-03-xx 回归修正：各面板已注入局部样式，此处保留全局 ID 匹配以作兜底 */
        #ContainerHeader {
            background-color: #252526;
            border-bottom: 1px solid #333333;
        }

        /* 全局滚动条美化 */
        QScrollBar:vertical {
            border: none;
            background: transparent;
            width: 4px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #333333;
            min-height: 20px;
            border-radius: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background: #444444;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }

        QScrollBar:horizontal {
            border: none;
            background: transparent;
            height: 4px;
            margin: 0px;
        }
        QScrollBar::handle:horizontal {
            background: #333333;
            min-width: 20px;
            border-radius: 2px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #444444;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: none;
        }

        /* 统一复选框样式 */
        QCheckBox { color: #EEEEEE; font-size: 12px; spacing: 5px; }
        QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid #444; border-radius: 2px; background: #1E1E1E; }
        QCheckBox::indicator:hover { border: 1px solid #666; }
        QCheckBox::indicator:checked { 
            border: 1px solid #378ADD; 
            background: #1E1E1E; 
            image: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCIgZmlsbD0ibm9uZSIgc3Ryb2tlPSIjMzc4QUREIiBzdHJva2Utd2lkdGg9IjMuNSIgc3Ryb2tlLWxpbmVjYXA9InJvdW5kIiBzdHJva2UtbGluZWpvaW49InJvdW5kIj48cG9seWxpbmUgcG9pbnRzPSIyMCA2IDkgMTcgNCAxMiI+PC9wb2x5bGluZT48L3N2Zz4=);
        }

        /* 统一输入框样式 */
        QLineEdit {
            background: #1E1E1E;
            border: 1px solid #333333;
            border-radius: 4px;
            color: #EEEEEE;
            padding-left: 8px;
        }
        QLineEdit:focus {
            border: 1px solid #378ADD;
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

void MainWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // 点击工具栏区域（前 36px）允许拖动窗口
        if (event->position().y() <= 36) {
            m_isDragging = true;
            m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    m_isDragging = false;
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    // 1. Alt+Q: 切换窗口置顶状态
    if (event->key() == Qt::Key_Q && (event->modifiers() & Qt::AltModifier)) {
        m_btnPinTop->setChecked(!m_btnPinTop->isChecked());
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

void MainWindow::initToolbar() {
    m_toolbar = addToolBar("MainToolbar");
    m_toolbar->setFixedHeight(32);
    m_toolbar->setMovable(false);
    m_toolbar->setStyleSheet("QToolBar { background-color: #252525; border: none; padding-left: 12px; padding-right: 12px; spacing: 8px; border-bottom: 1px solid #333; }");

    auto createBtn = [this](const QString& iconKey, const QString& tip) {
        QPushButton* btn = new QPushButton(this);
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
    m_pathStack = new QStackedWidget(this);
    // 2026-03-xx 按照用户最新要求：地址栏高度还原为 32px
    m_pathStack->setFixedHeight(32); 
    m_pathStack->setMinimumWidth(300);
    m_pathStack->setStyleSheet("QStackedWidget { background: transparent; border: 1px solid #444444; border-radius: 4px; }");

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

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("过滤内容...");
    m_searchEdit->setMinimumWidth(230);
    // 2026-03-xx 按照用户要求：对齐地址栏，将搜索框高度还原为 32px
    m_searchEdit->setFixedHeight(32); 
    m_searchEdit->setStyleSheet(
        "QLineEdit { background: transparent; border: 1px solid #444444; border-radius: 6px; color: #EEEEEE; padding-left: 8px; }"
        "QLineEdit:focus { border: 1px solid #FFFFFF; }"
    );

}



void MainWindow::setupSplitters() {
    QWidget* centralC = new QWidget(this);
    // 还原旧版紧凑布局：父容器背景透明，容器间无物理缝隙
    centralC->setStyleSheet("background-color: transparent;"); 
    QVBoxLayout* mainL = new QVBoxLayout(centralC);
    mainL->setContentsMargins(0, 0, 0, 0); // 还原 0px 全局边距
    mainL->setSpacing(0); // 还原 0px 垂直间距

    QWidget* addressBar = new QWidget(centralC);
    addressBar->setFixedHeight(32); // 2026-03-xx 按照最新要求：地址栏高度还原为 32px
    addressBar->setStyleSheet("QWidget { background: transparent; border: none; }");
    QHBoxLayout* addrL = new QHBoxLayout(addressBar);
    addrL->setContentsMargins(0, 0, 0, 0);
    // 地址栏组件间距同步还原为紧凑型 (2px)
    addrL->setSpacing(2);

    addrL->addWidget(m_btnBack);
    addrL->addWidget(m_btnForward);
    addrL->addWidget(m_btnUp);
    addrL->addWidget(m_pathStack, 1);
    addrL->addWidget(m_searchEdit);

    // --- 主拆分条 (还原 1px 物理缝隙，使容器边框重合) ---
    m_mainSplitter = new QSplitter(Qt::Horizontal, centralC);
    m_mainSplitter->setHandleWidth(1);
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setStyleSheet("QSplitter::handle { background-color: #333333; }");

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

    // 为每个面板应用阴影效果 (1:1 还原旧版本参数)
    auto applyShadow = [this](QWidget* w) {
        auto* shadow = new QGraphicsDropShadowEffect(w);
        shadow->setBlurRadius(10);
        shadow->setXOffset(0);
        shadow->setYOffset(4);
        shadow->setColor(QColor(0, 0, 0, 150));
        w->setGraphicsEffect(shadow);
    };

    applyShadow(m_categoryPanel);
    applyShadow(m_navPanel);
    applyShadow(m_contentPanel);
    applyShadow(m_metaPanel);
    applyShadow(m_filterPanel);

    m_mainSplitter->addWidget(m_categoryPanel);
    m_mainSplitter->addWidget(m_navPanel);
    m_mainSplitter->addWidget(m_contentPanel);
    m_mainSplitter->addWidget(m_metaPanel);
    m_mainSplitter->addWidget(m_filterPanel);

    // --- 底部状态栏（28px） ---
    QWidget* statusBar = new QWidget(centralC);
    statusBar->setFixedHeight(28);
    statusBar->setStyleSheet("QWidget { background-color: #252525; border-top: 1px solid #333333; }");
    QHBoxLayout* statusL = new QHBoxLayout(statusBar);
    statusL->setContentsMargins(5, 0, 5, 0);
    statusL->setSpacing(0);

    m_statusLeft = new QLabel("就绪中...", statusBar);
    m_statusLeft->setStyleSheet("font-size: 11px; color: #B0B0B0; background: transparent;");

    // 绑定 CoreController 状态到状态栏
    auto updateStatus = [this](const QString& text) {
        m_statusLeft->setText(text);
        // 如果正在索引，改变颜色提示
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
    // 初始状态同步
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

    mainL->addWidget(addressBar);
    mainL->addWidget(m_mainSplitter, 1);
    mainL->addWidget(statusBar);

    setCentralWidget(centralC);
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
        
        // 使用 UiHelper 全局辅助类
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

    m_btnCreate = createTitleBtn("add"); // 2026-03-xx 规范化：“+”按钮图标修正
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

    m_btnPinTop = createTitleBtn(m_isPinned ? "pin_vertical" : "pin_tilted");
    m_btnPinTop->setProperty("tooltipText", "置顶窗口");
    m_btnPinTop->installEventFilter(this);
    m_btnPinTop->setCheckable(true);
    m_btnPinTop->setChecked(m_isPinned);
    if (m_isPinned) {
        m_btnPinTop->setIcon(UiHelper::getIcon("pin_vertical", QColor("#FF551C")));
    }

    m_btnMin = createTitleBtn("minimize");
    m_btnMin->setProperty("tooltipText", "最小化");
    m_btnMin->installEventFilter(this);

    m_btnMax = createTitleBtn("maximize");
    m_btnMax->setProperty("tooltipText", "最大化/还原");
    m_btnMax->installEventFilter(this);

    m_btnClose = createTitleBtn("close", "#e81123"); // 关闭按钮悬停红色
    m_btnClose->setProperty("tooltipText", "关闭项目");
    m_btnClose->installEventFilter(this);

    m_btnCreate->installEventFilter(this);
    layout->addWidget(m_btnCreate);
    layout->addWidget(m_btnPinTop);
    layout->addWidget(m_btnMin);
    layout->addWidget(m_btnMax);
    layout->addWidget(m_btnClose);

    // 绑定基础逻辑
    connect(m_btnMin, &QPushButton::clicked, this, &MainWindow::showMinimized);
    connect(m_btnMax, &QPushButton::clicked, [this]() {
        if (isMaximized()) showNormal();
        else showMaximized();
    });
    connect(m_btnClose, &QPushButton::clicked, this, &MainWindow::close);

    // 将按钮组添加到工具栏最右侧 (QToolBar 不支持 addStretch，改用弹簧 Widget)
    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);
    m_toolbar->addWidget(titleBarBtns);

    // 逻辑：置顶切换
    connect(m_btnPinTop, &QPushButton::toggled, this, &MainWindow::onPinToggled);
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

void MainWindow::onPinToggled(bool checked) {
    // 2026-03-xx 按照用户要求优化置顶逻辑：
    // 避免重复调用导致卡顿，并优化 WinAPI 标志位以减少冗余消息推送
    if (m_isPinned == checked) return;
    m_isPinned = checked;

#ifdef Q_OS_WIN
    HWND hwnd = (HWND)winId();
    // 使用 SWP_NOSENDCHANGING 拦截冗余消息，减少 UI 线程的消息风暴，从而解决卡顿
    SetWindowPos(hwnd, checked ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
#else
    setWindowFlag(Qt::WindowStaysOnTopHint, checked);
    show(); // 非 Windows 平台修改 Flag 后通常需要重新显示
#endif

    // 更新图标和颜色 (按下置顶为品牌橙色)
    if (m_isPinned) {
        m_btnPinTop->setIcon(UiHelper::getIcon("pin_vertical", QColor("#FF551C")));
    } else {
        m_btnPinTop->setIcon(UiHelper::getIcon("pin_tilted", QColor("#EEEEEE")));
    }

    // 持久化存储
    QSettings settings("ArcMeta团队", "ArcMeta");
    settings.setValue("MainWindow/AlwaysOnTop", m_isPinned);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    QSettings settings("ArcMeta团队", "ArcMeta");
    settings.setValue("MainWindow/LastPath", m_currentPath);
    QMainWindow::closeEvent(event);
}

} // namespace ArcMeta
