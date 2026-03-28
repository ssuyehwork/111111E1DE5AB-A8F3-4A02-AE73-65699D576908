#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>

namespace ArcMeta {

/**
 * @brief 高级筛选面板（面板六）
 */
class FilterPanel : public QWidget {
    Q_OBJECT

public:
    explicit FilterPanel(QWidget* parent = nullptr);
    ~FilterPanel() override = default;

signals:
    void filterChanged();

public slots:
    void clearAllFilters();

private:
    void initUi();
    QWidget* createFilterGroup(const QString& title, QVBoxLayout*& contentLayout);
    void addCheckboxFilter(QVBoxLayout* layout, const QString& label, int count, const QColor& iconColor = Qt::transparent);
    QFrame* createSeparator();

    QVBoxLayout* m_mainLayout = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_container = nullptr;
    QVBoxLayout* m_containerLayout = nullptr;
    QPushButton* m_btnClearAll = nullptr;

    QVBoxLayout* m_ratingLayout = nullptr;
    QVBoxLayout* m_colorLayout = nullptr;
    QVBoxLayout* m_tagLayout = nullptr;
    QVBoxLayout* m_typeLayout = nullptr;
    QVBoxLayout* m_sizeLayout = nullptr;
    QVBoxLayout* m_attrLayout = nullptr;
};

} // namespace ArcMeta
