#pragma once

#include <optional>
#include <QString>
#include <QStringList>

enum class CoreType {
    Xray = 2,
    SingBox = 24,
    Mihomo = 25,
    Unknown = 999
};

inline QString coreTypeDisplayName(CoreType type)
{
    switch (type) {
    case CoreType::Xray:
        return QStringLiteral("Xray");
    case CoreType::SingBox:
        return QStringLiteral("sing-box");
    case CoreType::Mihomo:
        return QStringLiteral("Mihomo");
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
    case CoreType::Mihomo:
        return CoreType::Mihomo;
    case CoreType::Unknown:
        return CoreType::Unknown;
    default:
        return CoreType::SingBox;
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
    QString packetEncoding;
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
    // AnyTLS fields
    QString idleSessionCheckInterval;
    QString idleSessionTimeout;
    QString minIdleSession;
    // State, not configuration: the probe history that feeds the 24-hour health strip, encoded by
    // appcore/HealthStripPolicy ('+' / '-' / '0', oldest hour first). Persisted through
    // JsonConfigStateSerialization next to testResult, so it survives a restart without touching
    // the primary config file. Empty means "no history", which is also what an old state file
    // yields -- the strip then shows 24 untested cells, which is the truth.
    QString healthStrip;
    // State, not configuration, and persisted beside testResult/healthStrip in the .state.json's
    // serverStates[]: a location assignment made by hand (right-click "move to location", or the
    // exit-location fix) outranks the name-based inference on every refresh, so it must survive a
    // subscription update -- and updates carry it across because the same serverReuseKey path that
    // preserves testResult re-attaches the old indexId. Empty means "infer from the name". The
    // value is a LocationCatalog item id; FAVORITE is not a valid value (favoriting is its own
    // flag), and an unknown value falls back to inference at read time.
    QString locationOverride;
    // State: hand-picked nodes for the Favorite location item. Independent of the location:
    // a node can be favorite and US at the same time.
    bool favorite = false;
};
