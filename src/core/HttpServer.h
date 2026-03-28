#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QObject>

class HttpServer : public QObject {
    Q_OBJECT
public:
    static HttpServer& instance() {
        static HttpServer inst;
        return inst;
    }
    void start(int port) { Q_UNUSED(port); }
private:
    HttpServer() = default;
};

#endif // HTTPSERVER_H
