#include "backends/singbox/SingBoxTransportConfigFragments.h"

#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>

#include "backends/singbox/SingBoxOutboundConfigSupport.h"

namespace OutboundSupport = SingBoxOutboundConfigSupport;

namespace SingBoxTransportConfigFragments {

QJsonObject buildTransport(const VmessItem& server)
{
    const QString network = server.network.trimmed().isEmpty() ? QStringLiteral("tcp") : server.network.trimmed();
    QJsonObject transport;

    if (network.compare(QStringLiteral("ws"), Qt::CaseInsensitive) == 0) {
        transport.insert(QStringLiteral("type"), QStringLiteral("ws"));
        QString wsPath = server.path.trimmed();
        if (!wsPath.isEmpty()) {
            const int querySeparator = wsPath.indexOf(QLatin1Char('?'));
            if (querySeparator >= 0) {
                const QString basePath = wsPath.left(querySeparator);
                QUrlQuery query(wsPath.mid(querySeparator + 1));

                bool hasEarlyData = false;
                const int maxEarlyData = query.queryItemValue(QStringLiteral("ed")).toInt(&hasEarlyData);
                if (hasEarlyData) {
                    transport.insert(QStringLiteral("max_early_data"), maxEarlyData);
                    transport.insert(QStringLiteral("early_data_header_name"), QStringLiteral("Sec-WebSocket-Protocol"));
                    query.removeAllQueryItems(QStringLiteral("ed"));
                }

                const QString earlyDataHeader = query.queryItemValue(QStringLiteral("eh"));
                if (!earlyDataHeader.isEmpty()) {
                    transport.insert(QStringLiteral("early_data_header_name"), earlyDataHeader);
                }

                wsPath = basePath;
                const QString encodedQuery = query.toString(QUrl::FullyEncoded);
                if (!encodedQuery.isEmpty()) {
                    wsPath += QStringLiteral("?") + encodedQuery;
                }
            }
        }

        if (!wsPath.isEmpty()) {
            transport.insert(QStringLiteral("path"), wsPath);
        }

        const QStringList hosts = OutboundSupport::splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonObject headers;
            headers.insert(QStringLiteral("Host"), hosts.constFirst());
            transport.insert(QStringLiteral("headers"), headers);
        }

        return transport;
    }

    if (network.compare(QStringLiteral("kcp"), Qt::CaseInsensitive) == 0) {
        return {};
    }

    if (network.compare(QStringLiteral("quic"), Qt::CaseInsensitive) == 0) {
        transport.insert(QStringLiteral("type"), QStringLiteral("quic"));
        return transport;
    }

    if (network.compare(QStringLiteral("grpc"), Qt::CaseInsensitive) == 0) {
        transport.insert(QStringLiteral("type"), QStringLiteral("grpc"));
        if (!server.path.trimmed().isEmpty()) {
            transport.insert(QStringLiteral("service_name"), server.path.trimmed());
        }
        return transport;
    }

    if (network.compare(QStringLiteral("httpupgrade"), Qt::CaseInsensitive) == 0) {
        transport.insert(QStringLiteral("type"), QStringLiteral("httpupgrade"));
        if (!server.path.trimmed().isEmpty()) {
            transport.insert(QStringLiteral("path"), server.path.trimmed());
        }
        if (!server.requestHost.trimmed().isEmpty()) {
            transport.insert(QStringLiteral("host"), server.requestHost.trimmed());
        }
        return transport;
    }

    if (network.compare(QStringLiteral("h2"), Qt::CaseInsensitive) == 0
        || (network.compare(QStringLiteral("tcp"), Qt::CaseInsensitive) == 0
            && server.headerType.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0)) {
        transport.insert(QStringLiteral("type"), QStringLiteral("http"));

        const QString path = OutboundSupport::resolvePrimaryPath(server.path);
        if (!path.isEmpty()) {
            transport.insert(QStringLiteral("path"), path);
        }

        const QStringList hosts = OutboundSupport::splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonArray hostArray;
            for (const QString& host : hosts) {
                hostArray.append(host);
            }
            transport.insert(QStringLiteral("host"), hostArray);
        }

        return transport;
    }

    return {};
}

} // namespace SingBoxTransportConfigFragments
