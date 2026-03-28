#include "ToolTipOverlay.h"
#include <QVBoxLayout>
#include <QScreen>
#include <QGuiApplication>
#include <QStyleOption>
#include <QPainter>

namespace ArcMeta {

ToolTipOverlay& ToolTipOverlay::instance() {
    static ToolTipOverlay inst;
    return inst;
}

ToolTipOverlay::ToolTipOverlay() : QWidget(nullptr) {
    // 关键属性设置（红线：无边框、顶层、不占焦点、鼠标穿透、无阴影）
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::WindowTransparentForInput | Qt::NoDropShadowWindowHint |
                   Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    // 视觉表现（文档规范）
    // 背景色: #2B2B2B, 边框: 1px #B0B0B0, 圆角: 4px
    // 内边距: 水平 12px, 垂直 8px
    m_label = new QLabel(this);
    m_label->setWordWrap(true);
    m_label->setContentsMargins(12, 8, 12, 8);
    m_label->setMaximumWidth(450); // 最大宽度 450px
    m_label->setStyleSheet(
        "QLabel { "
        "  background-color: #2B2B2B; "
        "  color: #EEEEEE; "
        "  border: 1px solid #B0B0B0; "
        "  border-radius: 4px; "
        "  font-family: 'Microsoft YaHei', 'Segoe UI'; "
        "  font-size: 9pt; "
        "}"
    );

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_label);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &ToolTipOverlay::hideTip);
}

void ToolTipOverlay::showTip(const QString& text, const QPoint& pos, int timeout) {
    m_timer->stop();
    m_label->setText(text);

    // 强制布局调整以计算 size
    adjustSize();

    updatePosition(pos);

    show();
    if (timeout > 0) {
        m_timer->start(timeout);
    }
}

void ToolTipOverlay::hideTip() {
    hide();
}

/**
 * @brief 边缘保护：内置 QScreen 屏幕边界检测，防止提示框溢出
 */
void ToolTipOverlay::updatePosition(const QPoint& pos) {
    QPoint targetPos = pos + QPoint(15, 15); // 偏移坐标

    QScreen* screen = QGuiApplication::screenAt(pos);
    if (!screen) screen = QGuiApplication::primaryScreen();

    QRect screenRect = screen->availableGeometry();

    // 检查右边缘
    if (targetPos.x() + width() > screenRect.right()) {
        targetPos.setX(pos.x() - width() - 5);
    }

    // 检查下边缘
    if (targetPos.y() + height() > screenRect.bottom()) {
        targetPos.setY(pos.y() - height() - 5);
    }

    move(targetPos);
}

} // namespace ArcMeta
