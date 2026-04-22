#include "runtime/ServerConfigWriter.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace {

QString protocolName(ConfigType type)
{
    switch (type) {
    case ConfigType::VMess:
        return QStringLiteral("vmess");
    case ConfigType::VLESS:
        return QStringLiteral("vless");
    default:
        return {};
    }
}

} // namespace

OperationResult ServerConfigWriter::writeServerConfig(const VmessItem& server, const QString& filePath) const
{
    if (filePath.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Server config export path is empty."));
    }

    const OperationResult validationResult = validateServer(server);
    if (!validationResult.success) {
        return validationResult;
    }

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return OperationResult::fail(QStringLiteral("Failed to create the server config export directory."));
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return OperationResult::fail(QStringLiteral("Failed to open the server config export file for writing."));
    }

    const QJsonDocument document(buildRoot(server));
    if (file.write(document.toJson(QJsonDocument::Indented)) < 0) {
        return OperationResult::fail(QStringLiteral("Failed to write the server config export file."));
    }

    if (!file.commit()) {
        return OperationResult::fail(QStringLiteral("Failed to commit the server config export file."));
    }

    return OperationResult::ok(QStringLiteral("Server config template generated: %1").arg(QDir::toNativeSeparators(filePath)));
}

OperationResult ServerConfigWriter::validateServer(const VmessItem& server)
{
    if (protocolName(server.configType).isEmpty()) {
        return OperationResult::fail(
            QStringLiteral("Server config export currently supports VMess and VLESS only."));
    }

    if (server.port <= 0) {
        return OperationResult::fail(QStringLiteral("Server config export requires a valid port."));
    }

    if (server.id.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Server config export requires a valid UUID."));
    }

    if (isRealityTransport(server.streamSecurity)) {
        if (server.flow.trimmed().isEmpty()) {
            return OperationResult::fail(
                QStringLiteral("Reality server config export requires a flow (e.g. xtls-rprx-vision)."));
        }
    }

    return OperationResult::ok();
}

QJsonObject ServerConfigWriter::buildRoot(const VmessItem& server)
{
    QJsonObject root;
    root.insert(QStringLiteral("log"), buildLog());

    QJsonArray inbounds;
    inbounds.append(buildInbound(server));
    root.insert(QStringLiteral("inbounds"), inbounds);
    root.insert(QStringLiteral("outbounds"), buildOutbounds());
    return root;
}

QJsonObject ServerConfigWriter::buildLog()
{
    QJsonObject log;
    log.insert(QStringLiteral("loglevel"), QStringLiteral("warning"));
    return log;
}

QJsonObject ServerConfigWriter::buildInbound(const VmessItem& server)
{
    QJsonObject inbound;
    inbound.insert(QStringLiteral("listen"), QStringLiteral("0.0.0.0"));
    inbound.insert(QStringLiteral("port"), server.port);
    inbound.insert(QStringLiteral("protocol"), protocolName(server.configType));
    inbound.insert(QStringLiteral("settings"), buildInboundSettings(server));

    const QJsonObject streamSettings = buildStreamSettings(server);
    if (!streamSettings.isEmpty()) {
        inbound.insert(QStringLiteral("streamSettings"), streamSettings);
    }

    return inbound;
}

QJsonObject ServerConfigWriter::buildInboundSettings(const VmessItem& server)
{
    QJsonObject settings;
    QJsonArray clients;
    QJsonObject client;
    client.insert(QStringLiteral("id"), server.id.trimmed());
    if (server.configType == ConfigType::VMess) {
        client.insert(QStringLiteral("alterId"), server.alterId);
    } else if (!server.flow.trimmed().isEmpty()) {
        client.insert(QStringLiteral("flow"), server.flow.trimmed());
    }
    clients.append(client);
    settings.insert(QStringLiteral("clients"), clients);

    if (server.configType == ConfigType::VLESS) {
        settings.insert(
            QStringLiteral("decryption"),
            server.security.trimmed().isEmpty() ? QStringLiteral("none") : server.security.trimmed());
    }

    return settings;
}

