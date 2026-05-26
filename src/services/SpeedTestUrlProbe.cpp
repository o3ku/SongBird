#include "services/SpeedTestUrlProbe.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#include "common/UserAgent.h"

namespace {

constexpr int kProbeRetryThresholdMs = 800;
const QString kLoopbackAddress = QStringLiteral("127.0.0.1");

SpeedTestServiceInternal::UrlProbeResult probeProxiedUrl(
    QNetworkProxy::ProxyType proxyType,
    const QString& proxyHost,
    int proxyPort,
    const QString& user,
    const QString& password,
    const QString& url,
    int timeoutMs)
{
    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy(proxyType, proxyHost, proxyPort, user, password));

    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, fallbackUserAgent());

    QElapsedTimer timer;
    timer.start();

    bool timedOut = false;
    bool headersObserved = false;
    int headerStatusCode = 0;
    qint64 headerElapsedMs = -1;
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QNetworkReply* reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::metaDataChanged, &loop, [&]() {
        if (headersObserved) {
            return;
        }

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 0) {
            return;
        }

        headersObserved = true;
        headerStatusCode = statusCode;
        headerElapsedMs = timer.elapsed();
        if (statusCode == 200 || statusCode == 204) {
            reply->abort();
            loop.quit();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    timeoutTimer.start(timeoutMs);
    loop.exec();
    timeoutTimer.stop();

    const QString errorText = reply->errorString();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    const bool successFromHeaders = !timedOut && headersObserved && (headerStatusCode == 200 || headerStatusCode == 204);
    const bool successFromFinishedReply = !timedOut && (statusCode == 200 || statusCode == 204);
    const qint64 successLatencyMs = successFromHeaders ? headerElapsedMs : timer.elapsed();
    const qint64 reportedLatencyMs = (successFromHeaders || successFromFinishedReply)
        ? successLatencyMs
        : (timedOut ? -1 : timer.elapsed());
    return SpeedTestServiceInternal::classifyUrlProbeResult(
        successFromHeaders || successFromFinishedReply,
        timedOut,
        reportedLatencyMs,
        SpeedTestUrlProbe::normalizeErrorText(errorText));
}

SpeedTestServiceInternal::UrlProbeResult probeWithRetry(
    QNetworkProxy::ProxyType proxyType,
    const QString& proxyHost,
    int proxyPort,
    const QString& user,
    const QString& password,
    const QString& url,
    int timeoutMs,
    const std::atomic_bool& cancelled)
{
    SpeedTestServiceInternal::UrlProbeResult result =
        probeProxiedUrl(proxyType, proxyHost, proxyPort, user, password, url, timeoutMs);

    if (SpeedTestUrlProbe::shouldRetry(result) && !cancelled.load()) {
        result = probeProxiedUrl(proxyType, proxyHost, proxyPort, user, password, url, timeoutMs);
    }

    return result;
}

bool isIpLiteral(const QString& host)
{
    QHostAddress address;
    return address.setAddress(host.trimmed());
}

QString normalizeUpstreamProxyErrorText(const VmessItem& server, const QString& value)
{
    const QString normalized = SpeedTestUrlProbe::normalizeErrorText(value);
    if (server.configType == ConfigType::Socks
        && isIpLiteral(server.address)
        && normalized.contains(QStringLiteral("host not found"), Qt::CaseInsensitive)) {
        return QStringLiteral("SOCKS handshake failed");
    }

    return normalized;
}

} // namespace

namespace SpeedTestUrlProbe {

QString normalizeErrorText(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("Failed");
    }

    if (trimmed.contains(QStringLiteral("timeout"), Qt::CaseInsensitive)
        || trimmed.contains(QStringLiteral("timed out"), Qt::CaseInsensitive)) {
        return QStringLiteral("Timeout");
    }

    const QString firstLine = trimmed.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts).value(0).trimmed();
    return firstLine.left(96);
}

bool shouldRetry(const SpeedTestServiceInternal::UrlProbeResult& result)
{
    if (result.status == SpeedTestServiceInternal::UrlProbeStatus::Timeout) {
        return true;
    }

    return result.status == SpeedTestServiceInternal::UrlProbeStatus::Failed
        && result.latencyMs >= 0
        && result.latencyMs < kProbeRetryThresholdMs;
}

SpeedTestServiceInternal::UrlProbeResult probeReadyProxyWithRetry(
    const SpeedTestServiceInternal::ReadyProxy& proxy,
    const QString& url,
    int timeoutMs,
    const std::atomic_bool& cancelled)
{
    return probeWithRetry(
        proxy.type,
        kLoopbackAddress,
        proxy.port,
        QString{},
        QString{},
        url,
        timeoutMs,
        cancelled);
}

SpeedTestServiceInternal::UrlProbeResult probeSocksWithRetry(
    int socksPort,
    const QString& url,
    int timeoutMs,
    const std::atomic_bool& cancelled)
{
    return probeWithRetry(
        QNetworkProxy::Socks5Proxy,
        kLoopbackAddress,
        socksPort,
        QString{},
        QString{},
        url,
        timeoutMs,
        cancelled);
}

SpeedTestServiceInternal::UrlProbeResult probeUpstreamProxyWithRetry(
    const VmessItem& server,
    const QString& url,
    int timeoutMs,
    const std::atomic_bool& cancelled)
{
    QNetworkProxy::ProxyType proxyType = QNetworkProxy::DefaultProxy;
    if (server.configType == ConfigType::Socks) {
        proxyType = QNetworkProxy::Socks5Proxy;
    } else if (server.configType == ConfigType::HTTP) {
        proxyType = QNetworkProxy::HttpProxy;
    } else {
        return SpeedTestServiceInternal::UrlProbeResult{
            SpeedTestServiceInternal::UrlProbeStatus::Failed,
            -1,
            QStringLiteral("Unsupported")};
    }

    SpeedTestServiceInternal::UrlProbeResult result = probeWithRetry(
        proxyType,
        server.address,
        server.port,
        server.id,
        server.security,
        url,
        timeoutMs,
        cancelled);
    if (result.status == SpeedTestServiceInternal::UrlProbeStatus::Failed) {
        result.errorText = normalizeUpstreamProxyErrorText(server, result.errorText);
    }
    return result;
}

} // namespace SpeedTestUrlProbe
