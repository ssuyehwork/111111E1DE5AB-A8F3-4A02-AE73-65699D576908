#include "HeaderBar.h"
#include "StringUtils.h"
#include "MainWindow.h"
#include "IconHelper.h"
#include "ToolTipOverlay.h"
#include <QHBoxLayout>
#include <QSettings>
#include <QMouseEvent>
#include <QApplication>
#include <QWindow>
#include <QIntValidator>

HeaderBar::HeaderBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(36); 
    setStyleSheet("background-color: #252526; border: none;");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* topContent = new QWidget();
    auto* layout = new QHBoxLayout(topContent);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignVCenter);
    
    QLabel* appLogo = new QLabel();
    appLogo->setPixmap(IconHelper::getIcon("zap", "#4a90e2", 18).pixmap(18, 18));
    layout->addWidget(appLogo);
    layout->addSpacing(6);

    QLabel* titleLabel = new QLabel("ArcMeta");
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #4a90e2;");
    layout->addWidget(titleLabel);
    layout->addSpacing(15);

    m_searchEdit = new SearchLineEdit();
    m_searchEdit->setPlaceholderText("全局搜索文件...");
    m_searchEdit->setFixedWidth(280);
    m_searchEdit->setFixedHeight(24);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &HeaderBar::searchChanged);
    layout->addWidget(m_searchEdit);
    layout->addSpacing(15);

    auto createBtn = [&](const QString& icon, const QString& tip) {
        QPushButton* btn = new QPushButton();
        btn->setIcon(IconHelper::getIcon(icon, "#aaaaaa", 16));
        btn->setFixedSize(24, 24);
        btn->setStyleSheet("QPushButton { background: transparent; border: none; border-radius: 4px; } QPushButton:hover { background: rgba(255,255,255,0.1); }");
        btn->setProperty("customToolTip", tip);
        btn->installEventFilter(this);
        return btn;
    };

    layout->addWidget(createBtn("nav_first", "第一页"));
    layout->addWidget(createBtn("nav_prev", "上一页"));
    
    m_pageInput = new QLineEdit("1");
    m_pageInput->setFixedWidth(40);
    m_pageInput->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_pageInput);
    
    m_totalPageLabel = new QLabel("/ 1");
    m_totalPageLabel->setStyleSheet("color: #888; font-size: 12px;");
    layout->addWidget(m_totalPageLabel);
    
    layout->addWidget(createBtn("nav_next", "下一页"));
    layout->addWidget(createBtn("nav_last", "最后一页"));
    
    QPushButton* btnRefresh = createBtn("refresh", "刷新");
    connect(btnRefresh, &QPushButton::clicked, this, &HeaderBar::refreshRequested);
    layout->addWidget(btnRefresh);

    layout->addStretch();

    m_btnFilter = new QPushButton();
    m_btnFilter->setIcon(IconHelper::getIcon("filter", "#aaaaaa", 18));
    m_btnFilter->setCheckable(true);
    m_btnFilter->setFixedSize(24, 24);
    connect(m_btnFilter, &QPushButton::clicked, this, &HeaderBar::filterRequested);
    layout->addWidget(m_btnFilter);
    layout->addSpacing(4);

    m_btnMeta = new QPushButton();
    m_btnMeta->setIcon(IconHelper::getIcon("sidebar_right", "#aaaaaa", 18));
    m_btnMeta->setCheckable(true);
    m_btnMeta->setFixedSize(24, 24);
    connect(m_btnMeta, &QPushButton::toggled, this, &HeaderBar::metadataToggled);
    layout->addWidget(m_btnMeta);
    layout->addSpacing(4);

    m_btnStayOnTop = new QPushButton();
    m_btnStayOnTop->setObjectName("btnStayOnTop");
    m_btnStayOnTop->setIcon(IconHelper::getIcon("pin_tilted", "#aaaaaa", 18));
    m_btnStayOnTop->setCheckable(true);
    m_btnStayOnTop->setFixedSize(24, 24);
    connect(m_btnStayOnTop, &QPushButton::toggled, this, [this](bool checked){
        m_btnStayOnTop->setIcon(IconHelper::getIcon(checked ? "pin_vertical" : "pin_tilted", checked ? "#FF551C" : "#aaaaaa", 18));
        emit stayOnTopRequested(checked);
    });
    layout->addWidget(m_btnStayOnTop);
    layout->addSpacing(10);

    auto addWinBtn = [&](const QString& icon, const QString& hColor, auto sig) {
        QPushButton* btn = new QPushButton();
        btn->setIcon(IconHelper::getIcon(icon, "#aaaaaa", 18));
        btn->setFixedSize(24, 24);
        btn->setStyleSheet(QString("QPushButton { background: transparent; border: none; border-radius: 4px; } QPushButton:hover { background: %1; }").arg(hColor));
        connect(btn, &QPushButton::clicked, this, sig);
        layout->addWidget(btn);
    };

    addWinBtn("close", "#e81123", &HeaderBar::windowClose);
    layout->addSpacing(4);
    addWinBtn("maximize", "rgba(255,255,255,0.1)", &HeaderBar::windowMaximize);
    layout->addSpacing(4);
    addWinBtn("minimize", "rgba(255,255,255,0.1)", &HeaderBar::windowMinimize);

    mainLayout->addWidget(topContent);
}

void HeaderBar::updatePagination(int c, int t) {
    m_currentPage = c; m_totalPages = t;
    m_pageInput->setText(QString::number(c));
    m_totalPageLabel->setText(QString("/ %1").arg(t));
}
void HeaderBar::setFilterActive(bool a) { m_btnFilter->setChecked(a); }
void HeaderBar::setMetadataActive(bool a) { m_btnMeta->setChecked(a); }
void HeaderBar::updateToolboxStatus(bool) {}
void HeaderBar::focusSearch() { m_searchEdit->setFocus(); }
void HeaderBar::mousePressEvent(QMouseEvent* e) { if (e->button() == Qt::LeftButton && window()->windowHandle()) window()->windowHandle()->startSystemMove(); }
void HeaderBar::mouseMoveEvent(QMouseEvent* e) { QWidget::mouseMoveEvent(e); }
void HeaderBar::mouseDoubleClickEvent(QMouseEvent* e) { if (e->button() == Qt::LeftButton) emit windowMaximize(); }
bool HeaderBar::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::ToolTip) {
        auto* btn = qobject_cast<QPushButton*>(watched);
        if (btn) {
            QString tip = btn->property("customToolTip").toString();
            if (!tip.isEmpty()) {
                ToolTipOverlay::instance().showTip(btn, tip, 2000);
                return true; // 拦截原生 ToolTip
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
