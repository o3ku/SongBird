#include "platform/windows/WindowsPacServer.h"

#include <QTcpSocket>

WindowsPacServer::WindowsPacServer(QObject* parent)
    : QObject(parent)
{
}

bool WindowsPacServer::start(int httpPort, int pacPort)
{
    stop();
    httpPort_ = httpPort;
    pacPort_ = pacPort;

    if (!server_.listen(QHostAddress::LocalHost, pacPort_)) {
        return false;
    }

    connect(&server_, &QTcpServer::newConnection, this, &WindowsPacServer::handleNewConnection);
    return true;
}

void WindowsPacServer::stop()
{
    if (server_.isListening()) {
        server_.close();
    }
    disconnect(&server_, &QTcpServer::newConnection, this, &WindowsPacServer::handleNewConnection);
}

bool WindowsPacServer::isRunning() const
{
    return server_.isListening();
}

QString WindowsPacServer::pacUrl() const
{
    if (!server_.isListening()) {
        return {};
    }
    return QStringLiteral("http://127.0.0.1:%1/pac").arg(server_.serverPort());
}

void WindowsPacServer::handleNewConnection()
{
    QTcpSocket* socket = server_.nextPendingConnection();
    if (!socket) {
        return;
    }

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        // Read and discard the HTTP request
        socket->readAll();

        const QString pacContent = QStringLiteral(
            "function FindProxyForURL(url, host) {\n"
            "  return \"PROXY 127.0.0.1:%1\";\n"
            "}\n").arg(httpPort_);

        const QByteArray body = pacContent.toUtf8();
        const QByteArray response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/x-ns-proxy-autoconfig\r\n"
            "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
            "Connection: close\r\n"
            "\r\n" + body;

        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
    });

    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}
