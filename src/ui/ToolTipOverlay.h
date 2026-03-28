#ifndef TOOLTIPOVERLAY_H
#define TOOLTIPOVERLAY_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

class ToolTipOverlay : public QWidget {
    Q_OBJECT
public:
    static ToolTipOverlay& instance() {
        static ToolTipOverlay inst;
        return inst;
    }

    void showTip(QWidget* parent, const QString& text, int duration = 700) {
        m_label->setText(text);
        adjustSize();

        // 简单定位在 parent 下方
        if (parent) {
            move(parent->mapToGlobal(QPoint(0, parent->height() + 5)));
        }

        show();
        m_timer->start(duration);
    }

private:
    ToolTipOverlay() : QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint) {
        setAttribute(Qt::WA_TranslucentBackground);
        setStyleSheet("background-color: #2B2B2B; color: #EEEEEE; border: 1px solid #B0B0B0; border-radius: 4px; padding: 8px;");
        auto* l = new QVBoxLayout(this);
        m_label = new QLabel();
        l->addWidget(m_label);
        m_timer = new QTimer(this);
        m_timer->setSingleShot(true);
        connect(m_timer, &QTimer::timeout, this, &ToolTipOverlay::hide);
    }
    QLabel* m_label;
    QTimer* m_timer;
};

#endif // TOOLTIPOVERLAY_H
