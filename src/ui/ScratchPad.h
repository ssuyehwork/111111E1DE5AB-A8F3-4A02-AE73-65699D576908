#ifndef SCRATCH_PAD_H
#define SCRATCH_PAD_H

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QPushButton>

namespace ArcMeta {

class ScratchPad : public QWidget {
    Q_OBJECT
public:
    explicit ScratchPad(QWidget* parent = nullptr) : QWidget(parent) {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(5, 5, 5, 5);

        m_list = new QListWidget(this);
        m_list->setAcceptDrops(true);
        m_list->setDragDropMode(QAbstractItemView::DropOnly);
        layout->addWidget(m_list);

        auto* btnClear = new QPushButton("清空暂存篮", this);
        layout->addWidget(btnClear);

        connect(btnClear, &QPushButton::clicked, m_list, &QListWidget::clear);

        setStyleSheet("background-color: #252526; border-top: 1px solid #333;");
    }

private:
    QListWidget* m_list;
};

} // namespace ArcMeta

#endif // SCRATCH_PAD_H
