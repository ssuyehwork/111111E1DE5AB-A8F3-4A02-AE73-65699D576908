#ifndef HEADER_BAR_H
#define HEADER_BAR_H

#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include "IconHelper.h"

namespace ArcMeta {

class HeaderBar : public QWidget {
    Q_OBJECT
public:
    explicit HeaderBar(QWidget* parent = nullptr);

private:
    void initButtons();
    QPushButton* createFuncBtn(const QString& iconName, const QString& tooltip);

    QHBoxLayout* m_mainLayout;
    QPushButton* m_btnClose;
    QPushButton* m_btnMax;
    QPushButton* m_btnMin;
    QPushButton* m_btnTop;
};

} // namespace ArcMeta

#endif // HEADER_BAR_H
