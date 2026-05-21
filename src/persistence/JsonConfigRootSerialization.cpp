#include "persistence/JsonConfigRootSerialization.h"

#include <QJsonArray>

namespace {

constexpr int kSongBirdJsonConfigSchemaVersion = 7;

void insertIfNotDefault(QJsonObject& object, const QString& key, bool value, bool defaultValue)
{
    if (value != defaultValue) {
        object.insert(key, value);
    }
}

void insertIfNotDefault(QJsonObject& object, const QString& key, int value, int defaultValue)
{
    if (value != defaultValue) {
        object.insert(key, value);
    }
}

void insertIfNotEmpty(QJsonObject& object, const QString& key, const QString& value)
{
    if (!value.trimmed().isEmpty()) {
        object.insert(key, value);
    }
}

} // namespace

namespace JsonConfigRootSerialization {

void read(const QJsonObject& root, RootConfigState& config)
{
    config.logEnabled = root.value(QStringLiteral("logEnabled")).toBool(false);
    config.logLevel = root.value(QStringLiteral("logLevel")).toString();
    config.currentIndexId = root.value(QStringLiteral("indexId")).toString();

    const QJsonArray inbounds = root.value(QStringLiteral("inbound")).toArray();
    if (!inbounds.isEmpty() && inbounds.at(0).isObject()) {
        const QJsonObject inbound = inbounds.at(0).toObject();
        config.localPort = inbound.value(QStringLiteral("localPort")).toInt(10808);
        config.localProtocol = inbound.value(QStringLiteral("protocol")).toString(QStringLiteral("socks"));
        config.udpEnabled = inbound.value(QStringLiteral("udpEnabled")).toBool(true);
        config.sniffingEnabled = inbound.value(QStringLiteral("sniffingEnabled")).toBool(true);
        config.routeOnly = inbound.value(QStringLiteral("routeOnly")).toBool(false);
        config.allowLanConnection = inbound.value(QStringLiteral("allowLANConn")).toBool(false);
        config.inboundUser = inbound.value(QStringLiteral("user")).toString();
        config.inboundPassword = inbound.value(QStringLiteral("pass")).toString();
    }

    const QJsonObject defaults = root.value(QStringLiteral("defaults")).toObject();
    config.defaults().speedPingTestUrl = defaults.value(QStringLiteral("speedPingTestUrl")).toString();
    config.defaults().defIeProxyExceptions = defaults.value(QStringLiteral("ieProxyExceptions")).toString();

    config.muxEnabled = root.value(QStringLiteral("muxEnabled")).toBool(false);
    config.mux4SboxProtocol = root.contains(QStringLiteral("mux4SboxProtocol"))
        ? root.value(QStringLiteral("mux4SboxProtocol")).toString()
        : QStringLiteral("h2mux");
    config.mux4SboxMaxConnections = root.contains(QStringLiteral("mux4SboxMaxConnections"))
        ? root.value(QStringLiteral("mux4SboxMaxConnections")).toInt(8)
        : 8;
    if (config.mux4SboxMaxConnections <= 0) {
        config.mux4SboxMaxConnections = 8;
    }
    if (root.contains(QStringLiteral("mux4SboxPadding")) && root.value(QStringLiteral("mux4SboxPadding")).isBool()) {
        config.mux4SboxPadding = root.value(QStringLiteral("mux4SboxPadding")).toBool();
    } else {
        config.mux4SboxPadding.reset();
    }

    config.sysProxyType = root.value(QStringLiteral("sysProxyType")).toInt(0);

    config.dns().enableFragment = root.value(QStringLiteral("enableFragment")).toBool(false);
    config.dns().enableCacheFile4Sbox = root.contains(QStringLiteral("enableCacheFile4Sbox"))
        ? root.value(QStringLiteral("enableCacheFile4Sbox")).toBool(true)
        : true;
    config.dns().defaultFingerprint = root.value(QStringLiteral("defaultFingerprint")).toString();
    config.dns().defaultUserAgent = root.value(QStringLiteral("defaultUserAgent")).toString();
    if (root.contains(QStringLiteral("directDns"))) {
        config.dns().directDns = root.value(QStringLiteral("directDns")).toString();
    }
    if (root.contains(QStringLiteral("remoteDns"))) {
        config.dns().remoteDns = root.value(QStringLiteral("remoteDns")).toString();
    }
    if (root.contains(QStringLiteral("bootstrapDns"))) {
        config.dns().bootstrapDns = root.value(QStringLiteral("bootstrapDns")).toString();
    }
    if (root.contains(QStringLiteral("fakeIp"))) {
        config.dns().fakeIp = root.value(QStringLiteral("fakeIp")).toBool(false);
    }
    config.dns().globalFakeIp = root.contains(QStringLiteral("globalFakeIp"))
        ? root.value(QStringLiteral("globalFakeIp")).toBool(true)
        : true;
    config.dns().serveStale = root.contains(QStringLiteral("serveStale"))
        ? root.value(QStringLiteral("serveStale")).toBool(false)
        : false;
    config.dns().parallelQuery = root.value(QStringLiteral("parallelQuery")).toBool(false);
    config.dns().directExpectedIps = root.value(QStringLiteral("directExpectedIps")).toString();
    config.dns().useSystemHosts = root.contains(QStringLiteral("useSystemHosts"))
        ? root.value(QStringLiteral("useSystemHosts")).toBool(false)
        : false;
    config.dns().addCommonHosts = root.contains(QStringLiteral("addCommonHosts"))
        ? root.value(QStringLiteral("addCommonHosts")).toBool(true)
        : true;
    config.dns().blockBindingQuery = root.contains(QStringLiteral("blockBindingQuery"))
        ? root.value(QStringLiteral("blockBindingQuery")).toBool(true)
        : true;
    config.dns().domainStrategyForFreedom = root.value(QStringLiteral("domainStrategyForFreedom")).toString();
    config.dns().domainStrategyForProxy = root.value(QStringLiteral("domainStrategyForProxy")).toString();
    config.dns().dnsHosts = root.value(QStringLiteral("dnsHosts")).toString();
    config.dns().defaultAllowInsecure = root.value(QStringLiteral("defaultAllowInsecure")).toBool(false);
    config.dns().domainStrategy = root.value(QStringLiteral("domainStrategy")).toString();
    config.dns().domainStrategy4Singbox = root.value(QStringLiteral("domainStrategyForSingbox")).toString();
    config.dns().domainMatcher = root.value(QStringLiteral("domainMatcher")).toString();
    config.ignoreGeoUpdateCore = root.value(QStringLiteral("ignoreGeoUpdateCore")).toBool(false);
    config.systemProxyAdvancedProtocol = root.value(QStringLiteral("systemProxyAdvancedProtocol")).toString();
    config.checkPreReleaseUpdate = root.value(QStringLiteral("checkPreReleaseUpdate")).toBool(false);
}

void write(QJsonObject& root, const RootConfigState& config)
{
    root.insert(QStringLiteral("schemaVersion"), kSongBirdJsonConfigSchemaVersion);
    insertIfNotDefault(root, QStringLiteral("logEnabled"), config.logEnabled, false);
    insertIfNotEmpty(root, QStringLiteral("logLevel"), config.logLevel);
    insertIfNotEmpty(root, QStringLiteral("indexId"), config.currentIndexId);

    QJsonObject inbound;
    insertIfNotDefault(inbound, QStringLiteral("localPort"), config.localPort, 10808);
    if (config.localProtocol != QStringLiteral("socks")) {
        inbound.insert(QStringLiteral("protocol"), config.localProtocol);
    }
    insertIfNotDefault(inbound, QStringLiteral("udpEnabled"), config.udpEnabled, true);
    insertIfNotDefault(inbound, QStringLiteral("sniffingEnabled"), config.sniffingEnabled, true);
    insertIfNotDefault(inbound, QStringLiteral("routeOnly"), config.routeOnly, false);
    insertIfNotDefault(inbound, QStringLiteral("allowLANConn"), config.allowLanConnection, false);
    insertIfNotEmpty(inbound, QStringLiteral("user"), config.inboundUser);
    insertIfNotEmpty(inbound, QStringLiteral("pass"), config.inboundPassword);
    if (!inbound.isEmpty()) {
        root.insert(QStringLiteral("inbound"), QJsonArray{inbound});
    }

    QJsonObject defaults;
    insertIfNotEmpty(defaults, QStringLiteral("speedPingTestUrl"), config.defaults().speedPingTestUrl);
    insertIfNotEmpty(defaults, QStringLiteral("ieProxyExceptions"), config.defaults().defIeProxyExceptions);
    if (!defaults.isEmpty()) {
        root.insert(QStringLiteral("defaults"), defaults);
    }

    insertIfNotDefault(root, QStringLiteral("muxEnabled"), config.muxEnabled, false);
    if (config.mux4SboxProtocol != QStringLiteral("h2mux")) {
        root.insert(QStringLiteral("mux4SboxProtocol"), config.mux4SboxProtocol);
    }
    insertIfNotDefault(
        root,
        QStringLiteral("mux4SboxMaxConnections"),
        config.mux4SboxMaxConnections > 0 ? config.mux4SboxMaxConnections : 8,
        8);
    if (config.mux4SboxPadding.has_value()) {
        root.insert(QStringLiteral("mux4SboxPadding"), config.mux4SboxPadding.value());
    }
    insertIfNotDefault(root, QStringLiteral("sysProxyType"), config.sysProxyType, 0);
    insertIfNotDefault(root, QStringLiteral("enableFragment"), config.dns().enableFragment, false);
    insertIfNotDefault(root, QStringLiteral("enableCacheFile4Sbox"), config.dns().enableCacheFile4Sbox, true);
    if (config.dns().directDns != QStringLiteral("https://dns.alidns.com/dns-query")) {
        root.insert(QStringLiteral("directDns"), config.dns().directDns);
    }
    if (config.dns().remoteDns != QStringLiteral("https://cloudflare-dns.com/dns-query")) {
        root.insert(QStringLiteral("remoteDns"), config.dns().remoteDns);
    }
    if (config.dns().bootstrapDns != QStringLiteral("223.5.5.5")) {
        root.insert(QStringLiteral("bootstrapDns"), config.dns().bootstrapDns);
    }
    insertIfNotDefault(root, QStringLiteral("fakeIp"), config.dns().fakeIp, false);
    insertIfNotDefault(root, QStringLiteral("globalFakeIp"), config.dns().globalFakeIp, true);
    insertIfNotDefault(root, QStringLiteral("serveStale"), config.dns().serveStale, false);
    insertIfNotDefault(root, QStringLiteral("parallelQuery"), config.dns().parallelQuery, false);
    insertIfNotEmpty(root, QStringLiteral("directExpectedIps"), config.dns().directExpectedIps);
    insertIfNotDefault(root, QStringLiteral("useSystemHosts"), config.dns().useSystemHosts, false);
    insertIfNotDefault(root, QStringLiteral("addCommonHosts"), config.dns().addCommonHosts, true);
    insertIfNotDefault(root, QStringLiteral("blockBindingQuery"), config.dns().blockBindingQuery, true);
    insertIfNotEmpty(root, QStringLiteral("domainStrategyForFreedom"), config.dns().domainStrategyForFreedom);
    insertIfNotEmpty(root, QStringLiteral("domainStrategyForProxy"), config.dns().domainStrategyForProxy);
    insertIfNotEmpty(root, QStringLiteral("dnsHosts"), config.dns().dnsHosts);
    insertIfNotDefault(root, QStringLiteral("defaultAllowInsecure"), config.dns().defaultAllowInsecure, false);
    insertIfNotEmpty(root, QStringLiteral("defaultFingerprint"), config.dns().defaultFingerprint);
    insertIfNotEmpty(root, QStringLiteral("defaultUserAgent"), config.dns().defaultUserAgent);
    insertIfNotEmpty(root, QStringLiteral("domainStrategy"), config.dns().domainStrategy);
    insertIfNotEmpty(root, QStringLiteral("domainStrategyForSingbox"), config.dns().domainStrategy4Singbox);
    insertIfNotEmpty(root, QStringLiteral("domainMatcher"), config.dns().domainMatcher);
    insertIfNotDefault(root, QStringLiteral("ignoreGeoUpdateCore"), config.ignoreGeoUpdateCore, false);
    insertIfNotEmpty(root, QStringLiteral("systemProxyAdvancedProtocol"), config.systemProxyAdvancedProtocol);
    insertIfNotDefault(root, QStringLiteral("checkPreReleaseUpdate"), config.checkPreReleaseUpdate, false);
}

} // namespace JsonConfigRootSerialization

