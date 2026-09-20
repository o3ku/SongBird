#include "runtime/SingBoxDnsConfigSupport.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QUrl>

#include "common/RoutingValuePattern.h"
#include "runtime/DnsAddressParser.h"
#include "runtime/DnsHosts.h"
#include "runtime/SingBoxFakeIpFilter.h"

namespace {

QJsonArray stringArray(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : values) {
        array.append(value);
    }

    return array;
}

void appendJsonArrayValue(QJsonObject& object, const QString& key, const QString& value)
{
    QJsonArray array = object.value(key).toArray();
    array.append(value);
    object.insert(key, array);
}

} // namespace

namespace SingBoxDnsConfigSupport {

QString directDnsTag()
{
    return QStringLiteral("direct_dns");
}

QString remoteDnsTag()
{
    return QStringLiteral("remote_dns");
}

QString localDnsTag()
{
    return QStringLiteral("local_local");
}

QString hostsDnsTag()
{
    return QStringLiteral("hosts_dns");
}

QString fakeDnsTag()
{
    return QStringLiteral("fake_dns");
}

QJsonObject parseDnsAddress(const QString& value)
{
    const QStringList addresses = DnsAddressParser::splitDnsAddresses(value);
    if (addresses.isEmpty()) {
        return {};
    }

    const QString address = addresses.constFirst();
    if (address.compare(QStringLiteral("local"), Qt::CaseInsensitive) == 0
        || address.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0) {
        QJsonObject server;
        server.insert(QStringLiteral("type"), QStringLiteral("local"));
        return server;
    }

    const QUrl url(address);
    const QString scheme = url.scheme().trimmed();
    if (scheme.compare(QStringLiteral("dhcp"), Qt::CaseInsensitive) == 0) {
        QJsonObject server;
        server.insert(QStringLiteral("type"), QStringLiteral("dhcp"));
        const QString host = url.host();
        if (!host.isEmpty() && host.compare(QStringLiteral("auto"), Qt::CaseInsensitive) != 0) {
            server.insert(QStringLiteral("server"), host);
        }
        return server;
    }

    QJsonObject server;
    if (scheme.isEmpty()) {
        const auto [host, port] = DnsAddressParser::parseAuthority(address);
        server.insert(QStringLiteral("type"), QStringLiteral("udp"));
        server.insert(QStringLiteral("server"), host.isEmpty() ? address : host);
        if (port > 0) {
            server.insert(QStringLiteral("server_port"), port);
        }
        return server;
    }

    QString type = scheme;
    if (type.endsWith(QStringLiteral("+local"), Qt::CaseInsensitive)) {
        type.chop(QStringLiteral("+local").size());
    }
    type = type.toLower();

    QString host = url.host(QUrl::FullyDecoded);
    int port = url.port();
    if (host.isEmpty()) {
        QString remainder = address.section(QStringLiteral("://"), 1);
        remainder = remainder.section(QChar('/'), 0, 0);
        const auto parsed = DnsAddressParser::parseAuthority(remainder);
        host = parsed.first;
        port = parsed.second;
    }

    server.insert(QStringLiteral("type"), type);
    server.insert(QStringLiteral("server"), host);
    if (port > 0) {
        server.insert(QStringLiteral("server_port"), port);
    }

    QString path = url.path();
    if (!url.query().isEmpty()) {
        path.append(QChar('?'));
        path.append(url.query(QUrl::FullyDecoded));
    }
    if ((type == QStringLiteral("https") || type == QStringLiteral("h3"))
        && !path.isEmpty()
        && path != QStringLiteral("/")) {
        server.insert(QStringLiteral("path"), path);
    }

    return server;
}

bool isCustomDnsObjectText(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);
    return parseError.error == QJsonParseError::NoError
        && document.isObject()
        && document.object().contains(QStringLiteral("servers"));
}

