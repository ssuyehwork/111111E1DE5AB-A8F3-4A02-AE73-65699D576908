#include "FilterPanel.h"
#include <QScrollArea>
#include <QLabel>
#include <QCheckBox>
#include "../db/Database.h"
#include <QSqlQuery>

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

    for (auto* cb : m_starCheckboxes) {
        if (cb->isChecked()) state.ratings.insert(cb->property("rating").toInt());
    }

    for (auto* cb : m_colorCheckboxes) {
        if (cb->isChecked()) state.colors.append(cb->property("color").toString());
    }

    for (auto* cb : m_tagCheckboxes) {
        if (cb->isChecked()) state.tags.append(cb->text());
    }

    state.onlyPinned = m_chkOnlyPinned->isChecked();
    state.onlyEncrypted = m_chkOnlyEncrypted->isChecked();

    emit filterChanged(state);
}

void FilterPanel::clearAllFilters() {
    for (auto* cb : findChildren<QCheckBox*>()) {
        cb->blockSignals(true);
        cb->setChecked(false);
        cb->blockSignals(false);
    }
    onCheckboxToggled();
}

void FilterPanel::refreshTags() {
    // 清理旧标签 Checkbox
    qDeleteAll(m_tagCheckboxes);
    m_tagCheckboxes.clear();

    QSqlQuery q("SELECT tag FROM tags ORDER BY item_count DESC LIMIT 20", Database::instance().getDb());
    while (q.next()) {
        auto* cb = new QCheckBox(q.value(0).toString());
        connect(cb, &QCheckBox::toggled, this, &FilterPanel::onCheckboxToggled);
        m_tagSection->layout()->addWidget(cb);
        m_tagCheckboxes.append(cb);
    }
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

    // 2. 颜色
    QWidget* colorSec = createSection("颜色标记");
    QStringList colorNames = {"red", "orange", "yellow", "green", "cyan", "blue", "purple", "gray"};
    for (const QString& name : colorNames) {
        auto* cb = new QCheckBox(name);
        cb->setProperty("color", name);
        connect(cb, &QCheckBox::toggled, this, &FilterPanel::onCheckboxToggled);
        colorSec->layout()->addWidget(cb);
        m_colorCheckboxes.append(cb);
    }

    // 3. 标签 (动态填充)
    m_tagSection = createSection("标签筛选");
    refreshTags();

    // 4. 置顶与加密
    QWidget* statusSec = createSection("状态筛选");
    m_chkOnlyPinned = new QCheckBox("只显示置顶项");
    m_chkOnlyEncrypted = new QCheckBox("只显示加密项");
    connect(m_chkOnlyPinned, &QCheckBox::toggled, this, &FilterPanel::onCheckboxToggled);
    connect(m_chkOnlyEncrypted, &QCheckBox::toggled, this, &FilterPanel::onCheckboxToggled);
    statusSec->layout()->addWidget(m_chkOnlyPinned);
    statusSec->layout()->addWidget(m_chkOnlyEncrypted);
}

} // namespace ArcMeta