QJsonObject ServerConfigWriter::buildStreamSettings(const VmessItem& server)
{
    QJsonObject streamSettings;
    const QString network = server.network.trimmed().isEmpty() ? QStringLiteral("tcp") : server.network.trimmed();
    const QString transportSecurity = server.streamSecurity.trimmed();
    streamSettings.insert(QStringLiteral("network"), network);

    if (!transportSecurity.isEmpty()) {
        streamSettings.insert(QStringLiteral("security"), transportSecurity);
    }

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
            wsSettings.insert(QStringLiteral("path"), server.path.trimmed());
        }
        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonObject headers;
            headers.insert(QStringLiteral("Host"), hosts.constFirst());
            wsSettings.insert(QStringLiteral("headers"), headers);
        }
        streamSettings.insert(QStringLiteral("wsSettings"), wsSettings);
    } else if (network.compare(QStringLiteral("grpc"), Qt::CaseInsensitive) == 0) {
        QJsonObject grpcSettings;
        if (!server.path.trimmed().isEmpty()) {
            grpcSettings.insert(QStringLiteral("serviceName"), server.path.trimmed());
        }
        grpcSettings.insert(
            QStringLiteral("mode"),
            server.headerType.trimmed().isEmpty() ? QStringLiteral("gun") : server.headerType.trimmed());
        streamSettings.insert(QStringLiteral("grpcSettings"), grpcSettings);
    } else if (network.compare(QStringLiteral("h2"), Qt::CaseInsensitive) == 0) {
        QJsonObject httpSettings;
        if (!server.path.trimmed().isEmpty()) {
            httpSettings.insert(QStringLiteral("path"), server.path.trimmed());
        }
        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonArray hostArray;
            for (const QString& host : hosts) {
                hostArray.append(host);
            }
            httpSettings.insert(QStringLiteral("host"), hostArray);
        }
        streamSettings.insert(QStringLiteral("httpSettings"), httpSettings);
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

        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonObject headers;
            QJsonArray hostArray;
            for (const QString& host : hosts) {
                hostArray.append(host);
            }
            headers.insert(QStringLiteral("Host"), hostArray);
            request.insert(QStringLiteral("headers"), headers);
        }

        header.insert(QStringLiteral("request"), request);
        tcpSettings.insert(QStringLiteral("header"), header);
        streamSettings.insert(QStringLiteral("tcpSettings"), tcpSettings);
    }

    if (transportSecurity.compare(QStringLiteral("tls"), Qt::CaseInsensitive) == 0
        || transportSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) == 0) {
        QJsonObject tlsSettings;
        const QString serverName = resolveServerName(server).trimmed();
        if (!serverName.isEmpty()) {
            tlsSettings.insert(QStringLiteral("serverName"), serverName);
        }
        tlsSettings.insert(QStringLiteral("allowInsecure"), resolveAllowInsecure(server.allowInsecure));
        if (!server.fingerprint.trimmed().isEmpty()) {
            tlsSettings.insert(QStringLiteral("fingerprint"), server.fingerprint.trimmed());
        }

        if (!server.alpn.isEmpty()) {
            QJsonArray alpnArray;
            for (const QString& item : server.alpn) {
                if (!item.trimmed().isEmpty()) {
                    alpnArray.append(item.trimmed());
                }
            }
            if (!alpnArray.isEmpty()) {
                tlsSettings.insert(QStringLiteral("alpn"), alpnArray);
            }
        }

        if (transportSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) == 0) {
            streamSettings.insert(QStringLiteral("xtlsSettings"), tlsSettings);
        } else {
            streamSettings.insert(QStringLiteral("tlsSettings"), tlsSettings);
        }
    } else if (isRealityTransport(transportSecurity)) {
        QJsonObject realitySettings;
        const QString serverName = resolveServerName(server).trimmed();
        if (!serverName.isEmpty()) {
            QJsonArray serverNames;
            serverNames.append(serverName);
            realitySettings.insert(QStringLiteral("serverNames"), serverNames);
            realitySettings.insert(
                QStringLiteral("dest"),
                QStringLiteral("%1:%2").arg(serverName).arg(server.port));
        }
        realitySettings.insert(
            QStringLiteral("privateKey"),
            QStringLiteral("YOUR_SERVER_PRIVATE_KEY"));
        if (!server.shortId.trimmed().isEmpty()) {
            QJsonArray shortIds;
            shortIds.append(server.shortId.trimmed());
            realitySettings.insert(QStringLiteral("shortIds"), shortIds);
        }
        streamSettings.insert(QStringLiteral("realitySettings"), realitySettings);
    }

    return streamSettings;
}

QJsonArray ServerConfigWriter::buildOutbounds()
{
    QJsonArray outbounds;

    QJsonObject freedom;
    freedom.insert(QStringLiteral("protocol"), QStringLiteral("freedom"));
    freedom.insert(QStringLiteral("settings"), QJsonObject());
    outbounds.append(freedom);

    QJsonObject blackhole;
    blackhole.insert(QStringLiteral("protocol"), QStringLiteral("blackhole"));
    blackhole.insert(QStringLiteral("settings"), QJsonObject());
    outbounds.append(blackhole);

    return outbounds;
}

QStringList ServerConfigWriter::splitCsv(const QString& value)
{
    QStringList items;
    for (const QString& rawItem : value.split(QChar(','), Qt::SkipEmptyParts)) {
        const QString item = rawItem.trimmed();
        if (!item.isEmpty()) {
            items.append(item);
        }
    }
    return items;
}

QString ServerConfigWriter::resolveServerName(const VmessItem& server)
{
    if (!server.sni.trimmed().isEmpty()) {
        return server.sni.trimmed();
    }

    return server.address.trimmed();
}

bool ServerConfigWriter::resolveAllowInsecure(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("true")
        || normalized == QStringLiteral("1")
        || normalized == QStringLiteral("yes");
}

bool ServerConfigWriter::isRealityTransport(const QString& value)
{
    return value.trimmed().compare(QStringLiteral("reality"), Qt::CaseInsensitive) == 0;
}
