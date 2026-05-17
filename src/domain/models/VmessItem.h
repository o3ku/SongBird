#pragma once

#include <optional>
#include <QString>
#include <QStringList>

enum class CoreType {
    Xray = 2,
    SingBox = 24,
    Unknown = 999
};

inline QString coreTypeDisplayName(CoreType type)
{
    switch (type) {
    case CoreType::Xray:
        return QStringLiteral("Xray");
    case CoreType::SingBox:
        return QStringLiteral("sing-box");
    case CoreType::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

inline CoreType resolveRuntimeCoreType(CoreType type)
{
    switch (type) {
    case CoreType::Xray:
        return CoreType::Xray;
    case CoreType::SingBox:
        return CoreType::SingBox;
    case CoreType::Unknown:
        return CoreType::Unknown;
    default:
        return CoreType::Xray;
    }
}

enum class ConfigType {
    VMess = 1,
    Custom = 2,
    Shadowsocks = 3,
    Socks = 4,
    VLESS = 5,
    Trojan = 6,
    Hysteria2 = 7,
    TUIC = 8,
    WireGuard = 9,
    HTTP = 11,
    AnyTLS = 13,
    Naive = 14,
    Unknown = 999
};

inline QString configTypeDisplayName(ConfigType type)
{
    switch (type) {
    case ConfigType::VMess:
        return QStringLiteral("VMess");
    case ConfigType::Custom:
        return QStringLiteral("Custom");
    case ConfigType::Shadowsocks:
        return QStringLiteral("Shadowsocks");
    case ConfigType::Socks:
        return QStringLiteral("Socks");
    case ConfigType::VLESS:
        return QStringLiteral("VLESS");
    case ConfigType::Trojan:
        return QStringLiteral("Trojan");
    case ConfigType::HTTP:
        return QStringLiteral("HTTP");
    case ConfigType::Hysteria2:
        return QStringLiteral("Hysteria2");
    case ConfigType::TUIC:
        return QStringLiteral("TUIC");
    case ConfigType::WireGuard:
        return QStringLiteral("WireGuard");
    case ConfigType::AnyTLS:
        return QStringLiteral("AnyTLS");
    case ConfigType::Naive:
        return QStringLiteral("Naive");
    case ConfigType::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

struct VmessItem {
    QString indexId;
    ConfigType configType = ConfigType::VMess;
    CoreType coreType = CoreType::SingBox;
    QString address;
    int port = 0;
    QString id;
    int alterId = 0;
    QString security;
    QString network;
    QString remarks;
    QString headerType;
    QString requestHost;
    QString path;
    QString streamSecurity;
    QString allowInsecure;
    QString testResult;
    QString subId;
    QString flow;
    QString sni;
    QStringList alpn;
    int preSocksPort = 0;
    QString fingerprint;
    QString publicKey;
    QString shortId;
    QString spiderX;
    QString mldsa65Verify;
    QString echConfigList;
    QString echForceQuery;
    QString cert;
    QString certSha;
    std::optional<bool> muxEnabled;
    int sort = 0;
    QString extra;
    QString userAgent;
    QString finalmask;
    // Hysteria2 fields
    QString obfsPassword;
    QString upMbps;
    QString downMbps;
    // TUIC fields
    QString congestionControl;
    QString udpRelayMode;
    bool zeroRttHandshake = false;
    // WireGuard fields
    QString privateKey;
    QString peerPublicKey;
    QString localAddress;
    int wireguardMtu = 0;
    QString reserved;
    // Naive fields
    QString username;
    bool naiveQuic = false;
    int insecureConcurrency = 0;
};
