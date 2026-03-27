#include "FilterPanel.h"
#include <QScrollArea>

namespace ArcMeta {

FilterPanel::FilterPanel(QWidget* parent) : QWidget(parent) {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    m_btnClearAll = new QPushButton("清除所有筛选");
    m_btnClearAll->setFixedHeight(32);
    m_mainLayout->addWidget(m_btnClearAll);

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    m_container = new QWidget();
    m_containerLayout = new QVBoxLayout(m_container);
    m_containerLayout->setContentsMargins(12, 12, 12, 12);
    m_containerLayout->setSpacing(12);

    initSections();
    m_containerLayout->addStretch();

    scroll->setWidget(m_container);
    m_mainLayout->addWidget(scroll);
}

QWidget* FilterPanel::createSection(const QString& title) {
    QWidget* section = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QLabel* lbl = new QLabel(title);
    lbl->setStyleSheet("font-size: 11px; font-weight: 500; color: #888; margin-top: 5px;");
    layout->addWidget(lbl);

    m_containerLayout->addWidget(section);
    return section;
}

void FilterPanel::onCheckboxToggled() {
    FilterState state;
    // 此处应遍历各分组的 Checkbox 并填充 state
    // 为了演示逻辑，此处先建立信号结构
    emit filterChanged(state);
}

void FilterPanel::initSections() {
    // 1. 星级
    QWidget* starSec = createSection("星级筛选");
    for (int i = 5; i >= 1; --i) {
        auto* cb = new QCheckBox(QString(i, QChar(0x2605)));
        cb->setProperty("rating", i);
        connect(cb, &QCheckBox::toggled, this, &FilterPanel::onCheckboxToggled);
        starSec->layout()->addWidget(cb);
    }
    auto* cbNone = new QCheckBox("无星级");
    cbNone->setProperty("rating", 0);
    connect(cbNone, &QCheckBox::toggled, this, &FilterPanel::onCheckboxToggled);
    starSec->layout()->addWidget(cbNone);

    // 2. 颜色标记
    createSection("颜色标记");

    // 3. 标签
    createSection("标签筛选");

    // 4. 文件类型
    QWidget* typeSec = createSection("文件类型");
    typeSec->layout()->addWidget(new QCheckBox("文件夹"));

    // 5. 置顶状态
    QWidget* pinSec = createSection("置顶筛选");
    pinSec->layout()->addWidget(new QCheckBox("只显示置顶项"));
}

} // namespace ArcMeta
