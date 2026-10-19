#pragma once

#include <QString>

#include "domain/models/VmessItem.h"

// Generates a stable deduplication key for a proxy node.
//
// Unlike SubscriptionService::serverReuseKey(), which is a fine-grained identity
// key (transport details, TLS fingerprint, flow, etc.) used to preserve test
// results across subscription refreshes, this key is intentionally coarse:
//
//   configType + address + port + credentials
//
// "credentials" means the secret that proves you can use the proxy — UUID for
// VMess/VLESS/Trojan, password for Shadowsocks/Hysteria2/TUIC, username+password
// for Socks/HTTP/Naive, public key for WireGuard.  Name, transport wrapping,
// and TLS settings are deliberately ignored so that the same endpoint advertised
// with two different display names or transports still compares as equal.
//
// This is used by the export dedup pass: when the same server appears in multiple
// subscriptions, only the first occurrence (highest-priority subscription, lowest
// list index) is emitted.
namespace NodeFingerprint {

inline QString compute(const VmessItem& item)
{
    const QString normalizedAddress = item.address.trimmed().toLower();
    const QString portStr = QString::number(item.port);
    const QString typeStr = QString::number(static_cast<int>(item.configType));

    // The credential field differs by protocol. Normalise to lower-case + trimmed
    // so that the same UUID spelled with different case doesn't escape dedup.
    QString cred;
    switch (item.configType) {
    case ConfigType::WireGuard:
        cred = item.peerPublicKey.trimmed().toLower();
        break;
    case ConfigType::Socks:
    case ConfigType::HTTP:
    case ConfigType::Naive:
        cred = item.username.trimmed().toLower()
            + QStringLiteral(":") + item.id.trimmed().toLower();
        break;
    default:
        // VMess, VLESS, Trojan, Shadowsocks, Hysteria2, TUIC, AnyTLS, Custom, Unknown
        cred = item.id.trimmed().toLower();
        break;
    }

    return typeStr + QLatin1Char('|') + normalizedAddress + QLatin1Char('|')
        + portStr + QLatin1Char('|') + cred;
}

} // namespace NodeFingerprint
