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

struct Config {
    bool logEnabled = false;
    QString logLevel;
    QString currentIndexId;
    int localPort = 10808;
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
    bool enableStatistics = false;
    int statisticsFreshRate = 1;
    bool showMainOnStartup = true;
    bool autoRunEnabled = false;
    QString languageCode;
    int mainLocationX = 0;
    int mainLocationY = 0;
    int mainSizeWidth = 0;
    int mainSizeHeight = 0;
    QMap<QString, int> mainColumnWidths;
    QString mainSelectedSubId;
    QString settingsRoutingRuleTabKey;
    int mainServerLogSplitPercent = 60;
    int mainServerQrSplitPercent = 78;
    bool mainProxyEnabled = false;
    QString speedPingTestUrl;
    QString defIeProxyExceptions;
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
    int routingIndex = 0;
    bool enableRoutingAdvanced = false;
    bool ignoreGeoUpdateCore = false;
    QString systemProxyExceptions;
    QString systemProxyAdvancedProtocol;
    QString pacUrl;
    bool checkPreReleaseUpdate = false;
    int trayMenuServersLimit = 0;

    QList<VmessItem> servers;
    QList<SubItem> subscriptions;
    QList<GlobalHotkeyItem> globalHotkeys;
    QList<RoutingItem> routingItems;
    QList<RoutingRule> routingCustomRules;
    QList<CoreTypeItem> coreTypeItems;
    TunModeItem tunModeItem;
    QList<PolicyGroupItem> policyGroups;
};

