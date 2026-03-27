#include "ToolTipOverlay.h"
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QGuiApplication>

namespace ArcMeta {

ToolTipOverlay& ToolTipOverlay::instance() {
    static ToolTipOverlay inst;
    return inst;
}

ToolTipOverlay::ToolTipOverlay(QWidget* parent) : QWidget(parent) {
    // 设置窗口标志：无边框、置顶、不接受输入（鼠标穿透）、不夺取焦点
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8); // 规范：水平12, 垂直8

    m_label = new QLabel(this);
    m_label->setStyleSheet("color: #EEEEEE; font-size: 9pt; background: transparent;");
    m_label->setWordWrap(true);
    m_label->setMaximumWidth(450); // 规范限制
    layout->addWidget(m_label);

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &ToolTipOverlay::hideTip);
}

void ToolTipOverlay::showTip(const QPoint& globalPos, const QString& text, int timeout) {
    m_label->setText(text);
    adjustSize();

    // 位置计算与边缘保护
    QPoint pos = globalPos + QPoint(15, 15);
    QScreen* screen = QGuiApplication::screenAt(globalPos);
    if (!screen) screen = QGuiApplication::primaryScreen();

    if (screen) {
        QRect rect = screen->geometry();
        if (pos.x() + width() > rect.right()) pos.setX(globalPos.x() - width() - 15);
        if (pos.y() + height() > rect.bottom()) pos.setY(globalPos.y() - height() - 15);
    }

    move(pos);
    show();

    if (timeout > 0) {
        m_hideTimer->start(timeout);
    }
}

void ToolTipOverlay::hideTip() {
    hide();
}

void ToolTipOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 背景：#2B2B2B, 边框：1px #B0B0B0, 圆角：4px
    QRectF rect = rectF().adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(QColor("#B0B0B0"), 1));
    painter.setBrush(QColor("#2B2B2B"));
    painter.drawRoundedRect(rect, 4, 4);
}

} // namespace ArcMeta
