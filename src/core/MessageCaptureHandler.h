#ifndef MESSAGECAPTUREHANDLER_H
#define MESSAGECAPTUREHANDLER_H

#include <QObject>

class MessageCaptureHandler : public QObject {
    Q_OBJECT
public:
    explicit MessageCaptureHandler(QObject* parent = nullptr) : QObject(parent) {}
};

#endif // MESSAGECAPTUREHANDLER_H