QString mapDomainStrategy(const QString& strategy)
{
    if (strategy.startsWith(QStringLiteral("UseIPv4"), Qt::CaseInsensitive)) {
        return QStringLiteral("prefer_ipv4");
    }
    if (strategy.startsWith(QStringLiteral("UseIPv6"), Qt::CaseInsensitive)) {
        return QStringLiteral("prefer_ipv6");
    }
    if (strategy.startsWith(QStringLiteral("ForceIPv4"), Qt::CaseInsensitive)) {
        return QStringLiteral("ipv4_only");
    }
    if (strategy.startsWith(QStringLiteral("ForceIPv6"), Qt::CaseInsensitive)) {
        return QStringLiteral("ipv6_only");
    }
    return {};
}

bool usesDirectDnsAsFinalServer(const RoutingItem* routing)
{
    if (routing == nullptr) {
        return false;
    }

    for (auto it = routing->rules.crbegin(); it != routing->rules.crend(); ++it) {
        if (!it->enabled) {
            continue;
        }

        if (it->outboundTag.compare(QStringLiteral("direct"), Qt::CaseInsensitive) != 0) {
            return false;
        }

        const bool noDomain = it->domain.isEmpty();
        const bool noProcess = it->process.isEmpty();
        const bool anyIp = it->ip.isEmpty() || it->ip.contains(QStringLiteral("0.0.0.0/0"));
        const bool anyPort = it->port.trimmed().isEmpty() || it->port.trimmed() == QStringLiteral("0-65535");
        const QString network = it->network.trimmed();
        const bool anyNetwork = network.isEmpty() || network.compare(QStringLiteral("tcp,udp"), Qt::CaseInsensitive) == 0;
        return noDomain && noProcess && anyIp && anyPort && anyNetwork;
    }

    return false;
}

bool appendDomainField(QJsonObject& rule, const QString& value, bool plainAsDomain)
{
    const QString domain = value.trimmed();
    if (domain.isEmpty() || domain.startsWith(QChar('#'))) {
        return false;
    }

    // The prefix vocabulary lives in one place, so this and the routing mappers cannot drift
    // apart again. `plainAsDomain` is the one axis where a DNS rule genuinely differs from a
    // routing rule: a bare value here is a hostname read from a hosts file, so it matches that
    // exact host, whereas the same bare value in a routing rule is a keyword match.
    const RoutingValuePattern::DomainValue parsed = RoutingValuePattern::parseDomain(domain);
    if (!parsed.recognised || parsed.value.isEmpty()) {
        // Either a prefix no core defines, or a known prefix with nothing behind it. Both used
        // to fall through to the bare branch and emit the literal text as a domain, which could
        // never match a real query and was never reported to the user.
        return false;
    }

    switch (parsed.kind) {
    case RoutingValuePattern::DomainKind::Geosite:
        appendJsonArrayValue(rule, QStringLiteral("geosite"), parsed.value);
        return true;
    case RoutingValuePattern::DomainKind::Regex:
        appendJsonArrayValue(rule, QStringLiteral("domain_regex"), parsed.value);
        return true;
    case RoutingValuePattern::DomainKind::Suffix:
        // "domain:", and a leading '.', both mean the host and its subdomains. The leading dot
        // used to reach the bare branch below and produce an exact match instead.
        appendJsonArrayValue(rule, QStringLiteral("domain_suffix"), parsed.value);
        return true;
    case RoutingValuePattern::DomainKind::Exact:
        appendJsonArrayValue(rule, QStringLiteral("domain"), parsed.value);
        return true;
    case RoutingValuePattern::DomainKind::Keyword:
    case RoutingValuePattern::DomainKind::Dotless:
        // "dotless:" has no exact equivalent here; a keyword match is the closest the core
        // offers, and the routing settings page reports the approximation to the user.
        appendJsonArrayValue(rule, QStringLiteral("domain_keyword"), parsed.value);
        return true;
    case RoutingValuePattern::DomainKind::Ext:
        // "ext:" and "ext-domain:" name a rule set loaded from a file. Neither core reads that
        // file for a DNS rule, so the value is dropped rather than emitted as a literal host.
        return false;
    case RoutingValuePattern::DomainKind::Bare:
        break;
    }

    appendJsonArrayValue(rule, plainAsDomain ? QStringLiteral("domain") : QStringLiteral("domain_keyword"), parsed.value);
    return true;
}

