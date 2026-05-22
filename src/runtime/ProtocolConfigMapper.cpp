#include "runtime/ProtocolConfigMapper.h"

#include <QStringList>

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

} // namespace

QString ProtocolConfigMapper::resolveSingBoxNetwork(const VmessItem& server)
{
    const QString network = server.network.trimmed().isEmpty()
        ? QStringLiteral("tcp")
        : server.network.trimmed().toLower();

    if (network == QStringLiteral("ws")
        || network == QStringLiteral("grpc")
        || network == QStringLiteral("h2")
        || network == QStringLiteral("httpupgrade")) {
        return QStringLiteral("tcp");
    }

    if (network == QStringLiteral("quic")) {
        return QStringLiteral("udp");
    }

    return network;
}

QString ProtocolConfigMapper::resolveSingBoxOutboundType(ConfigType type)
{
    switch (type) {
    case ConfigType::VMess:
        return QStringLiteral("vmess");
    case ConfigType::VLESS:
        return QStringLiteral("vless");
    case ConfigType::Trojan:
        return QStringLiteral("trojan");
    case ConfigType::Shadowsocks:
        return QStringLiteral("shadowsocks");
    case ConfigType::Socks:
        return QStringLiteral("socks");
    case ConfigType::HTTP:
        return QStringLiteral("http");
    case ConfigType::Hysteria2:
        return QStringLiteral("hysteria2");
    case ConfigType::TUIC:
        return QStringLiteral("tuic");
    case ConfigType::WireGuard:
        return QStringLiteral("wireguard");
    case ConfigType::AnyTLS:
        return QStringLiteral("anytls");
    case ConfigType::Naive:
        return QStringLiteral("naive");
    case ConfigType::Custom:
    case ConfigType::Unknown:
    default:
        return {};
    }
}

QString ProtocolConfigMapper::normalizeSingBoxLogLevel(const QString& level)
{
    const QString normalized = level.trimmed().toLower();
    if (normalized.isEmpty()) {
        return QStringLiteral("warn");
    }
    if (normalized == QStringLiteral("warning")) {
        return QStringLiteral("warn");
    }
    return normalized;
}

QString ProtocolConfigMapper::resolveServerName(const VmessItem& server)
{
    if (!server.sni.trimmed().isEmpty()) {
        return server.sni;
    }

    const QStringList hosts = splitCsv(server.requestHost);
    if (!hosts.isEmpty()) {
        return hosts.constFirst();
    }

    return server.address;
}

QString ProtocolConfigMapper::resolveFingerprint(const Config& config, const VmessItem& server)
{
    return server.fingerprint.trimmed().isEmpty()
        ? config.dns().defaultFingerprint.trimmed()
        : server.fingerprint.trimmed();
}

QString ProtocolConfigMapper::resolveRealityFingerprint(const Config& config, const VmessItem& server)
{
    const QString fingerprint = resolveFingerprint(config, server);
    return fingerprint.isEmpty() ? QStringLiteral("chrome") : fingerprint;
}

bool ProtocolConfigMapper::isRealityTransport(const QString& value)
{
    return value.compare(QStringLiteral("reality"), Qt::CaseInsensitive) == 0;
}

bool ProtocolConfigMapper::isValidRealityShortId(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return true;
    }

    if (trimmed.size() > 16 || (trimmed.size() % 2) != 0) {
        return false;
    }

    for (const QChar ch : trimmed) {
        if (!ch.isDigit() && (ch.toLower() < QChar('a') || ch.toLower() > QChar('f'))) {
            return false;
        }
    }

    return true;
}

bool ProtocolConfigMapper::resolveAllowInsecure(const QString& value, bool fallbackValue)
{
    if (value.trimmed().isEmpty()) {
        return fallbackValue;
    }

    return value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("1"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0;
}
