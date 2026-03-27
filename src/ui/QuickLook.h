#ifndef QUICK_LOOK_H
#define QUICK_LOOK_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QKeyEvent>

namespace ArcMeta {

class QuickLook : public QWidget {
    Q_OBJECT
public:
    explicit QuickLook(QWidget* parent = nullptr) : QWidget(parent, Qt::Window | Qt::FramelessWindowHint) {
        setWindowTitle("ArcMeta QuickLook");
        resize(800, 600);
        auto* layout = new QVBoxLayout(this);
        m_previewLabel = new QLabel("快速预览", this);
        m_previewLabel->setAlignment(Qt::AlignCenter);
        m_previewLabel->setStyleSheet("font-size: 24px; color: #888;");
        layout->addWidget(m_previewLabel);

        setStyleSheet("background-color: rgba(30, 30, 30, 240); border: 1px solid #444; border-radius: 8px;");
    }

    void preview(const QString& filePath) {
        m_previewLabel->setText("正在预览: " + filePath);
        show();
        raise();
        activateWindow();
    }

protected:
    void keyPressEvent(QKeyEvent* event) override {
        // 空格或 Esc 关闭预览
        if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Escape) {
            hide();
        }
    }

private:
    QLabel* m_previewLabel;
};

} // namespace ArcMeta

#endif // QUICK_LOOK_H
