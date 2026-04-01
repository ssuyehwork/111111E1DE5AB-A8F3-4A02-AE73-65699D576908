#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QSettings>

namespace ArcMeta {

/**
 * @brief 确立项目标准的无边框对话框基类
 * 遵循 Memories.md 中的架构规范
 */
class FramelessDialog : public QDialog {
    Q_OBJECT

public:
    explicit FramelessDialog(const QString& title, QWidget* parent = nullptr);
    virtual ~FramelessDialog() override = default;

    /**
     * @brief 设置置顶状态并同步 UI
     */
    void setStayOnTop(bool stay);

protected:
    // UI 组件
    QVBoxLayout* m_outerLayout = nullptr;
    QFrame* m_container = nullptr;
    QVBoxLayout* m_mainLayout = nullptr;
    QWidget* m_titleBar = nullptr;
    QWidget* m_contentArea = nullptr;

    // 系统按钮
    QPushButton* m_btnPin = nullptr;
    QPushButton* m_minBtn = nullptr;
    QPushButton* m_maxBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    // 交互辅助
    bool m_isStayOnTop = false;

    // 事件重写
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private slots:
    void toggleStayOnTop(bool checked);
    void toggleMaximize();

private:
    void initBaseUi(const QString& title);
    void loadSettings();
    void saveSettings();

    // Windows 缩放辅助
    enum ResizeEdge {
        None = 0, Top, Bottom, Left, Right,
        TopLeft, TopRight, BottomLeft, BottomRight
    };
    ResizeEdge getEdge(const QPoint& pos);
};

/**
 * @brief 符合 Frameless 架构的标准输入对话框
 */
class FramelessInputDialog : public FramelessDialog {
    Q_OBJECT
public:
    explicit FramelessInputDialog(const QString& title, const QString& label, const QString& initialText = "", QWidget* parent = nullptr);
    QString textValue() const;

private:
    QLineEdit* m_lineEdit = nullptr;
};

/**
 * @brief 符合 Frameless 架构的标准消息对话框
 */
class FramelessMessageBox : public FramelessDialog {
    Q_OBJECT
public:
    enum IconType { Info, Question, Warning, Error };
    static bool question(QWidget* parent, const QString& title, const QString& text);

private:
    explicit FramelessMessageBox(const QString& title, const QString& text, IconType type, QWidget* parent = nullptr);
};

} // namespace ArcMeta
