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

#include "common/PortValidator.h"
#include "common/UserAgent.h"

namespace {
constexpr int kAvailabilityTimeoutMs = 30000;
const QString kLoopbackAddress = QStringLiteral("127.0.0.1");
const QString kDefaultSpeedPingUrl = QStringLiteral("https://www.google.com/generate_204");

int resolveAvailabilityTimeoutMs(int configuredMs)
{
    return configuredMs > 0 ? configuredMs : kAvailabilityTimeoutMs;
}

QString formatAvailabilityMessage(qint64 elapsedMs)
{
    return QCoreApplication::translate("ProxyAvailabilityCheckService", "Availability check: %1 ms")
        .arg(elapsedMs >= 0 ? elapsedMs : -1);
}

OperationResult checkViaHttpProxy(const QUrl& url, int httpPort, int timeoutMs)
{
    QTcpSocket socket;
    socket.connectToHost(kLoopbackAddress, httpPort);
    QElapsedTimer connectTimer;
    connectTimer.start();
    while (socket.state() != QAbstractSocket::ConnectedState && connectTimer.elapsed() < timeoutMs) {
        socket.waitForConnected(50);
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
        + QByteArray("\r\nUser-Agent: ")
        + fallbackUserAgent().toUtf8()
        + QByteArray("\r\nConnection: close\r\n\r\n");

    QElapsedTimer timer;
    timer.start();

    if (socket.write(request) < 0) {
        return OperationResult::fail(formatAvailabilityMessage(-1));
    }

    while (socket.bytesToWrite() > 0 && timer.elapsed() < timeoutMs) {
        socket.waitForBytesWritten(50);
    }
    if (socket.bytesToWrite() > 0) {
        return OperationResult::fail(formatAvailabilityMessage(-1));
    }

    QByteArray response;
    while (timer.elapsed() < timeoutMs) {
        if (socket.bytesAvailable() > 0 || socket.waitForReadyRead(100)) {
            response.append(socket.readAll());
            if (response.contains("\r\n\r\n")) {
                break;
            }
        }

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

OperationResult ProxyAvailabilityCheckService::check(const ProxyAvailabilityCheckConfig& config) const
{
    const int httpPort = config.httpPort;
    if (!isValidTcpPort(httpPort)) {
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
        return checkViaHttpProxy(url, httpPort, resolveAvailabilityTimeoutMs(config.timeoutMs));
    }

    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, kLoopbackAddress, httpPort));

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, fallbackUserAgent());

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
    timeoutTimer.start(resolveAvailabilityTimeoutMs(config.timeoutMs));
    loop.exec();
    timeoutTimer.stop();

    const qint64 elapsedMs = (!timedOut || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid())
        ? timer.elapsed()
        : -1;
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError error = reply->error();
    // Deleted here, not deleteLater'd: this check runs on the health watch's
    // probe thread, and a deferred delete posted to a thread that never spins
    // its event loop again is dropped -- the reply, with the half-open proxy
    // connection still attached, is then retained for the process lifetime.
    // The same guarded-delete shape the outbound probe services already use.
    QObject::disconnect(reply, nullptr, nullptr, nullptr);
    if (!reply->isFinished()) {
        reply->abort();
    }
    delete reply;

    if (!timedOut && error == QNetworkReply::NoError && (statusCode == 200 || statusCode == 204)) {
        return OperationResult::ok(formatAvailabilityMessage(elapsedMs));
    }

    return OperationResult::fail(formatAvailabilityMessage(elapsedMs));
}

