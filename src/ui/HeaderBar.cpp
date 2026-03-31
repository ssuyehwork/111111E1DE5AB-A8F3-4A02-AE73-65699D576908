#include "HeaderBar.h"
#include "StringUtils.h"
#include "MainWindow.h"
#include "IconHelper.h"
#include "ToolTipOverlay.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QApplication>
#include <QWindow>
#include <QFrame>

HeaderBar::HeaderBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(36);
    setStyleSheet("background-color: #252526; border: none;");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 顶部内容区
    auto* topContent = new QWidget();
    auto* layout = new QHBoxLayout(topContent);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignVCenter);

    // 1. Logo & Title
    QLabel* appLogo = new QLabel();
    appLogo->setFixedSize(18, 18);
    appLogo->setPixmap(IconHelper::getIcon("zap", "#4a90e2", 18).pixmap(18, 18));
    layout->addWidget(appLogo);
    layout->addSpacing(6);

    QLabel* titleLabel = new QLabel("快速笔记");
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #4a90e2; border: none; background: transparent;");
    layout->addWidget(titleLabel);
    layout->addSpacing(15);

    // 标准功能按钮样式
    QString funcBtnStyle =
        "QPushButton {"
        "    background-color: transparent;"
        "    border: none;"
        "    outline: none;"
        "    border-radius: 4px;"
        "    width: 24px;"
        "    height: 24px;"
        "    padding: 0px;"
        "}"
        "QPushButton:hover { background-color: rgba(255, 255, 255, 0.1); }"
        "QPushButton:pressed { background-color: rgba(255, 255, 255, 0.2); }";

    // 侧边栏切换
    QPushButton* btnSidebar = new QPushButton();
    btnSidebar->setIcon(IconHelper::getIcon("sidebar_left", "#aaaaaa", 18));
    btnSidebar->setIconSize(QSize(18, 18));
    btnSidebar->setProperty("tooltipText", "切换侧边栏显示 (Ctrl+L)"); btnSidebar->installEventFilter(this);
    btnSidebar->setStyleSheet(funcBtnStyle);
    connect(btnSidebar, &QPushButton::clicked, this, &HeaderBar::toggleSidebar);
    layout->addWidget(btnSidebar, 0, Qt::AlignCenter);
    layout->addSpacing(4);

    // 手动刷新
    QPushButton* btnRefresh = new QPushButton();
    btnRefresh->setIcon(IconHelper::getIcon("refresh", "#aaaaaa", 18));
    btnRefresh->setIconSize(QSize(18, 18));
    btnRefresh->setProperty("tooltipText", "刷新数据 (F5)"); btnRefresh->installEventFilter(this);
    btnRefresh->setStyleSheet(funcBtnStyle);
    connect(btnRefresh, &QPushButton::clicked, this, &HeaderBar::refreshRequested);
    layout->addWidget(btnRefresh, 0, Qt::AlignCenter);
    layout->addSpacing(4);

    // 全局锁定
    QPushButton* btnLock = new QPushButton();
    btnLock->setIcon(IconHelper::getIcon("lock", "#aaaaaa", 18));
    btnLock->setIconSize(QSize(18, 18));
    btnLock->setProperty("tooltipText", "全局锁定 （Ctrl + Shift + Alt + S）"); btnLock->installEventFilter(this);
    btnLock->setStyleSheet(funcBtnStyle);
    connect(btnLock, &QPushButton::clicked, this, &HeaderBar::globalLockRequested);
    layout->addWidget(btnLock, 0, Qt::AlignCenter);

    layout->addStretch();

    // 筛选
    m_btnFilter = new QPushButton();
    m_btnFilter->setIcon(IconHelper::getIcon("filter", "#aaaaaa", 18));
    m_btnFilter->setIconSize(QSize(18, 18));
    m_btnFilter->setProperty("tooltipText", "高级筛选 (Ctrl+G)"); m_btnFilter->installEventFilter(this);
    m_btnFilter->setStyleSheet(funcBtnStyle + " QPushButton:checked { background-color: rgba(255, 255, 255, 0.1); }");
    m_btnFilter->setCheckable(true);
    connect(m_btnFilter, &QPushButton::clicked, this, &HeaderBar::filterRequested);
    layout->addWidget(m_btnFilter, 0, Qt::AlignCenter);
    layout->addSpacing(4);

    // 元数据
    m_btnMeta = new QPushButton();
    m_btnMeta->setIcon(IconHelper::getIcon("sidebar_right", "#aaaaaa", 18));
    m_btnMeta->setIconSize(QSize(18, 18));
    m_btnMeta->setProperty("tooltipText", "元数据面板 (Ctrl+I)"); m_btnMeta->installEventFilter(this);
    m_btnMeta->setCheckable(true);
    m_btnMeta->setStyleSheet(funcBtnStyle + " QPushButton:checked { background-color: rgba(255, 255, 255, 0.1); }");
    connect(m_btnMeta, &QPushButton::toggled, this, &HeaderBar::metadataToggled);
    layout->addWidget(m_btnMeta, 0, Qt::AlignCenter);
    layout->addSpacing(4);

    // 新建
    QPushButton* btnAdd = new QPushButton();
    btnAdd->setIcon(IconHelper::getIcon("add", "#aaaaaa", 18));
    btnAdd->setIconSize(QSize(18, 18));
    btnAdd->setProperty("tooltipText", "新建数据"); btnAdd->installEventFilter(this);
    btnAdd->setStyleSheet(funcBtnStyle + " QPushButton::menu-indicator { width: 0px; image: none; }");

    QMenu* addMenu = new QMenu(this);
    IconHelper::setupMenu(addMenu);
    addMenu->setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                           "QMenu::item { padding: 6px 10px 6px 10px; border-radius: 3px; } "
                           "QMenu::icon { margin-left: 6px; } "
                           "QMenu::item:selected { background-color: #3E3E42; }");
    addMenu->addAction(IconHelper::getIcon("add", "#aaaaaa", 18), "新建数据", [this](){ emit newNoteRequested(); });
    btnAdd->setMenu(addMenu);
    connect(btnAdd, &QPushButton::clicked, [btnAdd](){ btnAdd->showMenu(); });
    layout->addWidget(btnAdd, 0, Qt::AlignCenter);
    layout->addSpacing(4);

    // 置顶
    m_btnStayOnTop = new QPushButton();
    m_btnStayOnTop->setObjectName("btnStayOnTop");
    m_btnStayOnTop->setIcon(IconHelper::getIcon("pin_tilted", "#aaaaaa", 18));
    m_btnStayOnTop->setIconSize(QSize(18, 18));
    m_btnStayOnTop->setProperty("tooltipText", "始终最前 （Alt + Q）"); m_btnStayOnTop->installEventFilter(this);
    m_btnStayOnTop->setCheckable(true);
    m_btnStayOnTop->setStyleSheet(funcBtnStyle + " QPushButton:checked { background-color: rgba(255, 255, 255, 0.1); }");
    connect(m_btnStayOnTop, &QPushButton::toggled, this, [this](bool checked){
        m_btnStayOnTop->setIcon(IconHelper::getIcon(checked ? "pin_vertical" : "pin_tilted", checked ? "#FF551C" : "#aaaaaa", 18));
        emit stayOnTopRequested(checked);
    });
    layout->addWidget(m_btnStayOnTop, 0, Qt::AlignCenter);
    layout->addSpacing(4);

    // 窗口控制
    auto addWinBtn = [&](const QString& icon, const QString& hoverColor, auto signal) {
        QPushButton* btn = new QPushButton();
        btn->setIcon(IconHelper::getIcon(icon, "#aaaaaa", 18));
        btn->setIconSize(QSize(18, 18));
        btn->setFixedSize(24, 24);
        btn->setStyleSheet(QString("QPushButton { background: transparent; border: none; border-radius: 4px; } QPushButton:hover { background: %1; }").arg(hoverColor));
        connect(btn, &QPushButton::clicked, this, signal);
        layout->addWidget(btn, 0, Qt::AlignCenter);
    };
    addWinBtn("minimize", "rgba(255,255,255,0.1)", &HeaderBar::windowMinimize);
    layout->addSpacing(4);
    addWinBtn("maximize", "rgba(255,255,255,0.1)", &HeaderBar::windowMaximize);
    layout->addSpacing(4);
    addWinBtn("close", "#e81123", &HeaderBar::windowClose);

    mainLayout->addWidget(topContent);

    // 分割线
    auto* bottomLine = new QFrame();
    bottomLine->setFrameShape(QFrame::HLine);
    bottomLine->setFixedHeight(1);
    bottomLine->setStyleSheet("background-color: #333333; border: none; margin: 0px;");
    mainLayout->addWidget(bottomLine);
}

void HeaderBar::setFilterActive(bool active) { m_btnFilter->setChecked(active); }
void HeaderBar::setMetadataActive(bool active) { m_btnMeta->setChecked(active); }

void HeaderBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (auto* win = window()) {
            if (auto* handle = win->windowHandle()) {
                handle->startSystemMove();
            }
        }
        event->accept();
    }
}

bool HeaderBar::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::ToolTip || event->type() == QEvent::HoverEnter) {
        QString text = watched->property("tooltipText").toString();
        if (!text.isEmpty()) ToolTipOverlay::instance()->showText(QCursor::pos(), text, 2000);
        return event->type() == QEvent::ToolTip;
    } else if (event->type() == QEvent::HoverLeave) {
        ToolTipOverlay::hideTip();
    }
    return QWidget::eventFilter(watched, event);
}

void HeaderBar::mouseMoveEvent(QMouseEvent* event) { QWidget::mouseMoveEvent(event); }
void HeaderBar::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) { emit windowMaximize(); event->accept(); }
}
