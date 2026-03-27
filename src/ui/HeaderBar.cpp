#include "HeaderBar.h"

namespace ArcMeta {

HeaderBar::HeaderBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(32); // 整体高度稍微大于按钮
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 10, 0);
    m_mainLayout->setSpacing(4);

    QLabel* logo = new QLabel("ArcMeta");
    logo->setStyleSheet("font-weight: bold; margin-left: 12px; color: #aaaaaa;");
    m_mainLayout->addWidget(logo);
    m_mainLayout->addStretch();

    initButtons();

    setStyleSheet("background-color: #1e1e1e;");
}

QPushButton* HeaderBar::createFuncBtn(const QString& iconName, const QString& tooltip) {
    QPushButton* btn = new QPushButton(this);
    btn->setFixedSize(24, 24);
    btn->setIcon(IconHelper::getIcon(iconName, "#aaaaaa", 18));
    btn->setToolTip(tooltip);
    btn->setCursor(Qt::PointingHandCursor);

    // 严格执行规范样式
    btn->setStyleSheet(
        "QPushButton {"
        "  background-color: transparent;"
        "  border-radius: 4px;"
        "  border: none;"
        "  padding: 0px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(255, 255, 255, 0.1);"
        "}"
        "QPushButton:pressed {"
        "  background-color: rgba(255, 255, 255, 0.2);"
        "}"
    );
    return btn;
}

void HeaderBar::initButtons() {
    // 物理顺序从右往左：关闭、最大化、最小化、置顶
    m_btnClose = createFuncBtn("close", "关闭");
    m_btnClose->setStyleSheet(m_btnClose->styleSheet() +
        "QPushButton:hover { background-color: #e81123; }"); // 规范要求红色

    m_btnMax = createFuncBtn("maximize", "最大化");
    m_btnMin = createFuncBtn("minimize", "最小化");
    m_btnTop = createFuncBtn("pin", "置顶");

    // 添加顺序决定显示（从左到右添加，即置顶在最左，关闭在最右）
    m_mainLayout->addWidget(m_btnTop);
    m_mainLayout->addWidget(m_btnMin);
    m_mainLayout->addWidget(m_btnMax);
    m_mainLayout->addWidget(m_btnClose);
}

} // namespace ArcMeta
