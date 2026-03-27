#include "FilterPanel.h"
#include <QScrollArea>
#include <QLabel>
#include <QCheckBox>

namespace ArcMeta {

FilterPanel::FilterPanel(QWidget* parent) : QWidget(parent) {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_btnClearAll = new QPushButton("清除所有筛选");
    m_btnClearAll->setFixedHeight(32);
    m_btnClearAll->setCursor(Qt::PointingHandCursor);
    connect(m_btnClearAll, &QPushButton::clicked, this, &FilterPanel::clearAllFilters);
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

    // 收集星级
    for (auto* cb : m_starCheckboxes) {
        if (cb->isChecked()) state.ratings.insert(cb->property("rating").toInt());
    }

    // 收集颜色
    for (auto* cb : m_colorCheckboxes) {
        if (cb->isChecked()) state.colors.append(cb->property("color").toString());
    }

    // 其他开关
    state.onlyPinned = m_chkOnlyPinned->isChecked();
    state.onlyEncrypted = m_chkOnlyEncrypted->isChecked();

    emit filterChanged(state);
}

void FilterPanel::clearAllFilters() {
    for (auto* cb : findChildren<QCheckBox*>()) {
        cb->setChecked(false);
    }
    onCheckboxToggled();
}

void FilterPanel::initSections() {
    // 1. 星级
    QWidget* starSec = createSection("星级筛选");
    for (int i = 5; i >= 1; --i) {
        auto* cb = new QCheckBox(QString(i, QChar(0x2605)));
        cb->setProperty("rating", i);
        connect(cb, &QCheckBox::toggled, this, &FilterPanel::onCheckboxToggled);
        starSec->layout()->addWidget(cb);
        m_starCheckboxes.append(cb);
    }
    auto* cbNone = new QCheckBox("无星级");
    cbNone->setProperty("rating", 0);
    connect(cbNone, &QCheckBox::toggled, this, &FilterPanel::onCheckboxToggled);
    starSec->layout()->addWidget(cbNone);
    m_starCheckboxes.append(cbNone);

    // 2. 颜色标记
    QWidget* colorSec = createSection("颜色标记");
    QStringList colorNames = {"red", "orange", "yellow", "green", "cyan", "blue", "purple", "gray"};
    for (const QString& name : colorNames) {
        auto* cb = new QCheckBox(name);
        cb->setProperty("color", name);
        connect(cb, &QCheckBox::toggled, this, &FilterPanel::onCheckboxToggled);
        colorSec->layout()->addWidget(cb);
        m_colorCheckboxes.append(cb);
    }

    // 3. 置顶与加密
    QWidget* statusSec = createSection("状态筛选");
    m_chkOnlyPinned = new QCheckBox("只显示置顶项");
    m_chkOnlyEncrypted = new QCheckBox("只显示加密项");
    connect(m_chkOnlyPinned, &QCheckBox::toggled, this, &FilterPanel::onCheckboxToggled);
    connect(m_chkOnlyEncrypted, &QCheckBox::toggled, this, &FilterPanel::onCheckboxToggled);
    statusSec->layout()->addWidget(m_chkOnlyPinned);
    statusSec->layout()->addWidget(m_chkOnlyEncrypted);
}

} // namespace ArcMeta
