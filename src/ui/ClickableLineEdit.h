#ifndef CLICKABLELINEEDIT_H
#define CLICKABLELINEEDIT_H

#include <QLineEdit>
#include <QMouseEvent>

class ClickableLineEdit : public QLineEdit {
    Q_OBJECT
public:
    using QLineEdit::QLineEdit;
protected:
    void mousePressEvent(QMouseEvent* event) override {
        QLineEdit::mousePressEvent(event);
        emit clicked();
    }
signals:
    void clicked();
};

#endif
