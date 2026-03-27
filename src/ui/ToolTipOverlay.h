#ifndef TOOL_TIP_OVERLAY_H
#define TOOL_TIP_OVERLAY_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QPointer>

namespace ArcMeta {

class ToolTipOverlay : public QWidget {
    Q_OBJECT
public:
    static ToolTipOverlay& instance();

    // 显示提示文字
    void showTip(const QPoint& globalPos, const QString& text, int timeout = 700);

    // 隐藏提示
    void hideTip();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    explicit ToolTipOverlay(QWidget* parent = nullptr);
    ~ToolTipOverlay() = default;
    ToolTipOverlay(const ToolTipOverlay&) = delete;
    ToolTipOverlay& operator=(const ToolTipOverlay&) = delete;

    QLabel* m_label;
    QTimer* m_hideTimer;
};

} // namespace ArcMeta

#endif // TOOL_TIP_OVERLAY_H
