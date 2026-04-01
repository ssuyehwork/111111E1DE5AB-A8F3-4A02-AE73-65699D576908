#include "FramelessDialog.h"
#include "UiHelper.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QApplication>

namespace ArcMeta {

// ============================================================================
// FramelessDialog 基类实现
// ============================================================================
FramelessDialog::FramelessDialog(const QString& title, QWidget* parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Window)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

    setWindowTitle(title);

    m_outerLayout = new QVBoxLayout(this);
    m_outerLayout->setContentsMargins(0, 0, 0, 0);

    m_container = new QWidget(this);
    m_container->setObjectName("DialogContainer");
    m_container->setAttribute(Qt::WA_StyledBackground);
    m_container->setStyleSheet(
        "#DialogContainer {"
        "  background-color: #1E1E1E;"
        "  border: 1px solid #333333;"
        "  border-radius: 12px;"
        "}"
    );
    m_outerLayout->addWidget(m_container);

    m_mainLayout = new QVBoxLayout(m_container);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // --- 标题栏 ---
    auto* titleBar = new QWidget();
    titleBar->setObjectName("TitleBar");
    titleBar->setFixedHeight(38);
    // 标题栏背景保持与主界面一致的深色，作为整体容器的一部分
    titleBar->setStyleSheet("background-color: transparent; border-bottom: 1px solid #2D2D2D;");
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 5, 0);
    titleLayout->setSpacing(4);

    m_titleLabel = new QLabel(title);
    m_titleLabel->setStyleSheet("color: #AAAAAA; font-size: 12px; font-weight: bold; border: none;");
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();

    // 严格遵循 Memories.md：按钮尺寸 24x24 px，外边距 2px，圆角 4px
    auto createTitleBtn = [&](const QString& iconKey, const QString& tip) {
        QPushButton* btn = new QPushButton();
        btn->setFixedSize(24, 24);
        btn->setIcon(UiHelper::getIcon(iconKey, QColor("#888888"), 16));
        btn->setIconSize(QSize(16, 16));
        btn->setAutoDefault(false);
        btn->setProperty("tooltipText", tip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { background: transparent; border: none; border-radius: 4px; margin: 2px; } "
            "QPushButton:hover { background-color: #3E3E42; } "
            "QPushButton:pressed { background-color: rgba(255, 255, 255, 0.15); }"
        );
        btn->installEventFilter(this);
        return btn;
    };

    m_btnPin = createTitleBtn("pin_tilted", "置顶");
    m_btnPin->setCheckable(true);
    connect(m_btnPin, &QPushButton::toggled, this, &FramelessDialog::toggleStayOnTop);
    titleLayout->addWidget(m_btnPin);

    m_minBtn = createTitleBtn("minimize", "最小化");
    connect(m_minBtn, &QPushButton::clicked, this, &QDialog::showMinimized);
    titleLayout->addWidget(m_minBtn);

    m_maxBtn = createTitleBtn("maximize", "最大化");
    connect(m_maxBtn, &QPushButton::clicked, this, &FramelessDialog::toggleMaximize);
    titleLayout->addWidget(m_maxBtn);

    m_closeBtn = createTitleBtn("close", "关闭");
    m_closeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 4px; margin: 2px; } "
        "QPushButton:hover { background-color: #E81123; } "
        "QPushButton:pressed { background-color: rgba(255, 255, 255, 0.15); }"
    );
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    titleLayout->addWidget(m_closeBtn);

    m_mainLayout->addWidget(titleBar);

    m_contentArea = new QWidget();
    m_contentArea->setObjectName("DialogContentArea");
    m_contentArea->setStyleSheet("QWidget#DialogContentArea { background: transparent; border: none; }");
    m_mainLayout->addWidget(m_contentArea, 1);
}

void FramelessDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
}

void FramelessDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // 判定是否在标题栏区域拖拽
        QWidget* child = childAt(event->pos());
        if (child) {
            bool inTitleBar = false;
            QWidget* p = child;
            while (p && p != m_container) {
                if (p->objectName() == "TitleBar") {
                    inTitleBar = true;
                    break;
                }
                p = p->parentWidget();
            }

            // 排除掉标题栏上的按钮
            if (inTitleBar && !qobject_cast<QPushButton*>(child)) {
                m_isDragging = true;
                m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
                event->accept();
                return;
            }
        }
    }
    QDialog::mousePressEvent(event);
}

