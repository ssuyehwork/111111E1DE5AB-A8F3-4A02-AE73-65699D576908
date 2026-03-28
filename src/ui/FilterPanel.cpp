#include "FilterPanel.h"
#include "IconHelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

FilterPanel::FilterPanel(QWidget* parent) : QWidget(parent) {
    auto* l = new QVBoxLayout(this);
    l->setContentsMargins(10, 10, 10, 10);

    m_tree = new QTreeWidget();
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(15);
    m_tree->setStyleSheet("QTreeWidget { background: transparent; border: none; color: #CCC; }");
    
    auto addRoot = [&](const QString& label, const QString& icon) {
        auto* item = new QTreeWidgetItem(m_tree);
        item->setText(0, label);
        item->setIcon(0, IconHelper::getIcon(icon, "#aaaaaa"));
        item->setExpanded(true);
        return item;
    };

    addRoot("文件类型", "folder");
    addRoot("星级评分", "star_filled");
    addRoot("颜色标记", "palette");
    addRoot("标签过滤", "tag");

    l->addWidget(m_tree);

    m_btnReset = new QPushButton("重置筛选");
    connect(m_btnReset, &QPushButton::clicked, this, &FilterPanel::resetFilters);
    l->addWidget(m_btnReset);
}

void FilterPanel::updateStats(const QString&, const QString&, const QVariant&) {}
QVariantMap FilterPanel::getCheckedCriteria() const { return {}; }
void FilterPanel::resetFilters() { emit filterChanged(); }
void FilterPanel::initUI() {}
void FilterPanel::setupTree() {}
void FilterPanel::onStatsReady() {}
void FilterPanel::onItemChanged(QTreeWidgetItem*, int) {}
void FilterPanel::onItemClicked(QTreeWidgetItem*, int) {}
void FilterPanel::refreshNode(const QString&, const QList<QVariantMap>&, bool) {}
