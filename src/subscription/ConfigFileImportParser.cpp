#include "subscription/ConfigFileImportParser.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace {

QString importRemark(const QString& type)
{
    return QStringLiteral("import %1@%2")
        .arg(type)
        .arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")));
}

bool isLoopbackOrAnyAddress(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized.isEmpty()
        || normalized == QStringLiteral("127.0.0.1")
        || normalized == QStringLiteral("0.0.0.0")
        || normalized == QStringLiteral("::")
        || normalized == QStringLiteral("::1")
        || normalized == QStringLiteral("[::]")
        || normalized == QStringLiteral("[::1]");
}

QString readHeaderValue(const QJsonObject& headers, const QString& name)
{
    const QJsonValue value = headers.value(name);
    if (value.isString()) {
        return value.toString().trimmed();
    }

    QStringList values;
    const QJsonArray array = value.toArray();
    values.reserve(array.size());
    for (const QJsonValue& item : array) {
        const QString text = item.toString().trimmed();
        if (!text.isEmpty()) {
            values.append(text);
        }
    }
    return values.join(QStringLiteral(","));
}

QString compactJsonText(const QJsonValue& value)
{
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    return QString();
}

QString joinCertificateLines(const QJsonValue& value)
{
    const QJsonArray certificates = value.toArray();
    QStringList pemBlocks;

    for (const QJsonValue& certificateValue : certificates) {
        if (!certificateValue.isObject()) {
            continue;
        }

        const QJsonArray lines = certificateValue.toObject().value(QStringLiteral("certificate")).toArray();
        if (lines.isEmpty()) {
            continue;
        }

        QStringList pemLines;
        pemLines.reserve(lines.size());
        for (const QJsonValue& lineValue : lines) {
            const QString line = lineValue.toString().trimmed();
            if (!line.isEmpty()) {
                pemLines.append(line);
            }
        }

        if (!pemLines.isEmpty()) {
            pemBlocks.append(pemLines.join(QChar('\n')));
        }
    }

    return pemBlocks.join(QChar('\n'));
}

} // namespace

OperationResult ConfigFileImportParser::parseClientConfig(const QString& text, VmessItem* item)
{
    if (item == nullptr) {
        return OperationResult::fail(QStringLiteral("Client config import target is unavailable."));
    }

    QJsonObject root;
    const OperationResult parseResult = parseDocument(text, &root, QStringLiteral("client"));
    if (!parseResult.success) {
        return parseResult;
    }

    const QJsonArray outbounds = root.value(QStringLiteral("outbounds")).toArray();
    QJsonObject outbound;
    QString protocol;
    for (const QJsonValue& value : outbounds) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject candidate = value.toObject();
        const QString proto = candidate.value(QStringLiteral("protocol")).toString().trimmed().toLower();
        if (proto == QStringLiteral("vmess")
            || proto == QStringLiteral("vless")
            || proto == QStringLiteral("trojan")
            || proto == QStringLiteral("shadowsocks")
            || proto == QStringLiteral("socks")
            || proto == QStringLiteral("http")) {
            outbound = candidate;
            protocol = proto;
            break;
        }
    }

    if (outbound.isEmpty()) {
        return OperationResult::fail(QStringLiteral("Client config does not contain a recognized outbound entry."));
    }

    const QJsonObject settings = outbound.value(QStringLiteral("settings")).toObject();

    VmessItem parsed;
    parsed.coreType = CoreType::Auto;
    parsed.network = QStringLiteral("tcp");
    parsed.headerType = QStringLiteral("none");
    parsed.remarks = importRemark(QStringLiteral("client"));

    if (protocol == QStringLiteral("vmess") || protocol == QStringLiteral("vless")) {
        if (!parseVnextUser(settings, protocol, parsed)) {
            return OperationResult::fail(
                QStringLiteral("Client config does not contain a valid %1 remote endpoint.").arg(protocol));
        }
    } else if (protocol == QStringLiteral("trojan")) {
        parsed.configType = ConfigType::Trojan;
        if (!parseServerEndpoint(settings, parsed)) {
            return OperationResult::fail(QStringLiteral("Client config does not contain a valid Trojan remote endpoint."));
        }
        parsed.id = parsed.id.isEmpty()
            ? settings.value(QStringLiteral("password")).toString().trimmed()
            : parsed.id;
    } else if (protocol == QStringLiteral("shadowsocks")) {
        parsed.configType = ConfigType::Shadowsocks;
        if (!parseServerEndpoint(settings, parsed)) {
            return OperationResult::fail(QStringLiteral("Client config does not contain a valid Shadowsocks remote endpoint."));
        }
        if (parsed.security.isEmpty()) {
            parsed.security = settings.value(QStringLiteral("method")).toString().trimmed();
        }
        if (parsed.id.isEmpty()) {
            parsed.id = settings.value(QStringLiteral("password")).toString().trimmed();
        }
    } else if (protocol == QStringLiteral("socks")) {
        parsed.configType = ConfigType::Socks;
        if (!parseServerEndpoint(settings, parsed)) {
            return OperationResult::fail(QStringLiteral("Client config does not contain a valid SOCKS remote endpoint."));
        }
    } else if (protocol == QStringLiteral("http")) {
        parsed.configType = ConfigType::HTTP;
        if (!parseServerEndpoint(settings, parsed)) {
            return OperationResult::fail(QStringLiteral("Client config does not contain a valid HTTP remote endpoint."));
        }
        if (parsed.id.isEmpty()) {
            parsed.id = settings.value(QStringLiteral("password")).toString().trimmed();
        }
    }

    populateCommonStreamSettings(outbound.value(QStringLiteral("streamSettings")).toObject(), parsed);

    if (parsed.address.isEmpty() || parsed.port <= 0) {
        return OperationResult::fail(
            QStringLiteral("Client config is missing the required %1 address or port.").arg(protocol));
    }

    *item = parsed;
    return OperationResult::ok();
}

