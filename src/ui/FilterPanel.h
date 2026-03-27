#ifndef FILTER_PANEL_H
#define FILTER_PANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>

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

private:
    void initSections();
    QWidget* createSection(const QString& title);

    QVBoxLayout* m_mainLayout;
    QWidget* m_container;
    QVBoxLayout* m_containerLayout;
    QPushButton* m_btnClearAll;
};

} // namespace ArcMeta

#endif // FILTER_PANEL_H
