#pragma once

#include <optional>

#include <QList>
#include <QMap>
#include <QString>

#include "domain/models/CoreTypeItem.h"
#include "domain/models/GlobalHotkeyItem.h"
#include "domain/models/PolicyGroupItem.h"
#include "domain/models/RoutingItem.h"
#include "domain/models/SubItem.h"
#include "domain/models/TunModeItem.h"
#include "domain/models/VmessItem.h"

struct DefaultsConfigState {
    QString speedPingTestUrl;
    QString defIeProxyExceptions;
};

struct DnsConfigState {
    bool enableFragment = false;
    bool enableCacheFile4Sbox = true;
    QString defaultFingerprint;
    QString defaultUserAgent;
    QString directDns = QStringLiteral("https://dns.alidns.com/dns-query");
    QString remoteDns = QStringLiteral("https://cloudflare-dns.com/dns-query");
    QString bootstrapDns = QStringLiteral("223.5.5.5");
    bool fakeIp = false;
    bool globalFakeIp = true;
    bool serveStale = false;
    bool parallelQuery = false;
    QString directExpectedIps;
    bool useSystemHosts = false;
    bool addCommonHosts = true;
    bool blockBindingQuery = true;
    QString domainStrategyForFreedom;
    QString domainStrategyForProxy;
    QString dnsHosts;
    bool defaultAllowInsecure = false;
    QString domainStrategy;
    QString domainStrategy4Singbox;
    QString domainMatcher;
};

struct UiConfigState {
    UiConfigState& ui() { return *this; }
    const UiConfigState& ui() const { return *this; }

    bool showMainOnStartup = true;
    bool autoRunEnabled = false;
    QString languageCode;
    QString themeName = QStringLiteral("Light");
    int mainLocationX = 0;
    int mainLocationY = 0;
    int mainSizeWidth = 0;
    int mainSizeHeight = 0;
    QMap<QString, int> mainColumnWidths;
    int mainServerSortColumn = -1;
    int mainServerSortOrder = 0;
    QString mainSelectedSubId;
    QString settingsRoutingRuleTabKey;
    int mainServerLogSplitPercent = 60;
    int mainServerQrSplitPercent = 78;
    bool mainQrPreviewVisible = false;
    bool mainProxyEnabled = false;
};

struct RootConfigState {
    DefaultsConfigState defaultsState;
    DnsConfigState dnsState;

    DefaultsConfigState& defaults() { return defaultsState; }
    const DefaultsConfigState& defaults() const { return defaultsState; }

    DnsConfigState& dns() { return dnsState; }
    const DnsConfigState& dns() const { return dnsState; }

    bool logEnabled = false;
    QString logLevel;
    QString currentIndexId;
    int localPort = 10808;
    int localHttpPort = 0;
    int localLocationProbePort = 0;
    QString localProtocol = QStringLiteral("socks");
    bool udpEnabled = true;
    bool sniffingEnabled = true;
    bool routeOnly = false;
    bool allowLanConnection = false;
    QString inboundUser;
    QString inboundPassword;
    bool muxEnabled = false;
    QString mux4SboxProtocol = QStringLiteral("h2mux");
    int mux4SboxMaxConnections = 8;
    std::optional<bool> mux4SboxPadding;
    int sysProxyType = 0;
    bool ignoreGeoUpdateCore = false;
    QString systemProxyAdvancedProtocol;
    bool checkPreReleaseUpdate = false;
};

struct CollectionConfigState {
    QList<VmessItem> servers;
    QList<SubItem> subscriptions;
    QList<GlobalHotkeyItem> globalHotkeys;
    int routingIndex = 0;
    bool enableRoutingAdvanced = false;
    QList<RoutingItem> routingItems;
    QList<RoutingRule> routingCustomRules;
};

struct TunConfigState {
    TunModeItem tunModeItem;
};

struct PolicyConfigState {
    QList<CoreTypeItem> coreTypeItems;
    QList<PolicyGroupItem> policyGroups;
};

struct Config : RootConfigState {
    UiConfigState uiState;
    CollectionConfigState collectionState;
    TunConfigState tunState;
    PolicyConfigState policyState;

    RootConfigState& root() { return *this; }
    const RootConfigState& root() const { return *this; }

    DefaultsConfigState& defaults() { return root().defaults(); }
    const DefaultsConfigState& defaults() const { return root().defaults(); }

    DnsConfigState& dns() { return root().dns(); }
    const DnsConfigState& dns() const { return root().dns(); }

    UiConfigState& ui() { return uiState; }
    const UiConfigState& ui() const { return uiState; }

    CollectionConfigState& collection() { return collectionState; }
    const CollectionConfigState& collection() const { return collectionState; }

    TunConfigState& tun() { return tunState; }
    const TunConfigState& tun() const { return tunState; }

    PolicyConfigState& policy() { return policyState; }
    const PolicyConfigState& policy() const { return policyState; }
};
