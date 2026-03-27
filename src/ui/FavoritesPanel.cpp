#include "FavoritesPanel.h"
#include <QStyle>

namespace ArcMeta {

FavoritesPanel::FavoritesPanel(QWidget* parent) : QWidget(parent) {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);

    initList();
    initDropArea();
}

void FavoritesPanel::initList() {
    m_listWidget = new QListWidget(this);
    m_listWidget->setStyleSheet("QListWidget { border: none; background: transparent; }");
    m_listWidget->setSpacing(2);

    m_mainLayout->addWidget(m_listWidget, 1);
}

void FavoritesPanel::initDropArea() {
    m_dropHint = new QLabel("拖拽文件夹或文件到此处", this);
    m_dropHint->setFixedHeight(60);
    m_dropHint->setAlignment(Qt::AlignCenter);
    m_dropHint->setStyleSheet(
        "QLabel {"
        "  border: 2px dashed #444444;"
        "  border-radius: 6px;"
        "  color: #888888;"
        "  font-size: 11px;"
        "}"
    );

    m_mainLayout->addWidget(m_dropHint);
}

} // namespace ArcMeta
