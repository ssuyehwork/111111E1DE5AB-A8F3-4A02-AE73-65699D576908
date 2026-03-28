#pragma once

#include <QWidget>
#include <QLabel>
#include <QTimer>

namespace ArcMeta {

/**
 * @brief 自定义 ToolTip 悬浮窗口
 * 符合文档：#2B2B2B 背景, 0 延时显隐, 鼠标穿透, 屏幕边界保护
 */
class ToolTipOverlay : public QWidget {
    Q_OBJECT

public:
    static ToolTipOverlay& instance();

    /**
     * @brief 在指定位置显示提示
     * @param text 提示内容（支持 HTML）
     * @param pos 触发位置（通常为光标坐标）
     * @param timeout 持续时间（ms）
     */
    void showTip(const QString& text, const QPoint& pos, int timeout = 700);
    
    void hideTip();

private:
    ToolTipOverlay();
    ~ToolTipOverlay() override = default;

    void updatePosition(const QPoint& pos);

    QLabel* m_label = nullptr;
    QTimer* m_timer = nullptr;
};

} // namespace ArcMeta
