#include "subscription/ClashYamlBuilder.h"

#include <QStringList>

namespace {

// Quotes a YAML flow-scalar value. Omits quotes when the value contains only
// safe characters; wraps in double-quotes and escapes backslash and the quote
// character otherwise. This is not a full YAML emitter -- it only needs to
// produce valid output for the field values that proxy entries actually contain.
QString q(const QString& value)
{
    if (value.isEmpty()) {
        return QStringLiteral("\"\"");
    }

    bool needsQuotes = false;
    for (const QChar ch : value) {
        if (ch == QLatin1Char('"') || ch == QLatin1Char('\\') || ch == QLatin1Char(',')
            || ch == QLatin1Char('{') || ch == QLatin1Char('}') || ch == QLatin1Char('[')
            || ch == QLatin1Char(']') || ch == QLatin1Char(':') || ch == QLatin1Char('#')
            || ch == QLatin1Char('&') || ch == QLatin1Char('*') || ch == QLatin1Char('!')
            || ch == QLatin1Char('|') || ch == QLatin1Char('>') || ch == QLatin1Char('\'')
            || ch == QLatin1Char('%') || ch == QLatin1Char('@') || ch == QLatin1Char('`')
            || ch.unicode() < 0x20) {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) {
        // Also quote strings that YAML would misinterpret as booleans / null / numbers.
        static const QStringList kReserved{
            QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("yes"),
            QStringLiteral("no"), QStringLiteral("on"), QStringLiteral("off"),
            QStringLiteral("null"), QStringLiteral("~")};
        if (kReserved.contains(value.toLower())) {
            needsQuotes = true;
        }
    }

    if (!needsQuotes) {
        return value;
    }

    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    const QChar quote = QLatin1Char('"');
    return quote + escaped + quote;
}

// Appends "key: value" to fields when value is non-empty.
void appendField(QStringList& fields, const QString& key, const QString& value)
{
    if (!value.trimmed().isEmpty()) {
        fields.append(key + QStringLiteral(": ") + q(value));
    }
}

// Appends "key: number" to fields (always included since port / alterId are
// meaningful even when 0 for some protocols).
void appendIntField(QStringList& fields, const QString& key, int value)
{
    fields.append(key + QStringLiteral(": ") + QString::number(value));
}

// Appends "key: true/false" to fields when the value differs from the default.
void appendBoolFieldIfTrue(QStringList& fields, const QString& key, const QString& value)
{
    if (value.trimmed() == QStringLiteral("true")
        || value.trimmed() == QStringLiteral("1")) {
        fields.append(key + QStringLiteral(": true"));
    }
}

// Appends a YAML inline sequence, e.g. "alpn: [h2, http/1.1]".
void appendAlpn(QStringList& fields, const QStringList& alpn)
{
    if (alpn.isEmpty()) {
        return;
    }
    QStringList quoted;
    for (const QString& v : alpn) {
        quoted.append(q(v));
    }
    fields.append(QStringLiteral("alpn: [") + quoted.join(QStringLiteral(", ")) + QLatin1Char(']'));
}

// Appends the TLS fragment shared by most protocols.
void appendTlsFields(QStringList& fields, const VmessItem& item)
{
    if (item.streamSecurity == QStringLiteral("tls")
        || item.streamSecurity == QStringLiteral("reality")) {
        fields.append(QStringLiteral("tls: true"));
    }
    appendField(fields, QStringLiteral("sni"), item.sni);
    appendBoolFieldIfTrue(fields, QStringLiteral("skip-cert-verify"), item.allowInsecure);
    appendField(fields, QStringLiteral("client-fingerprint"), item.fingerprint);
    appendAlpn(fields, item.alpn);
}

// Appends the ws-opts / grpc-opts / h2-opts transport fragment.
void appendTransportFields(QStringList& fields, const VmessItem& item)
{
    const QString& net = item.network;
    if (net == QStringLiteral("ws")) {
        appendField(fields, QStringLiteral("network"), item.network);
        QStringList wsOpts;
        appendField(wsOpts, QStringLiteral("path"), item.path);
        if (!item.requestHost.isEmpty()) {
            wsOpts.append(QStringLiteral("headers: {Host: ") + q(item.requestHost) + QLatin1Char('}'));
        }
        if (!wsOpts.isEmpty()) {
            fields.append(QStringLiteral("ws-opts: {") + wsOpts.join(QStringLiteral(", ")) + QLatin1Char('}'));
        }
    } else if (net == QStringLiteral("grpc")) {
        appendField(fields, QStringLiteral("network"), item.network);
        if (!item.path.isEmpty()) {
            fields.append(QStringLiteral("grpc-opts: {grpc-service-name: ") + q(item.path) + QLatin1Char('}'));
        }
    } else if (net == QStringLiteral("h2")) {
        appendField(fields, QStringLiteral("network"), item.network);
        QStringList h2Opts;
        appendField(h2Opts, QStringLiteral("path"), item.path);
        if (!item.requestHost.isEmpty()) {
            h2Opts.append(QStringLiteral("host: [") + q(item.requestHost) + QLatin1Char(']'));
        }
        if (!h2Opts.isEmpty()) {
            fields.append(QStringLiteral("h2-opts: {") + h2Opts.join(QStringLiteral(", ")) + QLatin1Char('}'));
        }
    } else if (net != QStringLiteral("tcp") && !net.isEmpty()) {
        appendField(fields, QStringLiteral("network"), item.network);
    }
}

QString buildVmess(const VmessItem& item)
{
    QStringList fields;
    fields.append(QStringLiteral("type: vmess"));
    appendField(fields, QStringLiteral("name"), item.remarks);
    appendField(fields, QStringLiteral("server"), item.address);
    appendIntField(fields, QStringLiteral("port"), item.port);
    appendField(fields, QStringLiteral("uuid"), item.id);
    appendIntField(fields, QStringLiteral("alterId"), item.alterId);
    appendField(fields, QStringLiteral("cipher"),
                item.security.isEmpty() ? QStringLiteral("auto") : item.security);
    appendTlsFields(fields, item);
    appendTransportFields(fields, item);
    return QLatin1Char('{') + fields.join(QStringLiteral(", ")) + QLatin1Char('}');
}

QString buildVless(const VmessItem& item)
{
    QStringList fields;
    fields.append(QStringLiteral("type: vless"));
    appendField(fields, QStringLiteral("name"), item.remarks);
    appendField(fields, QStringLiteral("server"), item.address);
    appendIntField(fields, QStringLiteral("port"), item.port);
    appendField(fields, QStringLiteral("uuid"), item.id);
    appendField(fields, QStringLiteral("flow"), item.flow);
    appendTlsFields(fields, item);
    appendTransportFields(fields, item);
    return QLatin1Char('{') + fields.join(QStringLiteral(", ")) + QLatin1Char('}');
}

QString buildTrojan(const VmessItem& item)
{
    QStringList fields;
    fields.append(QStringLiteral("type: trojan"));
    appendField(fields, QStringLiteral("name"), item.remarks);
    appendField(fields, QStringLiteral("server"), item.address);
    appendIntField(fields, QStringLiteral("port"), item.port);
    appendField(fields, QStringLiteral("password"), item.id);
    appendField(fields, QStringLiteral("sni"), item.sni);
    appendBoolFieldIfTrue(fields, QStringLiteral("skip-cert-verify"), item.allowInsecure);
    appendField(fields, QStringLiteral("client-fingerprint"), item.fingerprint);
    appendAlpn(fields, item.alpn);
    appendTransportFields(fields, item);
    return QLatin1Char('{') + fields.join(QStringLiteral(", ")) + QLatin1Char('}');
}

QString buildShadowsocks(const VmessItem& item)
{
    QStringList fields;
    fields.append(QStringLiteral("type: ss"));
    appendField(fields, QStringLiteral("name"), item.remarks);
    appendField(fields, QStringLiteral("server"), item.address);
    appendIntField(fields, QStringLiteral("port"), item.port);
    appendField(fields, QStringLiteral("password"), item.id);
    appendField(fields, QStringLiteral("cipher"), item.security);
    return QLatin1Char('{') + fields.join(QStringLiteral(", ")) + QLatin1Char('}');
}

QString buildSocksOrHttp(const VmessItem& item)
{
    const QString type = item.configType == ConfigType::HTTP
        ? QStringLiteral("http") : QStringLiteral("socks5");
    QStringList fields;
    fields.append(QStringLiteral("type: ") + type);
    appendField(fields, QStringLiteral("name"), item.remarks);
    appendField(fields, QStringLiteral("server"), item.address);
    appendIntField(fields, QStringLiteral("port"), item.port);
    appendField(fields, QStringLiteral("username"), item.id);
    appendField(fields, QStringLiteral("password"), item.security);
    if (item.streamSecurity == QStringLiteral("tls")) {
        fields.append(QStringLiteral("tls: true"));
        appendField(fields, QStringLiteral("sni"), item.sni);
        appendBoolFieldIfTrue(fields, QStringLiteral("skip-cert-verify"), item.allowInsecure);
    }
    return QLatin1Char('{') + fields.join(QStringLiteral(", ")) + QLatin1Char('}');
}

QString buildHysteria2(const VmessItem& item)
{
    QStringList fields;
    fields.append(QStringLiteral("type: hysteria2"));
    appendField(fields, QStringLiteral("name"), item.remarks);
    appendField(fields, QStringLiteral("server"), item.address);
    appendIntField(fields, QStringLiteral("port"), item.port);
    appendField(fields, QStringLiteral("password"), item.id);
    appendField(fields, QStringLiteral("sni"), item.sni);
    appendBoolFieldIfTrue(fields, QStringLiteral("skip-cert-verify"), item.allowInsecure);
    appendField(fields, QStringLiteral("obfs-password"), item.obfsPassword);
    return QLatin1Char('{') + fields.join(QStringLiteral(", ")) + QLatin1Char('}');
}

QString buildTuic(const VmessItem& item)
{
    QStringList fields;
    fields.append(QStringLiteral("type: tuic"));
    appendField(fields, QStringLiteral("name"), item.remarks);
    appendField(fields, QStringLiteral("server"), item.address);
    appendIntField(fields, QStringLiteral("port"), item.port);
    appendField(fields, QStringLiteral("uuid"), item.id);
    appendField(fields, QStringLiteral("password"), item.security);
    appendField(fields, QStringLiteral("sni"), item.sni);
    appendBoolFieldIfTrue(fields, QStringLiteral("skip-cert-verify"), item.allowInsecure);
    appendAlpn(fields, item.alpn);
    return QLatin1Char('{') + fields.join(QStringLiteral(", ")) + QLatin1Char('}');
}

QString buildAnytls(const VmessItem& item)
{
    QStringList fields;
    fields.append(QStringLiteral("type: anytls"));
    appendField(fields, QStringLiteral("name"), item.remarks);
    appendField(fields, QStringLiteral("server"), item.address);
    appendIntField(fields, QStringLiteral("port"), item.port);
    appendField(fields, QStringLiteral("password"), item.id);
    appendField(fields, QStringLiteral("sni"), item.sni);
    appendBoolFieldIfTrue(fields, QStringLiteral("skip-cert-verify"), item.allowInsecure);
    appendField(fields, QStringLiteral("client-fingerprint"), item.fingerprint);
    appendAlpn(fields, item.alpn);
    appendField(fields, QStringLiteral("idle-session-check-interval"), item.idleSessionCheckInterval);
    appendField(fields, QStringLiteral("idle-session-timeout"), item.idleSessionTimeout);
    appendField(fields, QStringLiteral("min-idle-session"), item.minIdleSession);
    return QLatin1Char('{') + fields.join(QStringLiteral(", ")) + QLatin1Char('}');
}

QString buildWireguard(const VmessItem& item)
{
    QStringList fields;
    fields.append(QStringLiteral("type: wireguard"));
    appendField(fields, QStringLiteral("name"), item.remarks);
    appendField(fields, QStringLiteral("server"), item.address);
    appendIntField(fields, QStringLiteral("port"), item.port);
    appendField(fields, QStringLiteral("private-key"), item.privateKey);
    appendField(fields, QStringLiteral("public-key"), item.peerPublicKey);

    // local-address: stored as a comma-joined string; emit as a YAML list.
    if (!item.localAddress.isEmpty()) {
        const QStringList addrs = item.localAddress.split(QLatin1Char(','),
                                                          Qt::SkipEmptyParts);
        QStringList quoted;
        for (const QString& a : addrs) {
            quoted.append(q(a.trimmed()));
        }
        if (addrs.size() == 1) {
            fields.append(QStringLiteral("ip: ") + q(addrs.constFirst().trimmed()));
        } else if (addrs.size() >= 2) {
            fields.append(QStringLiteral("ip: ") + q(addrs.at(0).trimmed()));
            fields.append(QStringLiteral("ipv6: ") + q(addrs.at(1).trimmed()));
        }
    }

    if (item.wireguardMtu > 0) {
        appendIntField(fields, QStringLiteral("mtu"), item.wireguardMtu);
    }
    appendField(fields, QStringLiteral("reserved"), item.reserved);
    return QLatin1Char('{') + fields.join(QStringLiteral(", ")) + QLatin1Char('}');
}

} // namespace

namespace ClashYamlBuilder {

QString buildProxyEntry(const VmessItem& item)
{
    if (item.address.trimmed().isEmpty() || item.port <= 0 || item.port > 65535) {
        return {};
    }

    switch (item.configType) {
    case ConfigType::VMess:
        return buildVmess(item);
    case ConfigType::VLESS:
        return buildVless(item);
    case ConfigType::Trojan:
        return buildTrojan(item);
    case ConfigType::Shadowsocks:
        return buildShadowsocks(item);
    case ConfigType::Socks:
    case ConfigType::HTTP:
        return buildSocksOrHttp(item);
    case ConfigType::Hysteria2:
        return buildHysteria2(item);
    case ConfigType::TUIC:
        return buildTuic(item);
    case ConfigType::AnyTLS:
        return buildAnytls(item);
    case ConfigType::WireGuard:
        return buildWireguard(item);
    case ConfigType::Custom:
    case ConfigType::Unknown:
    default:
        return {};
    }
}

QString buildProxiesBlock(const QList<VmessItem>& items)
{
    QStringList lines;
    for (const VmessItem& item : items) {
        const QString entry = buildProxyEntry(item);
        if (!entry.isEmpty()) {
            lines.append(QStringLiteral("  - ") + entry);
        }
    }

    if (lines.isEmpty()) {
        return QStringLiteral("proxies: []\n");
    }

    return QStringLiteral("proxies:\n") + lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

} // namespace ClashYamlBuilder