bool ConfigFileImportParser::parseVnextUser(const QJsonObject& settings, const QString& protocol, VmessItem& item)
{
    const QJsonArray vnextArray = settings.value(QStringLiteral("vnext")).toArray();
    if (vnextArray.isEmpty() || !vnextArray.at(0).isObject()) {
        return false;
    }

    const QJsonObject remote = vnextArray.at(0).toObject();
    const QJsonArray users = remote.value(QStringLiteral("users")).toArray();
    if (users.isEmpty() || !users.at(0).isObject()) {
        return false;
    }

    const QJsonObject user = users.at(0).toObject();

    item.configType = protocol == QStringLiteral("vless") ? ConfigType::VLESS : ConfigType::VMess;
    item.address = remote.value(QStringLiteral("address")).toString().trimmed();
    item.port = remote.value(QStringLiteral("port")).toInt(0);
    item.id = user.value(QStringLiteral("id")).toString().trimmed();

    if (item.configType == ConfigType::VMess) {
        item.alterId = user.value(QStringLiteral("alterId")).toInt(0);
        item.security = user.value(QStringLiteral("security")).toString().trimmed();
        if (item.security.isEmpty()) {
            item.security = QStringLiteral("auto");
        }
    } else {
        item.flow = user.value(QStringLiteral("flow")).toString().trimmed();
    }

    return true;
}

bool ConfigFileImportParser::parseServerEndpoint(const QJsonObject& settings, VmessItem& item)
{
    const QJsonArray servers = settings.value(QStringLiteral("servers")).toArray();
    if (servers.isEmpty() || !servers.at(0).isObject()) {
        return false;
    }

    const QJsonObject server = servers.at(0).toObject();
    item.address = server.value(QStringLiteral("address")).toString().trimmed();
    item.port = server.value(QStringLiteral("port")).toInt(0);
    if (item.id.isEmpty()) {
        item.id = server.value(QStringLiteral("password")).toString().trimmed();
    }
    if (item.configType == ConfigType::Shadowsocks) {
        item.security = server.value(QStringLiteral("method")).toString().trimmed();
    }

    if (item.configType == ConfigType::Socks) {
        const QJsonArray users = server.value(QStringLiteral("users")).toArray();
        if (!users.isEmpty() && users.at(0).isObject()) {
            const QJsonObject user = users.at(0).toObject();
            const QString socksUser = user.value(QStringLiteral("user")).toString().trimmed();
            const QString socksPass = user.value(QStringLiteral("pass")).toString().trimmed();
            if (item.id.isEmpty() && !socksPass.isEmpty()) {
                item.id = socksPass;
            }
            if (item.security.isEmpty() && !socksUser.isEmpty()) {
                item.security = socksUser;
            }
        }
    }

    return true;
}

