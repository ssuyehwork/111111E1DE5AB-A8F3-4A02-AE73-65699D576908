#ifndef FILTERPANEL_H
#define FILTERPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QVariantMap>

class FilterPanel : public QWidget {
    Q_OBJECT
public:
    explicit FilterPanel(QWidget* parent = nullptr);

    void updateStats(const QString& type, const QString& key, const QVariant& val);
    QVariantMap getCheckedCriteria() const;
    void resetFilters();

signals:
    void filterChanged();

private:
    void initUI();
    void setupTree();
    void onStatsReady();
    void onItemChanged(QTreeWidgetItem* item, int column);
    void onItemClicked(QTreeWidgetItem* item, int column);
    void refreshNode(const QString& nodeKey, const QList<QVariantMap>& stats, bool expanded);

    QTreeWidget* m_tree;
    QPushButton* m_btnReset;
};

#endif // FILTERPANEL_H