QString mapRcode(int code)
{
    switch (code) {
    case 0:
        return QStringLiteral("NOERROR");
    case 1:
        return QStringLiteral("FORMERR");
    case 2:
        return QStringLiteral("SERVFAIL");
    case 3:
        return QStringLiteral("NXDOMAIN");
    case 4:
        return QStringLiteral("NOTIMP");
    case 5:
        return QStringLiteral("REFUSED");
    default:
        return QStringLiteral("NOERROR");
    }
}

QJsonObject fakeIpFilterRule()
{
    return SingBoxFakeIpFilter::rule();
}

QJsonObject predefinedHosts(const Config& config, const QMap<QString, QStringList>& hostsMap)
{
    const QMap<QString, QString> systemHostsMap = config.dns().useSystemHosts ? DnsHosts::loadSystem() : QMap<QString, QString>{};
    QJsonObject predefinedHosts;
    if (config.dns().addCommonHosts) {
        const QMap<QString, QStringList>& commonHosts = DnsHosts::predefined();
        for (auto it = commonHosts.constBegin(); it != commonHosts.constEnd(); ++it) {
            predefinedHosts.insert(it.key(), stringArray(it.value()));
        }
    }
    for (auto it = systemHostsMap.constBegin(); it != systemHostsMap.constEnd(); ++it) {
        if (predefinedHosts.contains(it.key())) {
            continue;
        }
        predefinedHosts.insert(it.key(), QJsonArray{it.value()});
    }
    for (auto it = hostsMap.constBegin(); it != hostsMap.constEnd(); ++it) {
        QJsonObject domainRule;
        if (!appendDomainField(domainRule, it.key(), true)) {
            continue;
        }

        const QJsonArray exactDomains = domainRule.value(QStringLiteral("domain")).toArray();
        if (exactDomains.size() != 1) {
            continue;
        }

        QJsonArray answers;
        for (const QString& hostValue : it.value()) {
            if (DnsHosts::isIpAddress(hostValue)) {
                answers.append(hostValue);
            }
        }
        if (!answers.isEmpty()) {
            predefinedHosts.insert(exactDomains.at(0).toString(), answers);
        }
    }

    return predefinedHosts;
}

QJsonObject hostsDnsServer(const QJsonObject& predefinedHosts)
{
    if (predefinedHosts.isEmpty()) {
        return {};
    }

    QJsonObject hostsDnsServer;
    hostsDnsServer.insert(QStringLiteral("tag"), hostsDnsTag());
    hostsDnsServer.insert(QStringLiteral("type"), QStringLiteral("hosts"));
    hostsDnsServer.insert(QStringLiteral("predefined"), predefinedHosts);
    return hostsDnsServer;
}

void applyHostsResolver(QJsonObject& dnsServer, const QJsonObject& predefinedHosts)
{
    const QString serverName = dnsServer.value(QStringLiteral("server")).toString().trimmed();
    if (!serverName.isEmpty() && predefinedHosts.contains(serverName)) {
        dnsServer.insert(QStringLiteral("domain_resolver"), hostsDnsTag());
    }
}

QString finalDnsServerTag(
    bool hasRemoteDnsServer,
    bool hasDirectDnsServer,
    bool hasPredefinedHosts,
    bool useDirectFinal)
{
    if (hasRemoteDnsServer && !useDirectFinal) {
        return remoteDnsTag();
    }
    if (hasDirectDnsServer) {
        return directDnsTag();
    }
    if (hasPredefinedHosts) {
        return hostsDnsTag();
    }

    return {};
}

} // namespace SingBoxDnsConfigSupport