OperationResult ConfigFileImportParser::parseServerConfig(const QString& text, VmessItem* item)
{
    if (item == nullptr) {
        return OperationResult::fail(QStringLiteral("Server config import target is unavailable."));
    }

    QJsonObject root;
    const OperationResult parseResult = parseDocument(text, &root, QStringLiteral("server"));
    if (!parseResult.success) {
        return parseResult;
    }

    const QJsonArray inbounds = root.value(QStringLiteral("inbounds")).toArray();
    QJsonObject inbound;
    QString protocol;
    for (const QJsonValue& value : inbounds) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject candidate = value.toObject();
        const QString proto = candidate.value(QStringLiteral("protocol")).toString().trimmed().toLower();
        if (proto == QStringLiteral("vmess")
            || proto == QStringLiteral("vless")
            || proto == QStringLiteral("trojan")
            || proto == QStringLiteral("shadowsocks")
            || proto == QStringLiteral("socks")
            || proto == QStringLiteral("http")) {
            inbound = candidate;
            protocol = proto;
            break;
        }
    }

    if (inbound.isEmpty()) {
        return OperationResult::fail(QStringLiteral("Server config does not contain a recognized inbound entry."));
    }

    const QJsonObject settings = inbound.value(QStringLiteral("settings")).toObject();

    VmessItem parsed;
    parsed.coreType = CoreType::Auto;
    const QString listenAddress = inbound.value(QStringLiteral("listen")).toString().trimmed();
    parsed.address = isLoopbackOrAnyAddress(listenAddress) ? QString() : listenAddress;
    parsed.port = inbound.value(QStringLiteral("port")).toInt(0);
    parsed.network = QStringLiteral("tcp");
    parsed.headerType = QStringLiteral("none");
    parsed.remarks = importRemark(QStringLiteral("server"));

    const QJsonArray clients = settings.value(QStringLiteral("clients")).toArray();
    if (protocol == QStringLiteral("vmess") || protocol == QStringLiteral("vless")) {
        parsed.configType = protocol == QStringLiteral("vless") ? ConfigType::VLESS : ConfigType::VMess;
        if (clients.isEmpty() || !clients.at(0).isObject()) {
            return OperationResult::fail(
                QStringLiteral("Server config does not contain a valid %1 client entry.").arg(protocol));
        }
        const QJsonObject client = clients.at(0).toObject();
        parsed.id = client.value(QStringLiteral("id")).toString().trimmed();
        if (parsed.configType == ConfigType::VMess) {
            parsed.alterId = client.value(QStringLiteral("alterId")).toInt(0);
            parsed.security = QStringLiteral("auto");
        } else {
            parsed.flow = client.value(QStringLiteral("flow")).toString().trimmed();
        }
    } else if (protocol == QStringLiteral("trojan")) {
        parsed.configType = ConfigType::Trojan;
        if (clients.isEmpty() || !clients.at(0).isObject()) {
            return OperationResult::fail(QStringLiteral("Server config does not contain a valid Trojan client entry."));
        }
        parsed.id = clients.at(0).toObject().value(QStringLiteral("password")).toString().trimmed();
    } else if (protocol == QStringLiteral("shadowsocks")) {
        parsed.configType = ConfigType::Shadowsocks;
        if (clients.isEmpty() || !clients.at(0).isObject()) {
            return OperationResult::fail(QStringLiteral("Server config does not contain a valid Shadowsocks client entry."));
        }
        const QJsonObject client = clients.at(0).toObject();
        parsed.id = client.value(QStringLiteral("password")).toString().trimmed();
        parsed.security = client.value(QStringLiteral("method")).toString().trimmed();
    } else if (protocol == QStringLiteral("socks")) {
        parsed.configType = ConfigType::Socks;
        if (!clients.isEmpty() && clients.at(0).isObject()) {
            parsed.id = clients.at(0).toObject().value(QStringLiteral("pass")).toString().trimmed();
            parsed.security = clients.at(0).toObject().value(QStringLiteral("user")).toString().trimmed();
        }
    } else if (protocol == QStringLiteral("http")) {
        parsed.configType = ConfigType::HTTP;
        if (!clients.isEmpty() && clients.at(0).isObject()) {
            parsed.id = clients.at(0).toObject().value(QStringLiteral("password")).toString().trimmed();
        }
    }

    populateCommonStreamSettings(inbound.value(QStringLiteral("streamSettings")).toObject(), parsed);

    if (parsed.port <= 0) {
        return OperationResult::fail(
            QStringLiteral("Server config is missing the required %1 port.").arg(protocol));
    }

    *item = parsed;
    return OperationResult::ok();
}

