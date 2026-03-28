#ifndef CLIPBOARDMONITOR_H
#define CLIPBOARDMONITOR_H

#include <QObject>
#include <QVariantMap>

class ClipboardMonitor : public QObject {
    Q_OBJECT
public:
    static ClipboardMonitor& instance() {
        static ClipboardMonitor inst;
        return inst;
    }
    void skipNext() {}
signals:
    void newContentDetected(const QString& content, const QString& type, const QByteArray& data, const QString& sourceApp, const QString& sourceTitle);
private:
    ClipboardMonitor() = default;
};

#endif // CLIPBOARDMONITOR_H
