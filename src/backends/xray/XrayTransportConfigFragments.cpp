#include "backends/xray/XrayTransportConfigFragments.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include "common/UserAgent.h"

namespace {

QStringList splitCsv(const QString& value)
{
    QStringList result;
    const QStringList parts = value.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
        }
    }

    return result;
}

QString resolveLegacyUserAgent(const Config& config, const VmessItem& server)
{
    const QString serverUserAgent = server.userAgent.trimmed();
    if (!serverUserAgent.isEmpty()) {
        return serverUserAgent;
    }

    const QString defaultUserAgent = config.dns().defaultUserAgent.trimmed();
    return defaultUserAgent.isEmpty() ? fallbackUserAgent() : defaultUserAgent;
}

} // namespace

namespace XrayTransportConfigFragments {

QString resolveNetwork(const VmessItem& server)
{
    return server.network.trimmed().isEmpty() ? QStringLiteral("tcp") : server.network.trimmed();
}

QJsonObject buildHysteria2StreamSettings(const VmessItem& server)
{
    QJsonObject streamSettings;
    streamSettings.insert(QStringLiteral("network"), QStringLiteral("hysteria"));

    QJsonObject hysteriaSettings;
    hysteriaSettings.insert(QStringLiteral("version"), 2);
    hysteriaSettings.insert(QStringLiteral("auth"), server.id);
    streamSettings.insert(QStringLiteral("hysteriaSettings"), hysteriaSettings);
    return streamSettings;
}

void appendTransportSettings(
    QJsonObject& streamSettings,
    const Config& config,
    const VmessItem& server,
    const QString& network)
{
    const QString userAgent = resolveLegacyUserAgent(config, server);

    if (network.compare(QStringLiteral("kcp"), Qt::CaseInsensitive) == 0) {
        QJsonObject kcpSettings;
        QJsonObject header;
        header.insert(
            QStringLiteral("type"),
            server.headerType.trimmed().isEmpty() ? QStringLiteral("none") : server.headerType.trimmed());
        kcpSettings.insert(QStringLiteral("header"), header);
        if (!server.path.trimmed().isEmpty()) {
            kcpSettings.insert(QStringLiteral("seed"), server.path.trimmed());
        }
        streamSettings.insert(QStringLiteral("kcpSettings"), kcpSettings);
    } else if (network.compare(QStringLiteral("quic"), Qt::CaseInsensitive) == 0) {
        QJsonObject quicSettings;
        quicSettings.insert(
            QStringLiteral("security"),
            server.requestHost.trimmed().isEmpty() ? QStringLiteral("none") : server.requestHost.trimmed());
        quicSettings.insert(QStringLiteral("key"), server.path.trimmed());
        QJsonObject header;
        header.insert(
            QStringLiteral("type"),
            server.headerType.trimmed().isEmpty() ? QStringLiteral("none") : server.headerType.trimmed());
        quicSettings.insert(QStringLiteral("header"), header);
        streamSettings.insert(QStringLiteral("quicSettings"), quicSettings);
    } else if (network.compare(QStringLiteral("ws"), Qt::CaseInsensitive) == 0) {
        QJsonObject wsSettings;
        if (!server.path.trimmed().isEmpty()) {
            wsSettings.insert(QStringLiteral("path"), server.path);
        }
        QJsonObject headers;
        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            headers.insert(QStringLiteral("Host"), hosts.constFirst());
        }
        if (!userAgent.isEmpty()) {
            headers.insert(QStringLiteral("User-Agent"), userAgent);
        }
        if (!headers.isEmpty()) {
            wsSettings.insert(QStringLiteral("headers"), headers);
        }
        streamSettings.insert(QStringLiteral("wsSettings"), wsSettings);
    } else if (network.compare(QStringLiteral("grpc"), Qt::CaseInsensitive) == 0) {
        QJsonObject grpcSettings;
        grpcSettings.insert(QStringLiteral("serviceName"), server.path);
        grpcSettings.insert(
            QStringLiteral("multiMode"),
            server.headerType.compare(QStringLiteral("multi"), Qt::CaseInsensitive) == 0);
        streamSettings.insert(QStringLiteral("grpcSettings"), grpcSettings);
    } else if (network.compare(QStringLiteral("h2"), Qt::CaseInsensitive) == 0) {
        QJsonObject httpSettings;
        httpSettings.insert(QStringLiteral("path"), server.path);
        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonArray hostArray;
            for (const QString& host : hosts) {
                hostArray.append(host);
            }
            httpSettings.insert(QStringLiteral("host"), hostArray);
        }
        streamSettings.insert(QStringLiteral("httpSettings"), httpSettings);
    } else if (network.compare(QStringLiteral("httpupgrade"), Qt::CaseInsensitive) == 0) {
        QJsonObject httpupgradeSettings;
        if (!server.path.trimmed().isEmpty()) {
            httpupgradeSettings.insert(QStringLiteral("path"), server.path);
        }
        if (!server.requestHost.trimmed().isEmpty()) {
            httpupgradeSettings.insert(QStringLiteral("host"), server.requestHost);
        }
        if (!userAgent.isEmpty()) {
            QJsonObject headers;
            headers.insert(QStringLiteral("User-Agent"), userAgent);
            httpupgradeSettings.insert(QStringLiteral("headers"), headers);
        }
        streamSettings.insert(QStringLiteral("httpupgradeSettings"), httpupgradeSettings);
    } else if (network.compare(QStringLiteral("xhttp"), Qt::CaseInsensitive) == 0) {
        streamSettings.insert(QStringLiteral("network"), QStringLiteral("xhttp"));
        QJsonObject xhttpSettings;
        if (!server.path.trimmed().isEmpty()) {
            xhttpSettings.insert(QStringLiteral("path"), server.path);
        }
        if (!server.requestHost.trimmed().isEmpty()) {
            xhttpSettings.insert(QStringLiteral("host"), server.requestHost);
        }
        if (!server.headerType.trimmed().isEmpty()) {
            xhttpSettings.insert(QStringLiteral("mode"), server.headerType);
        }
        if (!server.extra.trimmed().isEmpty()) {
            QJsonParseError parseError;
            const QJsonDocument extraDoc = QJsonDocument::fromJson(server.extra.toUtf8(), &parseError);
            if (parseError.error == QJsonParseError::NoError && extraDoc.isObject()) {
                xhttpSettings.insert(QStringLiteral("extra"), extraDoc.object());
            }
        }
        streamSettings.insert(QStringLiteral("xhttpSettings"), xhttpSettings);
    } else if (network.compare(QStringLiteral("tcp"), Qt::CaseInsensitive) == 0
        && server.headerType.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0) {
        QJsonObject tcpSettings;
        QJsonObject header;
        header.insert(QStringLiteral("type"), QStringLiteral("http"));

        QJsonObject request;
        QJsonArray paths;
        const QStringList rawPaths = splitCsv(server.path);
        if (rawPaths.isEmpty()) {
            paths.append(QStringLiteral("/"));
        } else {
            for (const QString& path : rawPaths) {
                paths.append(path);
            }
        }
        request.insert(QStringLiteral("path"), paths);

        QJsonObject headers;
        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonArray hostArray;
            for (const QString& host : hosts) {
                hostArray.append(host);
            }
            headers.insert(QStringLiteral("Host"), hostArray);
        }
        if (!userAgent.isEmpty()) {
            headers.insert(QStringLiteral("User-Agent"), QJsonArray{userAgent});
        }
        if (!headers.isEmpty()) {
            request.insert(QStringLiteral("headers"), headers);
        }

        header.insert(QStringLiteral("request"), request);
        tcpSettings.insert(QStringLiteral("header"), header);
        streamSettings.insert(QStringLiteral("tcpSettings"), tcpSettings);
    }
}

} // namespace XrayTransportConfigFragments
