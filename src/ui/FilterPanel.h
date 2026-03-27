#ifndef FILTER_PANEL_H
#define FILTER_PANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QList>
#include "FilterEngine.h"

namespace ArcMeta {

class FilterPanel : public QWidget {
    Q_OBJECT
public:
    explicit FilterPanel(QWidget* parent = nullptr);

signals:
    void filterChanged(const FilterState& state);

private slots:
    void onCheckboxToggled();
    void clearAllFilters();

private:
    void initSections();
    QWidget* createSection(const QString& title);

    QVBoxLayout* m_mainLayout;
    QWidget* m_container;
    QVBoxLayout* m_containerLayout;
    QPushButton* m_btnClearAll;

    QList<QCheckBox*> m_starCheckboxes;
    QList<QCheckBox*> m_colorCheckboxes;
    QCheckBox* m_chkOnlyPinned;
    QCheckBox* m_chkOnlyEncrypted;
};

} // namespace ArcMeta

#endif // FILTER_PANEL_H
