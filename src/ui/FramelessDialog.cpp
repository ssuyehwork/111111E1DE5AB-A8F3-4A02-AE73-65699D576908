#include "FramelessDialog.h"
#include "UiHelper.h"
#include "ToolTipOverlay.h"
#include <QApplication>
#include <QMouseEvent>
#include <QEvent>
#include <QHoverEvent>
#include <QCursor>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

namespace ArcMeta {

FramelessDialog::FramelessDialog(const QString& title, QWidget* parent)
    : QDialog(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);
    setAttribute(Qt::WA_TranslucentBackground);

    initBaseUi(title);
    loadSettings();
}

void FramelessDialog::initBaseUi(const QString& title) {
    // 1. 外层布局：提供 20px 呼吸感外边距 (Memories.md 规范)
    m_outerLayout = new QVBoxLayout(this);
    m_outerLayout->setContentsMargins(20, 20, 20, 20);
    m_outerLayout->setSpacing(0);

    // 2. 主容器：圆角 12px (Memories.md 规范)
    m_container = new QFrame(this);
    m_container->setObjectName("DialogContainer");
    m_container->setStyleSheet(
        "QFrame#DialogContainer { "
        "  background-color: #1e1e1e; "
        "  border: 1px solid #333333; "
        "  border-radius: 12px; "
        "}"
    );

    m_mainLayout = new QVBoxLayout(m_container);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // 3. 标题栏：固定高度 32px (Memories.md 规范)
    m_titleBar = new QWidget(m_container);
    m_titleBar->setObjectName("TitleBar");
    m_titleBar->setFixedHeight(32);
    m_titleBar->setStyleSheet(
        "QWidget#TitleBar { "
        "  background-color: #252526; "
        "  border-bottom: 1px solid #333333; "
        "  border-top-left-radius: 12px; "
        "  border-top-right-radius: 12px; "
        "}"
    );

    QHBoxLayout* titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(12, 0, 2, 0);
    titleLayout->setSpacing(4);

    QLabel* titleLabel = new QLabel(title, m_titleBar);
    titleLabel->setStyleSheet("color: #CCCCCC; font-size: 12px; font-weight: bold;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    // 系统按钮组：顺序为 关闭 -> 最大化 -> 最小化 -> 置顶 (Memories.md 从右到左规范)
    auto createBtn = [this](const QString& iconKey) {
        QPushButton* btn = new QPushButton(m_titleBar);
        btn->setFixedSize(28, 28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setIcon(UiHelper::getIcon(iconKey, QColor("#EEEEEE")));
        btn->setIconSize(QSize(18, 18));
        btn->installEventFilter(this); // 强制安装事件过滤器以支持 ToolTipOverlay
        btn->setStyleSheet(
            "QPushButton { background: transparent; border: none; border-radius: 4px; }"
            "QPushButton:hover { background-color: rgba(255, 255, 255, 0.1); }"
            "QPushButton:pressed { background-color: rgba(255, 255, 255, 0.2); }"
        );
        return btn;
    };

    m_btnPin = createBtn("pin_tilted");
    m_btnPin->setProperty("tooltipText", "置顶");
    m_btnPin->setCheckable(true);
    connect(m_btnPin, &QPushButton::toggled, this, &FramelessDialog::toggleStayOnTop);

    m_minBtn = createBtn("minimize");
    m_minBtn->setProperty("tooltipText", "最小化");
    connect(m_minBtn, &QPushButton::clicked, this, &QDialog::showMinimized);

    m_maxBtn = createBtn("maximize");
    m_maxBtn->setProperty("tooltipText", "最大化");
    connect(m_maxBtn, &QPushButton::clicked, this, &FramelessDialog::toggleMaximize);

    m_closeBtn = createBtn("close");
    m_closeBtn->setProperty("tooltipText", "关闭");
    // 注意：关闭按钮特有的悬停红色需要单独追加
    m_closeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #E81123; }"
        "QPushButton:pressed { background-color: #A80000; }"
    );
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    // 添加顺序：置顶 -> 最小化 -> 最大化 -> 关闭 (从左到右添加，即按钮靠右显示)
    titleLayout->addWidget(m_btnPin);
    titleLayout->addWidget(m_minBtn);
    titleLayout->addWidget(m_maxBtn);
    titleLayout->addWidget(m_closeBtn);

    m_mainLayout->addWidget(m_titleBar);

    // 4. 内容区
    m_contentArea = new QWidget(m_container);
    m_contentArea->setObjectName("DialogContentArea");
    m_mainLayout->addWidget(m_contentArea, 1);

    m_outerLayout->addWidget(m_container);

    // 安装全局提示重定向过滤器 (Memories.md 规范)
    installEventFilter(this);
}

void FramelessDialog::setStayOnTop(bool stay) {
    if (m_btnPin) {
        m_btnPin->blockSignals(true);
        m_btnPin->setChecked(stay);
        m_btnPin->blockSignals(false);
    }
    toggleStayOnTop(stay);
}

void FramelessDialog::toggleStayOnTop(bool checked) {
    m_isStayOnTop = checked;
    saveSettings();

    if (m_btnPin) {
        // 置顶态图标橙色 (Memories.md 规范)
        m_btnPin->setIcon(UiHelper::getIcon(checked ? "pin_vertical" : "pin_tilted",
                          checked ? QColor("#FF551C") : QColor("#EEEEEE")));
    }

#ifdef Q_OS_WIN
    HWND hwnd = (HWND)winId();
    SetWindowPos(hwnd, checked ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
    setWindowFlag(Qt::WindowStaysOnTopHint, checked);
    show();
#endif
}

void FramelessDialog::toggleMaximize() {
    if (isMaximized()) showNormal();
    else showMaximized();
}

void FramelessDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::WindowStateChange) {
        bool max = isMaximized();
        m_maxBtn->setIcon(UiHelper::getIcon(max ? "restore" : "maximize", QColor("#EEEEEE")));
        m_maxBtn->setProperty("tooltipText", max ? "还原" : "最大化");

        // 最大化时圆角归零，移除外边距 (Memories.md 规范)
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
    QDialog::changeEvent(event);
}

void FramelessDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
#ifdef Q_OS_WIN
    if (m_isStayOnTop) {
        HWND hwnd = (HWND)winId();
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
#endif
}

bool FramelessDialog::eventFilter(QObject* watched, QEvent* event) {
    // 统一 ToolTip 重定向 (Memories.md 规范)
    if (event->type() == QEvent::HoverEnter || event->type() == QEvent::ToolTip) {
        QString text = watched->property("tooltipText").toString();
        if (!text.isEmpty()) {
            ToolTipOverlay::instance()->showText(QCursor::pos(), text);
            return true;
        }
    } else if (event->type() == QEvent::HoverLeave || event->type() == QEvent::MouseButtonPress) {
        ToolTipOverlay::hideTip();
    }
    return QDialog::eventFilter(watched, event);
}

#ifdef Q_OS_WIN
bool FramelessDialog::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_NCHITTEST) {
        int x = GET_X_LPARAM(msg->lParam);
        int y = GET_Y_LPARAM(msg->lParam);
        QPoint pos = mapFromGlobal(QPoint(x, y));

        // 1. 边缘缩放判定 (Memories.md 规范)
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

        // 2. 标题栏拖拽判定 (排除交互控件)
        if (m_titleBar && m_titleBar->geometry().contains(pos)) {
            QWidget* child = childAt(pos);
            // 如果点击的是按钮等，不拦截
            if (child && (child->inherits("QPushButton") || child->inherits("QToolButton") || child->inherits("QLineEdit"))) {
                return false;
            }
            *result = HTCAPTION;
            return true;
        }
    }
    return QDialog::nativeEvent(eventType, message, result);
}
#endif

FramelessDialog::ResizeEdge FramelessDialog::getEdge(const QPoint& pos) {
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

void FramelessDialog::loadSettings() {
    if (objectName().isEmpty()) return;
    QSettings settings("ArcMeta团队", "WindowStates");
    bool stay = settings.value(objectName() + "/StayOnTop", false).toBool();
    setStayOnTop(stay);
}

void FramelessDialog::saveSettings() {
    if (objectName().isEmpty()) return;
    QSettings settings("ArcMeta团队", "WindowStates");
    settings.setValue(objectName() + "/StayOnTop", m_isStayOnTop);
}

} // namespace ArcMeta
