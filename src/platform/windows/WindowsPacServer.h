#pragma once

#include <QObject>
#include <QTcpServer>

class WindowsPacServer : public QObject {
    Q_OBJECT

public:
    explicit WindowsPacServer(QObject* parent = nullptr);

    bool start(int httpPort, int pacPort);
    void stop();
    bool isRunning() const;
    QString pacUrl() const;

private:
    void handleNewConnection();

    QTcpServer server_;
    int httpPort_ = 0;
    int pacPort_ = 0;
};
