#include "services/ProxyAvailabilityCheckService.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

namespace {

constexpr int kAvailabilityTimeoutMs = 30000;
const QString kLoopbackAddress = QStringLiteral("127.0.0.1");
const QString kDefaultSpeedPingUrl = QStringLiteral("https://www.google.com/generate_204");

QString formatAvailabilityMessage(qint64 elapsedMs)
{
    return QCoreApplication::translate("ProxyAvailabilityCheckService", "Availability check: %1 ms")
        .arg(elapsedMs >= 0 ? elapsedMs : -1);
}

OperationResult checkViaHttpProxy(const QUrl& url, int httpPort)
{
    QTcpSocket socket;
    socket.connectToHost(kLoopbackAddress, httpPort);
    QElapsedTimer connectTimer;
    connectTimer.start();
    while (socket.state() != QAbstractSocket::ConnectedState && connectTimer.elapsed() < kAvailabilityTimeoutMs) {
        socket.waitForConnected(50);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    if (socket.state() != QAbstractSocket::ConnectedState) {
        return OperationResult::fail(formatAvailabilityMessage(-1));
    }

    QByteArray hostHeader = url.host().trimmed().toUtf8();
    const int targetPort = url.port(80);
    if (targetPort > 0 && targetPort != 80) {
        hostHeader += ':';
        hostHeader += QByteArray::number(targetPort);
    }

    const QByteArray request =
        QByteArray("GET ")
        + url.toString(QUrl::FullyEncoded).toUtf8()
        + QByteArray(" HTTP/1.1\r\nHost: ")
        + hostHeader
        + QByteArray("\r\nUser-Agent: v2rayq-testme\r\nConnection: close\r\n\r\n");

    QElapsedTimer timer;
    timer.start();

    if (socket.write(request) < 0) {
        return OperationResult::fail(formatAvailabilityMessage(-1));
    }

    while (socket.bytesToWrite() > 0 && timer.elapsed() < kAvailabilityTimeoutMs) {
        socket.waitForBytesWritten(50);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    if (socket.bytesToWrite() > 0) {
        return OperationResult::fail(formatAvailabilityMessage(-1));
    }

    QByteArray response;
    while (timer.elapsed() < kAvailabilityTimeoutMs) {
        if (socket.bytesAvailable() > 0 || socket.waitForReadyRead(100)) {
            response.append(socket.readAll());
            if (response.contains("\r\n\r\n")) {
                break;
            }
        }

        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        if (socket.state() == QAbstractSocket::UnconnectedState) {
            response.append(socket.readAll());
            break;
        }
    }

    const qint64 elapsedMs = timer.elapsed();
    if (response.isEmpty()) {
        return OperationResult::fail(formatAvailabilityMessage(-1));
    }

    const QByteArray statusLine = response.split('\n').value(0).trimmed();
    const QList<QByteArray> parts = statusLine.split(' ');
    const int statusCode = parts.size() >= 2 ? parts.at(1).toInt() : 0;
    if (statusCode == 200 || statusCode == 204) {
        return OperationResult::ok(formatAvailabilityMessage(elapsedMs));
    }

    return OperationResult::fail(formatAvailabilityMessage(elapsedMs));
}

} // namespace

OperationResult ProxyAvailabilityCheckService::check(const Config& config) const
{
    const int httpPort = config.localPort + 1;
    if (config.localPort <= 0 || httpPort <= 0 || httpPort > 65535) {
        return OperationResult::fail(formatAvailabilityMessage(-1));
    }

    const QString urlText = config.speedPingTestUrl.trimmed().isEmpty()
        ? kDefaultSpeedPingUrl
        : config.speedPingTestUrl.trimmed();
    const QUrl url = QUrl::fromUserInput(urlText);
    if (!url.isValid() || url.scheme().trimmed().isEmpty()) {
        return OperationResult::fail(formatAvailabilityMessage(-1));
    }

    if (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0) {
        return checkViaHttpProxy(url, httpPort);
    }

    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, kLoopbackAddress, httpPort));

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("v2rayq-testme"));

    QElapsedTimer timer;
    timer.start();

    bool timedOut = false;
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QNetworkReply* reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    timeoutTimer.start(kAvailabilityTimeoutMs);
    loop.exec();
    timeoutTimer.stop();

    const qint64 elapsedMs = (!timedOut || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid())
        ? timer.elapsed()
        : -1;
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError error = reply->error();
    reply->deleteLater();

    if (!timedOut && error == QNetworkReply::NoError && (statusCode == 200 || statusCode == 204)) {
        return OperationResult::ok(formatAvailabilityMessage(elapsedMs));
    }

    return OperationResult::fail(formatAvailabilityMessage(elapsedMs));
}
