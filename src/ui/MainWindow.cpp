#include "MainWindow.h"
#include "CategoryPanel.h"
#include "NavPanel.h"
#include "NavPanel.h"
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
#include <QMouseEvent>
#include <QApplication>
#include <QSettings>
#include <QCloseEvent>
#include "UiHelper.h"
#include <QFileInfo>
#include "../meta/AmMetaJson.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace ArcMeta {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    resize(1600, 900);
    setMinimumSize(1280, 720);
    setWindowTitle("ArcMeta");

    // 从设置读取置顶状态
    QSettings settings("ArcMeta团队", "ArcMeta");
    m_isPinned = settings.value("MainWindow/AlwaysOnTop", false).toBool();
    
    // 设置基础窗口标志 (保持无边框)
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);

    // 初始应用置顶 (WinAPI)
    if (m_isPinned) {
#ifdef Q_OS_WIN
        HWND hwnd = (HWND)winId();
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
#endif
    }

    // 应用全局样式（包括滚动条美化）
    QString qss = R"(
        QMainWindow { background-color: #1A1A1A; }
        
        /* 全局滚动条美化 */
        QScrollBar:vertical {
            border: none;
            background: #1E1E1E;
            width: 8px;
            margin: 0px 0px 0px 0px;
        }
        QScrollBar::handle:vertical {
            background: #333333;
            min-height: 20px;
            border-radius: 4px;
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
            background: #1E1E1E;
            height: 8px;
            margin: 0px 0px 0px 0px;
        }
        QScrollBar::handle:horizontal {
            background: #333333;
            min-width: 20px;
            border-radius: 4px;
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

        /* 统一输入框样式 */
        QLineEdit {
            background: #1A1A1A;
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

    // 恢复最后一次打开的路径
    QString lastPath = settings.value("MainWindow/LastPath", "C:/").toString();
    if (!QDir(lastPath).exists()) lastPath = "C:/";
    navigateTo(lastPath);
}

void MainWindow::initUi() {
    initToolbar();
    setupSplitters();
    setupCustomTitleBarButtons();
    
    // 设置默认权重分配: 230 | 200 | 弹性 | 240 | 230 (移除一个 200)
    QList<int> sizes;
    sizes << 230 << 200 << 700 << 240 << 230;
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
        m_pathEdit->setText("分类: " + name);
        m_contentPanel->search(name); // 暂时通过 search 模拟分类展示
    });

    // 2. 内容面板选中项改变 -> 元数据面板刷新
    connect(m_contentPanel, &ContentPanel::selectionChanged, [this](const QStringList& paths) {
        if (paths.isEmpty()) {
            m_metaPanel->updateInfo("-", "-", "-", "-", "-", "-", "-", false);
            m_metaPanel->setRating(0);
            m_metaPanel->setColor(L"");
            m_metaPanel->setPinned(false);
            m_metaPanel->setTags(QStringList());
        } else {
            QString path = paths.first();
            QFileInfo info(path);
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

            // 从 .am-meta.json 回读物理状态以刷新侧边面板 UI
            ArcMeta::AmMetaJson meta(info.absolutePath().toStdWString());
            meta.load();
            auto it = meta.items().find(info.fileName().toStdWString());
            if (it != meta.items().end()) {
                m_metaPanel->setRating(it->second.rating);
                m_metaPanel->setColor(it->second.color);
                m_metaPanel->setPinned(it->second.pinned);
                QStringList tgs;
                for (const auto& t : it->second.tags) tgs << QString::fromStdWString(t);
                m_metaPanel->setTags(tgs);
            } else {
                m_metaPanel->setRating(0);
                m_metaPanel->setColor(L"");
                m_metaPanel->setPinned(false);
                m_metaPanel->setTags(QStringList());
            }
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
        QString input = m_pathEdit->text();
        if (QDir(input).exists()) {
            navigateTo(input);
        } else {
            // 如果路径无效，恢复为当前实际路径
            m_pathEdit->setText(QDir::toNativeSeparators(m_currentPath));
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
            QString path = idx.data(Qt::UserRole + 3 /* PathRole */).toString(); // 根据 ContentPanel.h Role 确定
            if(path.isEmpty()) continue;
            
            QFileInfo info(path);
            ArcMeta::AmMetaJson meta(info.absolutePath().toStdWString());
            meta.load();

            if (rating != -1) {
                m_contentPanel->model()->setData(idx, rating, Qt::UserRole + 1 /* RatingRole */);
                meta.items()[info.fileName().toStdWString()].rating = rating;
            }
            if (color != L"__NO_CHANGE__") {
                m_contentPanel->model()->setData(idx, QString::fromStdWString(color), Qt::UserRole + 2 /* ColorRole */);
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

void MainWindow::initToolbar() {
    m_toolbar = addToolBar("MainToolbar");
    m_toolbar->setFixedHeight(36);
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

    m_btnBack = createBtn("nav_prev", "后退");
    m_btnForward = createBtn("nav_next", "前进");
    m_btnUp = createBtn("arrow_up", "上级");

    connect(m_btnBack, &QPushButton::clicked, this, &MainWindow::onBackClicked);
    connect(m_btnForward, &QPushButton::clicked, this, &MainWindow::onForwardClicked);
    connect(m_btnUp, &QPushButton::clicked, this, &MainWindow::onUpClicked);

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

}

void MainWindow::setupSplitters() {
    QWidget* centralC = new QWidget(this);
    QVBoxLayout* mainL = new QVBoxLayout(centralC);
    mainL->setContentsMargins(0, 3, 0, 0); // 与自定义标题栏之间保持3像素的间距
    mainL->setSpacing(3);

    QWidget* addressBar = new QWidget(centralC);
    addressBar->setFixedHeight(40);
    QHBoxLayout* addrL = new QHBoxLayout(addressBar);
    addrL->setContentsMargins(12, 0, 12, 0);
    addrL->setSpacing(8);

    addrL->addWidget(m_btnBack);
    addrL->addWidget(m_btnForward);
    addrL->addWidget(m_btnUp);
    addrL->addWidget(m_pathEdit);
    addrL->addWidget(m_searchEdit);

    m_mainSplitter = new QSplitter(Qt::Horizontal, centralC);
    m_mainSplitter->setHandleWidth(3);
    m_mainSplitter->setStyleSheet("QSplitter::handle { background-color: #333333; }");

    m_categoryPanel = new CategoryPanel(this);
    m_navPanel = new NavPanel(this);
    m_contentPanel = new ContentPanel(this);
    m_metaPanel = new MetaPanel(this);
    m_filterPanel = new FilterPanel(this);

    m_mainSplitter->addWidget(m_categoryPanel);
    m_mainSplitter->addWidget(m_navPanel);
    m_mainSplitter->addWidget(m_contentPanel);
    m_mainSplitter->addWidget(m_metaPanel);
    m_mainSplitter->addWidget(m_filterPanel);

    mainL->addWidget(addressBar);
    mainL->addWidget(m_mainSplitter, 1);

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

    m_btnPinTop = createTitleBtn(m_isPinned ? "pin_vertical" : "pin_tilted");
    m_btnPinTop->setCheckable(true);
    m_btnPinTop->setChecked(m_isPinned);
    if (m_isPinned) {
        m_btnPinTop->setIcon(UiHelper::getIcon("pin_vertical", QColor("#FF551C")));
    }

    m_btnMin = createTitleBtn("minimize");
    m_btnMax = createTitleBtn("maximize");
    m_btnClose = createTitleBtn("close", "#e81123"); // 关闭按钮悬停红色

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

    QString normPath = QDir::toNativeSeparators(path);
    m_currentPath = normPath;

    if (record) {
        // 如果在历史中间进行跳转，清除之后的历史
        if (m_historyIndex < m_history.size() - 1) {
            m_history = m_history.mid(0, m_historyIndex + 1);
        }

        // 避免重复记录相同路径
        if (m_history.isEmpty() || m_history.last() != normPath) {
            m_history.append(normPath);
            m_historyIndex = m_history.size() - 1;
        }
    }

    m_pathEdit->setText(normPath);
    m_contentPanel->loadDirectory(normPath);
    updateNavButtons();
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

    QDir dir(m_currentPath);
    m_btnUp->setEnabled(!dir.isRoot() && !m_currentPath.isEmpty());
}

void MainWindow::onPinToggled(bool checked) {
    m_isPinned = checked;

#ifdef Q_OS_WIN
    HWND hwnd = (HWND)winId();
    SetWindowPos(hwnd, checked ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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
