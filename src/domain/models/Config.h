#pragma once

#include <optional>

#include <QList>
#include <QMap>
#include <QString>

#include "common/SystemProxySnapshot.h"
#include "domain/models/CoreTypeItem.h"
#include "domain/models/RoutingItem.h"
#include "domain/models/SubItem.h"
#include "domain/models/TunModeItem.h"
#include "domain/models/VmessItem.h"

struct DefaultsConfigState {
    QString speedPingTestUrl;
    QString defIeProxyExceptions;
};

struct DnsConfigState {
    // Declared on purpose, and not redundant. Without it this type is an aggregate, and MSVC 19.29
    // compiling the Debug test trees (/Od /Ob0 /RTC1) mis-initialises the three resolver defaults
    // when the object is built from an empty brace list: `DnsConfigState{}` reports remoteDns with
    // directDns's value and bootstrapDns with remoteDns's value, while `DnsConfigState()` and a
    // plain declaration are both correct. Declaring the constructor moves the brace spelling off
    // the aggregate-initialisation path, so all three spellings agree.
    // Guarded by loadMissingFileBuildsDefaultSongBirdConfig in tests/JsonConfigRepositoryTests.cpp.
    DnsConfigState() = default;
    bool enableFragment = false;
    bool enableCacheFile4Sbox = true;
    QString defaultFingerprint;
    QString defaultUserAgent;
    // IP form rather than the dns.alidns.com hostname, for the same reason remoteDns uses
    // one: DnsHosts::predefined() holds a hard-coded snapshot of what AliDNS answers with today,
    // and a hostname default would silently depend on that table staying current. AliDNS
    // publishes IP:223.5.5.5 in the certificate SAN, so the encrypted form still validates.
    QString directDns = QStringLiteral("https://223.5.5.5/dns-query");
    // IP form, not the cloudflare-dns.com hostname: a hostname-addressed resolver can only be
    // reached after its own name is resolved, which the generator covers with a hard-coded hosts
    // snapshot (DnsHosts::predefined()). Those snapshot addresses are what Cloudflare happens to
    // answer with today, so a hostname default would silently depend on a stale table -- and it
    // would fail as a DNS timeout, not as an error naming the cause. The certificate carries
    // IP:1.1.1.1 as a SAN, so TLS validation still succeeds.
    QString remoteDns = QStringLiteral("https://1.1.1.1/dns-query");
    // Plain 223.5.5.5 was sent as cleartext UDP 53, which is observable and spoofable on the
    // networks this default is meant for. AliDNS publishes IP:223.5.5.5 in the certificate SAN,
    // so the encrypted form validates against the address itself.
    QString bootstrapDns = QStringLiteral("https://223.5.5.5/dns-query");    // Resolves the proxy node's own hostname, which has to happen before the node can be dialled
    // and therefore can never go through it. Empty keeps the historical behaviour of letting the
    // direct DNS answer that lookup through route.default_domain_resolver.
    QString proxyServerDns;
    bool fakeIp = false;
    // Only read while fakeIp is on (see SingBoxDnsConfigBuilder); defaulting it to true made the
    // stored value disagree with the behaviour, since a default install never enables FakeIP.
    bool globalFakeIp = false;
    bool serveStale = false;
    bool parallelQuery = false;
    QString directExpectedIps;
    bool useSystemHosts = false;
    bool addCommonHosts = true;
    bool blockBindingQuery = true;
    QString domainStrategyForFreedom;
    QString domainStrategyForProxy;
    QString dnsHosts;
    // On by default: an expired or hostname-mismatched certificate is common on the free and
    // self-hosted nodes this app is pointed at, and a node that cannot complete a handshake is
    // indistinguishable from a dead node to a user who does not know to look here.
    bool defaultAllowInsecure = true;
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
    // The location-grouping toggle on the subscription tab bar: when true the tab bar shows
    // location items (Favorite / US / SG / TW(HK+TW) / JP / KR / GB / DE / RU / AU / Other) and the
    // selected item is mainSelectedLocationId. Persisted in the .state.json so the grouping mode
    // survives a restart. Subscription view keeps using mainSelectedSubId; the two selections are
    // independent and only one of them is meaningful at a time.
    bool mainGroupByLocation = false;
    QString mainSelectedLocationId;
    // The checkable "auto switch" action in a location item's context menu: when set and the
    // active node fails for good (the session reports failed, meaning the crash-restart path
    // could not bring it back), the composition root restarts the proxy on the next node in the
    // same location group. Session-wide preference, persisted with the rest of the ui state.
    bool autoSwitchInGroup = false;
    QString settingsRoutingRuleTabKey;
    int mainServerLogSplitPercent = 60;
    int mainServerQrSplitPercent = 78;
    bool mainQrPreviewVisible = false;
    bool mainProxyEnabled = false;
    QString autoSelectionStrategy = QStringLiteral("lowestLatency");
    // One-shot flag for the startup prompt that offers to download a core when
    // none was found anywhere. Persisted so the offer is spent after the first
    // launch instead of returning on every start.
    bool coreSetupPromptShown = false;
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
    // Advanced escape hatch: skip the startup outbound gate entirely and apply the system proxy
    // without probing. Off by default -- the gate is what stops a dead node from being reported as
    // active -- but a user who already knows their node works, or whose network blocks every
    // generate_204 host, needs a way past a false negative.
    bool skipOutboundCheck = false;
    QString systemProxyAdvancedProtocol;
    bool checkPreReleaseUpdate = false;
};

struct CollectionConfigState {
    QList<VmessItem> servers;
    QList<SubItem> subscriptions;
    QString routingModeId = QStringLiteral("builtin:whitelist");
    QList<RoutingItem> customRoutingItems;
    QList<RoutingRule> routingCustomRules;
};

struct TunConfigState {
    TunModeItem tunModeItem;
};

struct PolicyConfigState {
    QList<CoreTypeItem> coreTypeItems;
};

struct Config : RootConfigState {
    UiConfigState uiState;
    CollectionConfigState collectionState;
    TunConfigState tunState;
    PolicyConfigState policyState;
    // What the system proxy looked like before this program first changed it.
    //
    // State-only data, written to the .state.json companion rather than the primary config:
    // it describes this machine's registry at one moment, not the user's preferences, and it
    // must not travel with a config file copied to another machine. Persisting it is what makes
    // a faithful restore possible after the process is killed -- an in-memory-only snapshot can
    // only recognise our own leftovers, never put the user's values back.
    //
    // A default-constructed value has `valid == false`, which means "nothing to restore".
    SystemProxySnapshot systemProxySnapshotState;

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

    SystemProxySnapshot& systemProxySnapshot() { return systemProxySnapshotState; }
    const SystemProxySnapshot& systemProxySnapshot() const { return systemProxySnapshotState; }
};
