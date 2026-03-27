#ifndef QUICK_LOOK_H
#define QUICK_LOOK_H

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QFileInfo>
#include <QPixmap>
#include "ThumbnailProvider.h"
#include "../meta/AmMetaJson.h"
#include "../meta/SyncQueue.h"

namespace ArcMeta {

class QuickLook : public QDialog {
    Q_OBJECT
public:
    explicit QuickLook(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
        resize(800, 600);

        auto* layout = new QVBoxLayout(this);
        m_view = new QLabel();
        m_view->setAlignment(Qt::AlignCenter);
        m_view->setStyleSheet("background: #1e1e1e; color: #eee;");
        layout->addWidget(m_view);
    }

    void preview(const QString& filePath) {
        m_currentPath = filePath;
        QFileInfo fi(filePath);
        m_view->setText("Loading: " + fi.fileName());

        QPixmap thumb = ThumbnailProvider::getThumbnail(filePath, 1024);
        if (!thumb.isNull()) {
            m_view->setPixmap(thumb.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            m_view->setText(fi.fileName() + "\n(No Preview Available)");
        }

        show();
    }

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Escape) {
            close();
        }
        else if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_5) {
            int rating = event->key() - Qt::Key_0;
            applyQuickMetadata(rating);
        }
    }

private:
    void applyQuickMetadata(int rating) {
        if (m_currentPath.isEmpty()) return;

        QFileInfo fi(m_currentPath);
        QString dirPath = fi.absolutePath();
        AmMeta meta = AmMetaJson::load(dirPath);

        ItemMetadata item = meta.items.value(fi.fileName());
        item.rating = rating;
        meta.items[fi.fileName()] = item;

        if (AmMetaJson::save(dirPath, meta)) {
            SyncQueue::instance().enqueue(dirPath);
        }
    }

    QLabel* m_view;
    QString m_currentPath;
};

} // namespace ArcMeta

#endif // QUICK_LOOK_H
