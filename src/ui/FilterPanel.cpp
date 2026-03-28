#include "FilterPanel.h"
#include <QScrollBar>
#include <QSlider>

namespace ArcMeta {

/**
 * @brief 构造函数，设置面板属性
 */
FilterPanel::FilterPanel(QWidget* parent)
    : QWidget(parent) {
    // 设置固定宽度（遵循文档：筛选面板 230px）
    setFixedWidth(230);
    setStyleSheet("QWidget { background-color: #1E1E1E; color: #EEEEEE; border: none; }");

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    initUi();
}

/**
 * @brief 初始化整体 UI 结构
 */
void FilterPanel::initUi() {
    // 面板标题
    QLabel* titleLabel = new QLabel("智能筛选", this);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #888; padding: 10px 12px; background: #252525;");
    m_mainLayout->addWidget(titleLabel);

    // 顶部清除按钮区
    QWidget* topBar = new QWidget(this);
    topBar->setFixedHeight(40);
    QHBoxLayout* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(12, 0, 12, 0);

    m_btnClearAll = new QPushButton("清除所有筛选", topBar);
    m_btnClearAll->setFixedHeight(24);
    m_btnClearAll->setCursor(Qt::PointingHandCursor);
    m_btnClearAll->setStyleSheet(
        "QPushButton { background: #2B2B2B; border: 1px solid #333333; border-radius: 4px; font-size: 11px; color: #B0B0B0; }"
        "QPushButton:hover { background: #333333; color: #EEEEEE; }"
    );
    connect(m_btnClearAll, &QPushButton::clicked, this, &FilterPanel::clearAllFilters);
    topLayout->addWidget(m_btnClearAll);
    m_mainLayout->addWidget(topBar);

    // 滚动区
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    m_container = new QWidget(m_scrollArea);
    m_containerLayout = new QVBoxLayout(m_container);
    m_containerLayout->setContentsMargins(12, 0, 12, 12);
    m_containerLayout->setSpacing(16);

    // 1. 星级筛选
    m_containerLayout->addWidget(createFilterGroup("星级", m_ratingLayout));
    addCheckboxFilter(m_ratingLayout, "无星级", 0);
    addCheckboxFilter(m_ratingLayout, "★", 0);
    addCheckboxFilter(m_ratingLayout, "★★", 0);
    addCheckboxFilter(m_ratingLayout, "★★★", 0);
    addCheckboxFilter(m_ratingLayout, "★★★★", 0);
    addCheckboxFilter(m_ratingLayout, "★★★★★", 0);
    addCheckboxFilter(m_ratingLayout, "★★★ 及以上", 0);

    // 2. 颜色标记
    m_containerLayout->addWidget(createFilterGroup("颜色标记", m_colorLayout));
    addCheckboxFilter(m_colorLayout, "红色", 0, QColor("#E24B4A"));
    addCheckboxFilter(m_colorLayout, "橙色", 0, QColor("#EF9F27"));
    addCheckboxFilter(m_colorLayout, "蓝色", 0, QColor("#378ADD"));

    // 3. 标签
    m_containerLayout->addWidget(createFilterGroup("标签", m_tagLayout));
    addCheckboxFilter(m_tagLayout, "(当前无标签)", 0);

    // 4. 文件类型
    m_containerLayout->addWidget(createFilterGroup("类型", m_typeLayout));
    addCheckboxFilter(m_typeLayout, "文件夹", 0);
    addCheckboxFilter(m_typeLayout, "图片", 0);

    // 5. 文件大小
    m_containerLayout->addWidget(createFilterGroup("大小 (MB)", m_sizeLayout));
    QSlider* sizeSlider = new QSlider(Qt::Horizontal, m_container);
    sizeSlider->setRange(0, 1024);
    sizeSlider->setStyleSheet(
        "QSlider::groove:horizontal { border: 1px solid #333; height: 4px; background: #2B2B2B; margin: 2px 0; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #378ADD; border: none; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }"
    );
    m_sizeLayout->addWidget(sizeSlider);
    QLabel* lblSizeVal = new QLabel("0 MB - 1024 MB", m_container);
    lblSizeVal->setStyleSheet("font-size: 11px; color: #5F5E5A;");
    m_sizeLayout->addWidget(lblSizeVal);

    // 6. 其它属性
    m_containerLayout->addWidget(createFilterGroup("属性", m_attrLayout));
    addCheckboxFilter(m_attrLayout, "仅显示置顶", 0);
    addCheckboxFilter(m_attrLayout, "仅显示加密", 0);

    m_containerLayout->addStretch();
    m_scrollArea->setWidget(m_container);
    m_mainLayout->addWidget(m_scrollArea);
}

QWidget* FilterPanel::createFilterGroup(const QString& title, QVBoxLayout*& contentLayout) {
    QWidget* g = new QWidget(m_container);
    QVBoxLayout* gl = new QVBoxLayout(g);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(4);

    QLabel* lt = new QLabel(title, g);
    lt->setStyleSheet("font-size: 11px; font-weight: 500; color: #5F5E5A; margin-bottom: 4px;");
    gl->addWidget(lt);

    QWidget* ca = new QWidget(g);
    contentLayout = new QVBoxLayout(ca);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    
    gl->addWidget(ca);
    return g;
}

void FilterPanel::addCheckboxFilter(QVBoxLayout* layout, const QString& label, int count, const QColor& iconColor) {
    QWidget* r = new QWidget(m_container);
    r->setFixedHeight(24);
    QHBoxLayout* rl = new QHBoxLayout(r);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(6);

    QCheckBox* chk = new QCheckBox(r);
    chk->setCursor(Qt::PointingHandCursor);
    chk->setStyleSheet("QCheckBox::indicator { width: 14px; height: 14px; }");
    connect(chk, &QCheckBox::checkStateChanged, this, &FilterPanel::filterChanged);

    if (iconColor != Qt::transparent) {
        QLabel* d = new QLabel(r);
        d->setFixedSize(8, 8);
        d->setStyleSheet(QString("background-color: %1; border-radius: 4px;").arg(iconColor.name()));
        rl->addWidget(d);
    }

    QLabel* lt = new QLabel(label, r);
    lt->setStyleSheet(label.contains("★") ? "font-size: 12px; color: #EF9F27;" : "font-size: 12px; color: #EEEEEE;");
    
    QLabel* lc = new QLabel(QString::number(count), r);
    lc->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lc->setStyleSheet("font-size: 11px; color: #5F5E5A;");

    rl->addWidget(chk);
    rl->addWidget(lt);
    rl->addStretch();
    rl->addWidget(lc);
    
    layout->addWidget(r);
}

void FilterPanel::clearAllFilters() {
    QList<QCheckBox*> checks = m_container->findChildren<QCheckBox*>();
    for (auto c : checks) c->setChecked(false);
    emit filterChanged();
}

QFrame* FilterPanel::createSeparator() {
    QFrame* l = new QFrame(this);
    l->setFrameShape(QFrame::HLine);
    l->setFixedHeight(1);
    l->setStyleSheet("background-color: #333333; border: none;");
    return l;
}

} // namespace ArcMeta
