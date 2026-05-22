#include "persistence/JsonConfigRootSerialization.h"

#include <QJsonArray>

#include "persistence/JsonConfigUtils.h"

namespace {

constexpr int kSongBirdJsonConfigSchemaVersion = 7;

using namespace JsonConfigUtils;

} // namespace

namespace JsonConfigRootSerialization {

void read(const QJsonObject& root, RootConfigState& config)
{
    config.logEnabled = readBool(root, QStringLiteral("logEnabled"), false);
    config.logLevel = readString(root, QStringLiteral("logLevel"));
    config.currentIndexId = readString(root, QStringLiteral("indexId"));

    const QJsonArray inbounds = root.value(QStringLiteral("inbound")).toArray();
    if (!inbounds.isEmpty() && inbounds.at(0).isObject()) {
        const QJsonObject inbound = inbounds.at(0).toObject();
        config.localPort = readInt(inbound, QStringLiteral("localPort"), 10808);
        config.localProtocol = readString(inbound, QStringLiteral("protocol"), QStringLiteral("socks"));
        config.udpEnabled = readBool(inbound, QStringLiteral("udpEnabled"), true);
        config.sniffingEnabled = readBool(inbound, QStringLiteral("sniffingEnabled"), true);
        config.routeOnly = readBool(inbound, QStringLiteral("routeOnly"), false);
        config.allowLanConnection = readBool(inbound, QStringLiteral("allowLANConn"), false);
        config.inboundUser = readString(inbound, QStringLiteral("user"));
        config.inboundPassword = readString(inbound, QStringLiteral("pass"));
    }

    const QJsonObject defaults = root.value(QStringLiteral("defaults")).toObject();
    config.defaults().speedPingTestUrl = readString(defaults, QStringLiteral("speedPingTestUrl"));
    config.defaults().defIeProxyExceptions = readString(defaults, QStringLiteral("ieProxyExceptions"));

    config.muxEnabled = readBool(root, QStringLiteral("muxEnabled"), false);
    config.mux4SboxProtocol = root.contains(QStringLiteral("mux4SboxProtocol"))
        ? readString(root, QStringLiteral("mux4SboxProtocol"))
        : QStringLiteral("h2mux");
    config.mux4SboxMaxConnections = root.contains(QStringLiteral("mux4SboxMaxConnections"))
        ? readInt(root, QStringLiteral("mux4SboxMaxConnections"), 8)
        : 8;
    if (config.mux4SboxMaxConnections <= 0) {
        config.mux4SboxMaxConnections = 8;
    }
    if (root.contains(QStringLiteral("mux4SboxPadding")) && root.value(QStringLiteral("mux4SboxPadding")).isBool()) {
        config.mux4SboxPadding = readBool(root, QStringLiteral("mux4SboxPadding"));
    } else {
        config.mux4SboxPadding.reset();
    }

    config.sysProxyType = readInt(root, QStringLiteral("sysProxyType"), 0);

    config.dns().enableFragment = readBool(root, QStringLiteral("enableFragment"), false);
    config.dns().enableCacheFile4Sbox = root.contains(QStringLiteral("enableCacheFile4Sbox"))
        ? readBool(root, QStringLiteral("enableCacheFile4Sbox"), true)
        : true;
    config.dns().defaultFingerprint = readString(root, QStringLiteral("defaultFingerprint"));
    config.dns().defaultUserAgent = readString(root, QStringLiteral("defaultUserAgent"));
    if (root.contains(QStringLiteral("directDns"))) {
        config.dns().directDns = readString(root, QStringLiteral("directDns"));
    }
    if (root.contains(QStringLiteral("remoteDns"))) {
        config.dns().remoteDns = readString(root, QStringLiteral("remoteDns"));
    }
    if (root.contains(QStringLiteral("bootstrapDns"))) {
        config.dns().bootstrapDns = readString(root, QStringLiteral("bootstrapDns"));
    }
    if (root.contains(QStringLiteral("fakeIp"))) {
        config.dns().fakeIp = readBool(root, QStringLiteral("fakeIp"), false);
    }
    config.dns().globalFakeIp = root.contains(QStringLiteral("globalFakeIp"))
        ? readBool(root, QStringLiteral("globalFakeIp"), true)
        : true;
    config.dns().serveStale = root.contains(QStringLiteral("serveStale"))
        ? readBool(root, QStringLiteral("serveStale"), false)
        : false;
    config.dns().parallelQuery = readBool(root, QStringLiteral("parallelQuery"), false);
    config.dns().directExpectedIps = readString(root, QStringLiteral("directExpectedIps"));
    config.dns().useSystemHosts = root.contains(QStringLiteral("useSystemHosts"))
        ? readBool(root, QStringLiteral("useSystemHosts"), false)
        : false;
    config.dns().addCommonHosts = root.contains(QStringLiteral("addCommonHosts"))
        ? readBool(root, QStringLiteral("addCommonHosts"), true)
        : true;
    config.dns().blockBindingQuery = root.contains(QStringLiteral("blockBindingQuery"))
        ? readBool(root, QStringLiteral("blockBindingQuery"), true)
        : true;
    config.dns().domainStrategyForFreedom = readString(root, QStringLiteral("domainStrategyForFreedom"));
    config.dns().domainStrategyForProxy = readString(root, QStringLiteral("domainStrategyForProxy"));
    config.dns().dnsHosts = readString(root, QStringLiteral("dnsHosts"));
    config.dns().defaultAllowInsecure = readBool(root, QStringLiteral("defaultAllowInsecure"), false);
    config.dns().domainStrategy = readString(root, QStringLiteral("domainStrategy"));
    config.dns().domainStrategy4Singbox = readString(root, QStringLiteral("domainStrategyForSingbox"));
    config.dns().domainMatcher = readString(root, QStringLiteral("domainMatcher"));
    config.ignoreGeoUpdateCore = readBool(root, QStringLiteral("ignoreGeoUpdateCore"), false);
    config.systemProxyAdvancedProtocol = readString(root, QStringLiteral("systemProxyAdvancedProtocol"));
    config.checkPreReleaseUpdate = readBool(root, QStringLiteral("checkPreReleaseUpdate"), false);
}

void write(QJsonObject& root, const RootConfigState& config)
{
    root.insert(QStringLiteral("schemaVersion"), kSongBirdJsonConfigSchemaVersion);
    writeIfNotDefault(root, QStringLiteral("logEnabled"), config.logEnabled, false);
    writeIfNotEmpty(root, QStringLiteral("logLevel"), config.logLevel);
    writeIfNotEmpty(root, QStringLiteral("indexId"), config.currentIndexId);

    QJsonObject inbound;
    writeIfNotDefault(inbound, QStringLiteral("localPort"), config.localPort, 10808);
    if (config.localProtocol != QStringLiteral("socks")) {
        inbound.insert(QStringLiteral("protocol"), config.localProtocol);
    }
    writeIfNotDefault(inbound, QStringLiteral("udpEnabled"), config.udpEnabled, true);
    writeIfNotDefault(inbound, QStringLiteral("sniffingEnabled"), config.sniffingEnabled, true);
    writeIfNotDefault(inbound, QStringLiteral("routeOnly"), config.routeOnly, false);
    writeIfNotDefault(inbound, QStringLiteral("allowLANConn"), config.allowLanConnection, false);
    writeIfNotEmpty(inbound, QStringLiteral("user"), config.inboundUser);
    writeIfNotEmpty(inbound, QStringLiteral("pass"), config.inboundPassword);
    if (!inbound.isEmpty()) {
        root.insert(QStringLiteral("inbound"), QJsonArray{inbound});
    }

    QJsonObject defaults;
    writeIfNotEmpty(defaults, QStringLiteral("speedPingTestUrl"), config.defaults().speedPingTestUrl);
    writeIfNotEmpty(defaults, QStringLiteral("ieProxyExceptions"), config.defaults().defIeProxyExceptions);
    if (!defaults.isEmpty()) {
        root.insert(QStringLiteral("defaults"), defaults);
    }

    writeIfNotDefault(root, QStringLiteral("muxEnabled"), config.muxEnabled, false);
    if (config.mux4SboxProtocol != QStringLiteral("h2mux")) {
        root.insert(QStringLiteral("mux4SboxProtocol"), config.mux4SboxProtocol);
    }
    writeIfNotDefault(
        root,
        QStringLiteral("mux4SboxMaxConnections"),
        config.mux4SboxMaxConnections > 0 ? config.mux4SboxMaxConnections : 8,
        8);
    if (config.mux4SboxPadding.has_value()) {
        root.insert(QStringLiteral("mux4SboxPadding"), config.mux4SboxPadding.value());
    }
    writeIfNotDefault(root, QStringLiteral("sysProxyType"), config.sysProxyType, 0);
    writeIfNotDefault(root, QStringLiteral("enableFragment"), config.dns().enableFragment, false);
    writeIfNotDefault(root, QStringLiteral("enableCacheFile4Sbox"), config.dns().enableCacheFile4Sbox, true);
    if (config.dns().directDns != QStringLiteral("https://dns.alidns.com/dns-query")) {
        root.insert(QStringLiteral("directDns"), config.dns().directDns);
    }
    if (config.dns().remoteDns != QStringLiteral("https://cloudflare-dns.com/dns-query")) {
        root.insert(QStringLiteral("remoteDns"), config.dns().remoteDns);
    }
    if (config.dns().bootstrapDns != QStringLiteral("223.5.5.5")) {
        root.insert(QStringLiteral("bootstrapDns"), config.dns().bootstrapDns);
    }
    writeIfNotDefault(root, QStringLiteral("fakeIp"), config.dns().fakeIp, false);
    writeIfNotDefault(root, QStringLiteral("globalFakeIp"), config.dns().globalFakeIp, true);
    writeIfNotDefault(root, QStringLiteral("serveStale"), config.dns().serveStale, false);
    writeIfNotDefault(root, QStringLiteral("parallelQuery"), config.dns().parallelQuery, false);
    writeIfNotEmpty(root, QStringLiteral("directExpectedIps"), config.dns().directExpectedIps);
    writeIfNotDefault(root, QStringLiteral("useSystemHosts"), config.dns().useSystemHosts, false);
    writeIfNotDefault(root, QStringLiteral("addCommonHosts"), config.dns().addCommonHosts, true);
    writeIfNotDefault(root, QStringLiteral("blockBindingQuery"), config.dns().blockBindingQuery, true);
    writeIfNotEmpty(root, QStringLiteral("domainStrategyForFreedom"), config.dns().domainStrategyForFreedom);
    writeIfNotEmpty(root, QStringLiteral("domainStrategyForProxy"), config.dns().domainStrategyForProxy);
    writeIfNotEmpty(root, QStringLiteral("dnsHosts"), config.dns().dnsHosts);
    writeIfNotDefault(root, QStringLiteral("defaultAllowInsecure"), config.dns().defaultAllowInsecure, false);
    writeIfNotEmpty(root, QStringLiteral("defaultFingerprint"), config.dns().defaultFingerprint);
    writeIfNotEmpty(root, QStringLiteral("defaultUserAgent"), config.dns().defaultUserAgent);
    writeIfNotEmpty(root, QStringLiteral("domainStrategy"), config.dns().domainStrategy);
    writeIfNotEmpty(root, QStringLiteral("domainStrategyForSingbox"), config.dns().domainStrategy4Singbox);
    writeIfNotEmpty(root, QStringLiteral("domainMatcher"), config.dns().domainMatcher);
    writeIfNotDefault(root, QStringLiteral("ignoreGeoUpdateCore"), config.ignoreGeoUpdateCore, false);
    writeIfNotEmpty(root, QStringLiteral("systemProxyAdvancedProtocol"), config.systemProxyAdvancedProtocol);
    writeIfNotDefault(root, QStringLiteral("checkPreReleaseUpdate"), config.checkPreReleaseUpdate, false);
}

} // namespace JsonConfigRootSerialization

