#include "FavoritesPanel.h"
#include <QStyle>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QUrl>
#include "../db/FavoritesRepo.h"

namespace ArcMeta {

FavoritesPanel::FavoritesPanel(QWidget* parent) : QWidget(parent) {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);

    setAcceptDrops(true);

    initList();
    initDropArea();

    refreshList();
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

void FavoritesPanel::refreshList() {
    m_listWidget->clear();
    auto list = FavoritesRepo::getAll();
    for (const auto& fav : list) {
        auto* item = new QListWidgetItem(fav.name, m_listWidget);
        item->setData(Qt::UserRole, fav.path);
        // 此处可根据 fav.type 设置图标
    }
}

void FavoritesPanel::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FavoritesPanel::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                Favorite fav;
                fav.path = path;
                fav.name = QFileInfo(path).fileName();
                fav.type = QFileInfo(path).isDir() ? "folder" : "file";
                FavoritesRepo::add(fav);
            }
        }
        refreshList();
        event->acceptProposedAction();
    }
}

} // namespace ArcMeta
