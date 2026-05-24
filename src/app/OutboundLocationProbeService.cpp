#include "app/OutboundLocationProbeService.h"

#include <QByteArray>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QThread>

#include "common/PortValidator.h"
#include "common/UserAgent.h"

namespace {

QString countryCodeToFlag(const QString& countryCode)
{
    const QString code = countryCode.trimmed().toUpper();
    if (code.size() != 2) {
        return {};
    }

    QVector<uint> flagCodePoints;
    flagCodePoints.reserve(2);
    for (const QChar ch : code) {
        if (ch < QLatin1Char('A') || ch > QLatin1Char('Z')) {
            return {};
        }
        flagCodePoints.append(0x1F1E6u + static_cast<uint>(ch.unicode() - QLatin1Char('A').unicode()));
    }
    return QString::fromUcs4(flagCodePoints.constData(), flagCodePoints.size());
}

QString buildLocationSummaryFromPayload(const QByteArray& payload)
{
    const QJsonDocument json = QJsonDocument::fromJson(payload);
    if (!json.isObject()) {
        return {};
    }

    const QJsonObject object = json.object();
    const QString status = object.value(QStringLiteral("status")).toString().trimmed();
    if (status.compare(QStringLiteral("fail"), Qt::CaseInsensitive) == 0) {
        return {};
    }
    if (object.contains(QStringLiteral("success")) && !object.value(QStringLiteral("success")).toBool(true)) {
        return {};
    }

    QString city = object.value(QStringLiteral("city")).toString().trimmed();
    if (city.isEmpty()) {
        city = object.value(QStringLiteral("regionName")).toString().trimmed();
    }
    if (city.isEmpty()) {
        city = object.value(QStringLiteral("region_name")).toString().trimmed();
    }
    if (city.isEmpty()) {
        city = object.value(QStringLiteral("region")).toString().trimmed();
    }

    QString country = object.value(QStringLiteral("country")).toString().trimmed();
    if (country.isEmpty()) {
        country = object.value(QStringLiteral("country_name")).toString().trimmed();
    }
    QString countryCode = object.value(QStringLiteral("countryCode")).toString().trimmed();
    if (countryCode.isEmpty()) {
        countryCode = object.value(QStringLiteral("country_code")).toString().trimmed();
    }
    if (countryCode.isEmpty()) {
        countryCode = object.value(QStringLiteral("country_iso")).toString().trimmed();
    }
    if (countryCode.isEmpty() && country.size() == 2) {
        countryCode = country;
    }

    QStringList parts;
    if (!country.isEmpty()) {
        const QString flag = countryCodeToFlag(countryCode);
        parts.append(flag.isEmpty() ? country : QStringLiteral("%1 %2").arg(flag, country));
    }
    if (!city.isEmpty()) {
        parts.append(city);
    }

    const QString summary = parts.join(QStringLiteral(", "));
    if (!summary.isEmpty()) {
        return summary;
    }

    QString ip = object.value(QStringLiteral("query")).toString().trimmed();
    if (ip.isEmpty()) {
        ip = object.value(QStringLiteral("ip")).toString().trimmed();
    }
    return ip;
}

QString locationProbeErrorMessage(const QUrl& url, const QString& error)
{
    const QString host = url.host().trimmed();
    if (host.isEmpty()) {
        return error;
    }
    if (error.trimmed().isEmpty()) {
        return QStringLiteral("Outbound location request failed: %1").arg(host);
    }
    return QStringLiteral("%1: %2").arg(host, error.trimmed());
}

} // namespace

int OutboundLocationProbeService::resolveHttpPort(const Config& config, bool usesDedicatedProbe)
{
    if (usesDedicatedProbe) {
        return isValidTcpPort(config.localLocationProbePort)
            ? config.localLocationProbePort
            : config.localPort + LocationProbePortOffset;
    }

    return isValidTcpPort(config.localHttpPort)
        ? config.localHttpPort
        : config.localPort + 1;
}

OutboundLocationProbeResult OutboundLocationProbeService::probe(int httpPort) const
{
    QString location;
    QString lastError;
    QElapsedTimer probeTimer;
    probeTimer.start();
    const QStringList urls = probeUrls();
    int attempt = 0;

    while (location.isEmpty()
        && probeTimer.elapsed() < LocationProbeTotalTimeoutMs
        && attempt < LocationProbeMaxRounds) {
        ++attempt;
        const OutboundLocationProbeResult probeResult = probeOnce(
            urls,
            httpPort,
            qMin(LocationProbeTimeoutMs, static_cast<int>(LocationProbeTotalTimeoutMs - probeTimer.elapsed())));
        location = probeResult.location;
        if (location.isEmpty() && !probeResult.error.trimmed().isEmpty()) {
            lastError = probeResult.error;
        }

        if (location.isEmpty()
            && probeTimer.elapsed() < LocationProbeTotalTimeoutMs
            && attempt < LocationProbeMaxRounds) {
            QThread::msleep(LocationProbeRetryDelayMs);
        }
    }

    OutboundLocationProbeResult result;
    result.location = location;
    result.error = lastError;
    return result;
}

QStringList OutboundLocationProbeService::probeUrls()
{
    return {
        QStringLiteral("http://ip-api.com/json/?fields=status,message,country,countryCode,regionName,city,query"),
        QStringLiteral("http://ipwho.is/"),
        QStringLiteral("https://ipinfo.io/json"),
        QStringLiteral("https://ifconfig.co/json"),
        QStringLiteral("https://ipapi.co/json/")
    };
}

OutboundLocationProbeResult OutboundLocationProbeService::probeOnce(
    const QStringList& probeUrls,
    int httpPort,
    int timeoutMs)
{
    OutboundLocationProbeResult result;
    if (probeUrls.isEmpty() || !isValidTcpPort(httpPort) || timeoutMs <= 0) {
        result.error = QStringLiteral("Outbound location probe is unavailable.");
        return result;
    }

    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"), httpPort));

    QEventLoop loop;
    QList<QNetworkReply*> replies;
    replies.reserve(probeUrls.size());
    bool completed = false;

    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        if (!completed) {
            result.error = QStringLiteral("Outbound location request timed out.");
        }
        loop.quit();
    });

    for (const QString& rawUrl : probeUrls) {
        const QUrl probeUrl(rawUrl);
        QNetworkRequest request(probeUrl);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, fallbackUserAgent());

        QNetworkReply* reply = manager.get(request);
        replies.append(reply);
        QObject::connect(reply, &QNetworkReply::finished, &loop, [&result, &loop, &completed, reply, probeUrl]() {
            if (completed) {
                return;
            }

            if (reply->error() == QNetworkReply::NoError) {
                const QString location = buildLocationSummaryFromPayload(reply->readAll());
                if (!location.isEmpty()) {
                    result.location = location;
                    completed = true;
                    loop.quit();
                    return;
                }
                result.error = locationProbeErrorMessage(
                    probeUrl,
                    QStringLiteral("Outbound location response was empty."));
                return;
            }

            result.error = locationProbeErrorMessage(probeUrl, reply->errorString());
        });
    }

    timeoutTimer.start(timeoutMs);
    loop.exec();
    timeoutTimer.stop();

    for (QNetworkReply* reply : replies) {
        if (reply == nullptr) {
            continue;
        }
        QObject::disconnect(reply, nullptr, &loop, nullptr);
        if (!reply->isFinished()) {
            reply->abort();
        }
        delete reply;
    }

    return result;
}