OperationResult ConfigFileImportParser::parseDocument(const QString& text, QJsonObject* root, const QString& configKind)
{
    if (root == nullptr) {
        return OperationResult::fail(QStringLiteral("Config import target is unavailable."));
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(text.trimmed().toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return OperationResult::fail(QStringLiteral("Failed to parse %1 config JSON.").arg(configKind));
    }

    *root = document.object();
    return OperationResult::ok();
}

void ConfigFileImportParser::populateCommonStreamSettings(const QJsonObject& streamSettings, VmessItem& item)
{
    if (streamSettings.isEmpty()) {
        return;
    }

    const QString network = streamSettings.value(QStringLiteral("network")).toString().trimmed().toLower();
    if (!network.isEmpty()) {
        item.network = network == QStringLiteral("http") ? QStringLiteral("h2") : network;
    }

    item.streamSecurity = streamSettings.value(QStringLiteral("security")).toString().trimmed();
    item.finalmask = compactJsonText(streamSettings.value(QStringLiteral("finalmask")));

    const QJsonObject tlsSettings = streamSettings.value(QStringLiteral("tlsSettings")).toObject();
    const QJsonObject xtlsSettings = streamSettings.value(QStringLiteral("xtlsSettings")).toObject();
    const QJsonObject secureSettings = !xtlsSettings.isEmpty() ? xtlsSettings : tlsSettings;
    if (!secureSettings.isEmpty()) {
        item.sni = secureSettings.value(QStringLiteral("serverName")).toString().trimmed();
        item.alpn = toStringList(secureSettings.value(QStringLiteral("alpn")));
        item.echConfigList = secureSettings.value(QStringLiteral("echConfigList")).toString().trimmed();
        item.echForceQuery = secureSettings.value(QStringLiteral("echForceQuery")).toString().trimmed();
        item.fingerprint = secureSettings.value(QStringLiteral("fingerprint")).toString().trimmed();
        item.cert = joinCertificateLines(secureSettings.value(QStringLiteral("certificates")));
        item.certSha = secureSettings.value(QStringLiteral("pinnedPeerCertSha256")).toString().trimmed();
        if (secureSettings.contains(QStringLiteral("allowInsecure"))) {
            item.allowInsecure = secureSettings.value(QStringLiteral("allowInsecure")).toBool(false)
                ? QStringLiteral("true")
                : QStringLiteral("false");
        }
    }

    const QJsonObject realitySettings = streamSettings.value(QStringLiteral("realitySettings")).toObject();
    if (!realitySettings.isEmpty()) {
        if (item.streamSecurity.isEmpty()) {
            item.streamSecurity = QStringLiteral("reality");
        }
        if (item.sni.isEmpty()) {
            item.sni = realitySettings.value(QStringLiteral("serverName")).toString().trimmed();
        }
        item.publicKey = realitySettings.value(QStringLiteral("publicKey")).toString().trimmed();
        item.shortId = realitySettings.value(QStringLiteral("shortId")).toString().trimmed();
        item.spiderX = realitySettings.value(QStringLiteral("spiderX")).toString().trimmed();
        item.mldsa65Verify = realitySettings.value(QStringLiteral("mldsa65Verify")).toString().trimmed();
        item.fingerprint = realitySettings.value(QStringLiteral("fingerprint")).toString().trimmed();
    }

    const QJsonObject tcpHeader = streamSettings.value(QStringLiteral("tcpSettings")).toObject().value(QStringLiteral("header")).toObject();
    if (!tcpHeader.isEmpty()) {
        item.headerType = tcpHeader.value(QStringLiteral("type")).toString().trimmed();
        if (item.headerType.isEmpty()) {
            item.headerType = QStringLiteral("none");
        }

        if (item.headerType.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0) {
            const QJsonObject request = tcpHeader.value(QStringLiteral("request")).toObject();
            item.path = joinStringValues(request.value(QStringLiteral("path")));
            item.requestHost = joinStringValues(request.value(QStringLiteral("headers")).toObject().value(QStringLiteral("Host")));
        }
    }

    const QJsonObject kcpHeader = streamSettings.value(QStringLiteral("kcpSettings")).toObject().value(QStringLiteral("header")).toObject();
    if (!kcpHeader.isEmpty()) {
        item.headerType = kcpHeader.value(QStringLiteral("type")).toString().trimmed();
        if (item.headerType.isEmpty()) {
            item.headerType = QStringLiteral("none");
        }
    }

    const QJsonObject wsSettings = streamSettings.value(QStringLiteral("wsSettings")).toObject();
    if (!wsSettings.isEmpty()) {
        item.path = wsSettings.value(QStringLiteral("path")).toString().trimmed();
        item.requestHost = joinStringValues(wsSettings.value(QStringLiteral("host")));
        const QJsonObject headers = wsSettings.value(QStringLiteral("headers")).toObject();
        if (item.requestHost.isEmpty()) {
            item.requestHost = readHeaderValue(headers, QStringLiteral("Host"));
        }
        item.userAgent = readHeaderValue(headers, QStringLiteral("User-Agent"));
    }

    const QJsonObject httpSettings = streamSettings.value(QStringLiteral("httpSettings")).toObject();
    if (!httpSettings.isEmpty()) {
        item.path = httpSettings.value(QStringLiteral("path")).toString().trimmed();
        item.requestHost = joinStringValues(httpSettings.value(QStringLiteral("host")));
    }

    const QJsonObject quicSettings = streamSettings.value(QStringLiteral("quicSettings")).toObject();
    if (!quicSettings.isEmpty()) {
        item.requestHost = quicSettings.value(QStringLiteral("security")).toString().trimmed();
        item.path = quicSettings.value(QStringLiteral("key")).toString().trimmed();
    }

    const QJsonObject grpcSettings = streamSettings.value(QStringLiteral("grpcSettings")).toObject();
    if (!grpcSettings.isEmpty()) {
        item.path = grpcSettings.value(QStringLiteral("serviceName")).toString().trimmed();
        item.headerType = grpcSettings.value(QStringLiteral("multiMode")).toBool(false)
            ? QStringLiteral("multi")
            : grpcSettings.value(QStringLiteral("mode")).toString().trimmed();
    }

    const QJsonObject httpupgradeSettings = streamSettings.value(QStringLiteral("httpupgradeSettings")).toObject();
    if (!httpupgradeSettings.isEmpty()) {
        item.path = httpupgradeSettings.value(QStringLiteral("path")).toString().trimmed();
        item.requestHost = joinStringValues(httpupgradeSettings.value(QStringLiteral("host")));
        item.userAgent = readHeaderValue(httpupgradeSettings.value(QStringLiteral("headers")).toObject(), QStringLiteral("User-Agent"));
    }

    const QJsonObject xhttpSettings = streamSettings.value(QStringLiteral("xhttpSettings")).toObject();
    if (!xhttpSettings.isEmpty()) {
        item.path = xhttpSettings.value(QStringLiteral("path")).toString().trimmed();
        item.requestHost = joinStringValues(xhttpSettings.value(QStringLiteral("host")));
        item.headerType = xhttpSettings.value(QStringLiteral("mode")).toString().trimmed();
        item.extra = compactJsonText(xhttpSettings.value(QStringLiteral("extra")));
    }
}

QString ConfigFileImportParser::joinStringValues(const QJsonValue& value)
{
    const QStringList values = toStringList(value);
    return values.join(QStringLiteral(","));
}

QStringList ConfigFileImportParser::toStringList(const QJsonValue& value)
{
    QStringList values;

    if (value.isString()) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty()) {
            values.append(text);
        }
        return values;
    }

    if (!value.isArray()) {
        return values;
    }

    const QJsonArray array = value.toArray();
    values.reserve(array.size());
    for (const QJsonValue& item : array) {
        const QString text = item.toString().trimmed();
        if (!text.isEmpty()) {
            values.append(text);
        }
    }

    return values;
}