void FramelessDialog::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void FramelessDialog::mouseReleaseEvent(QMouseEvent* event) {
    m_isDragging = false;
    QDialog::mouseReleaseEvent(event);
}

void FramelessDialog::toggleStayOnTop(bool checked) {
    if (checked) {
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
        m_btnPin->setIcon(UiHelper::getIcon("pin_vertical", QColor("#FF551C"), 16));
        // 保持 Checked 状态下的背景显示
        m_btnPin->setStyleSheet(
            "QPushButton { background-color: #3E3E42; border: none; border-radius: 4px; margin: 2px; } "
            "QPushButton:hover { background-color: #4E4E52; } "
            "QPushButton:pressed { background-color: rgba(255, 255, 255, 0.15); }"
        );
    } else {
        setWindowFlag(Qt::WindowStaysOnTopHint, false);
        m_btnPin->setIcon(UiHelper::getIcon("pin_tilted", QColor("#888888"), 16));
        m_btnPin->setStyleSheet(
            "QPushButton { background: transparent; border: none; border-radius: 4px; margin: 2px; } "
            "QPushButton:hover { background-color: #3E3E42; } "
            "QPushButton:pressed { background-color: rgba(255, 255, 255, 0.15); }"
        );
    }
    show(); // 修改 WindowFlags 后必须调用 show()
}

void FramelessDialog::toggleMaximize() {
    if (isMaximized()) {
        showNormal();
        m_maxBtn->setIcon(UiHelper::getIcon("maximize", QColor("#888888"), 16));
    } else {
        showMaximized();
        m_maxBtn->setIcon(UiHelper::getIcon("restore", QColor("#888888"), 16));
    }
}

void FramelessDialog::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        reject();
    } else {
        QDialog::keyPressEvent(event);
    }
}

bool FramelessDialog::eventFilter(QObject* watched, QEvent* event) {
    return QDialog::eventFilter(watched, event);
}

// ============================================================================
// FramelessInputDialog 实现
// ============================================================================
FramelessInputDialog::FramelessInputDialog(const QString& title, const QString& label,
                                           const QString& initial, QWidget* parent)
    : FramelessDialog(title, parent)
{
    // 严格复原 RapidNotes 原始尺寸与间距
    resize(500, 260);
    setMinimumSize(400, 240);

    auto* layout = new QVBoxLayout(m_contentArea);
    layout->setContentsMargins(20, 15, 20, 20);
    layout->setSpacing(7);

    auto* lbl = new QLabel(label);
    lbl->setStyleSheet("color: #EEEEEE; font-size: 13px;");
    layout->addWidget(lbl);

    m_edit = new QLineEdit(initial);
    m_edit->setMinimumHeight(38);
    // 严格对齐 RapidNotes 输入框风格：深色背景 + 蓝色聚焦边框
    m_edit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #2D2D2D; border: 1px solid #444; border-radius: 4px;"
        "  padding: 0px 10px; color: white; selection-background-color: #4A90E2;"
        "  font-size: 14px;"
        "}"
        "QLineEdit:focus { border: 1px solid #4A90E2; }"
    );
    layout->addWidget(m_edit);

    connect(m_edit, &QLineEdit::returnPressed, this, &QDialog::accept);

    layout->addStretch();

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto* btnCancel = new QPushButton("取消");
    btnCancel->setFixedSize(80, 32);
    btnCancel->setCursor(Qt::PointingHandCursor);
    btnCancel->setStyleSheet(
        "QPushButton { background-color: transparent; color: #888; border: 1px solid #444; border-radius: 4px; } "
        "QPushButton:hover { color: #EEE; background-color: #333; }"
    );
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(btnCancel);

    auto* btnOk = new QPushButton("确定");
    btnOk->setFixedSize(80, 32);
    btnOk->setCursor(Qt::PointingHandCursor);
    btnOk->setStyleSheet(
        "QPushButton { background-color: #4A90E2; color: white; border: none; border-radius: 4px; font-weight: bold; } "
        "QPushButton:hover { background-color: #3E3E42; }" // 统一悬停色
    );
    connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(btnOk);

    layout->addLayout(btnLayout);

    m_edit->setFocus();
    m_edit->selectAll();
}

void FramelessInputDialog::showEvent(QShowEvent* event) {
    FramelessDialog::showEvent(event);
    QTimer::singleShot(50, m_edit, qOverload<>(&QWidget::setFocus));
}

} // namespace ArcMeta
