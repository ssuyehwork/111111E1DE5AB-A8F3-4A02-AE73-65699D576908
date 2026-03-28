#ifndef TAGCAPSULE_H
#define TAGCAPSULE_H

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

class TagCapsule : public QWidget {
    Q_OBJECT
public:
    TagCapsule(const QString& text, QWidget* parent = nullptr) : QWidget(parent) {
        auto* l = new QHBoxLayout(this);
        l->setContentsMargins(8, 2, 8, 2);
        l->setSpacing(4);
        setStyleSheet("TagCapsule { background-color: #333; border-radius: 11px; } QLabel { color: #eee; font-size: 11px; border: none; }");

        auto* label = new QLabel(text);
        l->addWidget(label);

        auto* btn = new QPushButton("×");
        btn->setFixedSize(14, 14);
        btn->setStyleSheet("QPushButton { border: none; color: #888; font-weight: bold; } QPushButton:hover { color: #ff4d4f; }");
        connect(btn, &QPushButton::clicked, this, [this, text](){ emit removeRequested(text); });
        l->addWidget(btn);
    }
signals:
    void removeRequested(const QString& tag);
};

#endif
