#ifndef KEYBOARDHOOK_H
#define KEYBOARDHOOK_H

#include <QObject>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

class KeyboardHook : public QObject {
    Q_OBJECT
public:
    static KeyboardHook& instance();
    void start();
    void stop();
    bool isActive() const { return m_active; }

    void setDigitInterceptEnabled(bool enabled) { m_digitInterceptEnabled = enabled; }
    void setCapsLockToEnterEnabled(bool enabled) { m_capsLockToEnterEnabled = enabled; }

signals:
    void digitPressed(int digit);
    void f4PressedInExplorer();
    void globalLockRequested();
    void enterPressedInOtherApp(); // [FIX] 2026-03-xx 补全被意外还原掉的信号，解决 MainWindow 连接报错

private:
    bool m_digitInterceptEnabled = false;
    bool m_capsLockToEnterEnabled = false;
    KeyboardHook();
    ~KeyboardHook();
    bool m_active = false;

#ifdef Q_OS_WIN
    static LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam);
#endif
};

#endif // KEYBOARDHOOK_H
