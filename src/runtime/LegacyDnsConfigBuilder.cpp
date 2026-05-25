#include "runtime/LegacyDnsConfigBuilder.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

#include "runtime/DnsAddressParser.h"
#include "runtime/DnsHosts.h"
#include "runtime/RoutingRuleJsonMapper.h"
#include "runtime/SingBoxDnsConfigSupport.h"

namespace {

namespace DnsSupport = SingBoxDnsConfigSupport;

const QString kLegacyDnsTag = QStringLiteral("dns-module");
const QString kLegacyDirectDnsTagPrefix = QStringLiteral("direct-dns");

QJsonObject createLegacyDnsServer(
    const QString& dnsAddress,
    const QStringList& domains = {},
    bool skipFallback = true,
    const QStringList& expectedIps = {})
{
    const QStringList addresses = DnsAddressParser::splitDnsAddresses(dnsAddress);
    if (addresses.isEmpty()) {
        return {};
    }

    const QString address = addresses.constFirst();
    const QUrl url(address);
    const QString scheme = url.scheme().trimmed();

    QJsonObject server;
    if (scheme.isEmpty() || scheme.startsWith(QStringLiteral("udp"), Qt::CaseInsensitive)) {
        const auto [host, port] = DnsAddressParser::parseAuthority(scheme.isEmpty() ? address : address.section(QStringLiteral("://"), 1));
        server.insert(QStringLiteral("address"), host.isEmpty() ? address : host);
        if (port > 0) {
            server.insert(QStringLiteral("port"), port);
        }
    } else if (scheme.startsWith(QStringLiteral("tcp"), Qt::CaseInsensitive)) {
        const QString host = url.host(QUrl::FullyDecoded);
        server.insert(QStringLiteral("address"), QStringLiteral("tcp://%1").arg(host));
        if (url.port() > 0) {
            server.insert(QStringLiteral("port"), url.port());
        }
    } else {
        server.insert(QStringLiteral("address"), address);
    }

    if (!domains.isEmpty()) {
        QJsonArray domainArray;
        for (const QString& domain : domains) {
            domainArray.append(domain);
        }
        server.insert(QStringLiteral("domains"), domainArray);
    }
    if (!expectedIps.isEmpty()) {
        QJsonArray expectedIpArray;
        for (const QString& expectedIp : expectedIps) {
            expectedIpArray.append(expectedIp);
        }
        server.insert(QStringLiteral("expectIPs"), expectedIpArray);
    }
    if (skipFallback) {
        server.insert(QStringLiteral("skipFallback"), true);
    }

    return server;
}

} // namespace

namespace LegacyDnsConfigBuilder {

QJsonObject build(const Config& config, const RoutingItem* selectedRouting)
{
    if (DnsSupport::isCustomDnsObjectText(config.dns().remoteDns)) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(config.dns().remoteDns.trimmed().toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            return document.object();
        }
    }

    const QStringList remoteDnsAddresses = DnsAddressParser::splitDnsAddresses(config.dns().remoteDns);
    const QStringList directDnsAddresses = DnsAddressParser::splitDnsAddresses(config.dns().directDns);
    const QStringList bootstrapDnsAddresses = DnsAddressParser::splitDnsAddresses(config.dns().bootstrapDns);
    const QMap<QString, QStringList> hostsMap = DnsHosts::parseConfigured(config.dns().dnsHosts);
    const QMap<QString, QString> systemHostsMap = config.dns().useSystemHosts ? DnsHosts::loadSystem() : QMap<QString, QString>{};
    const bool useDirectFinal = DnsSupport::usesDirectDnsAsFinalServer(selectedRouting);

    if (remoteDnsAddresses.isEmpty()
        && directDnsAddresses.isEmpty()
        && bootstrapDnsAddresses.isEmpty()
        && hostsMap.isEmpty()
        && systemHostsMap.isEmpty()) {
        return {};
    }

    QStringList directDomains;
    QStringList directGeositeDomains;
    QStringList proxyDomains;
    QStringList proxyGeositeDomains;
    QStringList expectedIpDomains;
    QStringList expectedIps;
    QSet<QString> expectedIpRegionNames;
    for (const QString& item : config.dns().directExpectedIps.split(QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts)) {
        const QString trimmed = item.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        expectedIps.append(trimmed);
        if (trimmed.startsWith(QStringLiteral("geoip:"), Qt::CaseInsensitive)) {
            const QString region = trimmed.mid(QStringLiteral("geoip:").size()).trimmed();
            if (region.isEmpty()) {
                continue;
            }
            expectedIpRegionNames.insert(QStringLiteral("geosite:%1").arg(region));
            expectedIpRegionNames.insert(QStringLiteral("geosite:geolocation-%1").arg(region));
            expectedIpRegionNames.insert(QStringLiteral("geosite:tld-%1").arg(region));
        }
    }
    if (selectedRouting != nullptr) {
        for (const RoutingRule& rule : selectedRouting->rules) {
            if (!rule.enabled) {
                continue;
            }

            const QStringList domains = RoutingRuleJsonMapper::normalizeRuleValues(rule.domain, true, true);
            if (domains.isEmpty()) {
                continue;
            }

            for (const QString& domain : domains) {
                if (rule.outboundTag.compare(QStringLiteral("direct"), Qt::CaseInsensitive) == 0) {
                    if ((domain.startsWith(QStringLiteral("geosite:"), Qt::CaseInsensitive)
                            || domain.startsWith(QStringLiteral("ext:"), Qt::CaseInsensitive))
                        && expectedIpRegionNames.contains(domain)) {
                        expectedIpDomains.append(domain);
                    } else if (domain.startsWith(QStringLiteral("geosite:"), Qt::CaseInsensitive)
                        || domain.startsWith(QStringLiteral("ext:"), Qt::CaseInsensitive)) {
                        directGeositeDomains.append(domain);
                    } else {
                        directDomains.append(domain);
                    }
                } else if (rule.outboundTag.compare(QStringLiteral("block"), Qt::CaseInsensitive) != 0) {
                    if (domain.startsWith(QStringLiteral("geosite:"), Qt::CaseInsensitive)
                        || domain.startsWith(QStringLiteral("ext:"), Qt::CaseInsensitive)) {
                        proxyGeositeDomains.append(domain);
                    } else {
                        proxyDomains.append(domain);
                    }
                }
            }
        }
    }

    QJsonArray serverArray;
    int directDnsTagIndex = 1;

    const auto addTaggedDirectServers = [&](
                                            const QStringList& addresses,
                                            const QStringList& domains,
                                            bool skipFallback,
                                            const QStringList& serverExpectedIps = {}) {
        for (const QString& address : addresses) {
            QJsonObject server = createLegacyDnsServer(address, domains, skipFallback, serverExpectedIps);
            if (server.isEmpty()) {
                continue;
            }
            server.insert(QStringLiteral("tag"), QStringLiteral("%1-%2").arg(kLegacyDirectDnsTagPrefix).arg(directDnsTagIndex++));
            serverArray.append(server);
        }
    };

    const auto addServers = [&](
                                 const QStringList& addresses,
                                 const QStringList& domains,
                                 bool skipFallback,
                                 const QStringList& serverExpectedIps = {}) {
        for (const QString& address : addresses) {
            QJsonObject server = createLegacyDnsServer(address, domains, skipFallback, serverExpectedIps);
            if (!server.isEmpty()) {
                serverArray.append(server);
            }
        }
    };

    if (!proxyDomains.isEmpty()) {
        addServers(remoteDnsAddresses, proxyDomains, true);
    }
    if (!proxyGeositeDomains.isEmpty()) {
        addServers(remoteDnsAddresses, proxyGeositeDomains, true);
    }
    if (!directDomains.isEmpty()) {
        addTaggedDirectServers(directDnsAddresses, directDomains, true);
    }
    if (!directGeositeDomains.isEmpty()) {
        addTaggedDirectServers(directDnsAddresses, directGeositeDomains, true);
    }
    if (!expectedIpDomains.isEmpty()) {
        addTaggedDirectServers(directDnsAddresses, expectedIpDomains, true, expectedIps);
    }

    QStringList dnsServerDomains;
    for (const QString& address : directDnsAddresses) {
        const QString host = DnsAddressParser::extractHost(address);
        if (!host.isEmpty() && host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) != 0 && !DnsHosts::isIpAddress(host)) {
            dnsServerDomains.append(QStringLiteral("full:%1").arg(host));
        }
    }
    for (const QString& address : remoteDnsAddresses) {
        const QString host = DnsAddressParser::extractHost(address);
        if (!host.isEmpty() && host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) != 0 && !DnsHosts::isIpAddress(host)) {
            dnsServerDomains.append(QStringLiteral("full:%1").arg(host));
        }
    }
    dnsServerDomains.removeDuplicates();
    if (!dnsServerDomains.isEmpty()) {
        addServers(bootstrapDnsAddresses, dnsServerDomains, true);
    }

    if (useDirectFinal) {
        addTaggedDirectServers(directDnsAddresses, {}, false);
    } else {
        addServers(remoteDnsAddresses, {}, false);
    }

    QJsonObject dns;
    dns.insert(QStringLiteral("tag"), kLegacyDnsTag);
    dns.insert(QStringLiteral("servers"), serverArray);
    if (config.dns().serveStale) {
        dns.insert(QStringLiteral("serveStale"), true);
    }
    if (config.dns().parallelQuery) {
        dns.insert(QStringLiteral("enableParallelQuery"), true);
    }
    if (config.dns().addCommonHosts || config.dns().useSystemHosts || !hostsMap.isEmpty()) {
        QJsonObject hostsObject;
        if (config.dns().addCommonHosts) {
            const QMap<QString, QStringList>& predefinedHosts = DnsHosts::predefined();
            for (auto it = predefinedHosts.constBegin(); it != predefinedHosts.constEnd(); ++it) {
                QJsonArray values;
                for (const QString& value : it.value()) {
                    values.append(value);
                }
                hostsObject.insert(it.key(), values);
            }
        }
        for (auto it = systemHostsMap.constBegin(); it != systemHostsMap.constEnd(); ++it) {
            if (hostsObject.contains(it.key())) {
                continue;
            }
            hostsObject.insert(it.key(), QJsonArray{it.value()});
        }
        for (auto it = hostsMap.constBegin(); it != hostsMap.constEnd(); ++it) {
            QJsonArray values;
            for (const QString& value : it.value()) {
                values.append(value);
            }
            hostsObject.insert(it.key(), values);
        }
        dns.insert(QStringLiteral("hosts"), hostsObject);
    }
    return dns;
}

} // namespace LegacyDnsConfigBuilder
