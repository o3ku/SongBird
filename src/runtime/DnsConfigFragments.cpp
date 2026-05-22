#include "runtime/DnsConfigFragments.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

#include "runtime/RoutingRuleJsonMapper.h"

namespace {

const QString kSingBoxDirectDnsTag = QStringLiteral("direct_dns");
const QString kSingBoxRemoteDnsTag = QStringLiteral("remote_dns");
const QString kSingBoxLocalDnsTag = QStringLiteral("local_local");
const QString kSingBoxHostsDnsTag = QStringLiteral("hosts_dns");
const QString kSingBoxFakeDnsTag = QStringLiteral("fake_dns");
const QString kLegacyDnsTag = QStringLiteral("dns-module");
const QString kLegacyDirectDnsTagPrefix = QStringLiteral("direct-dns");
const char kSingBoxFakeIpFilterJson[] = R"json(
{
  "domain": [
    "amobile.music.tc.qq.com",
    "api-jooxtt.sanook.com",
    "api.joox.com",
    "aqqmusic.tc.qq.com",
    "dl.stream.qqmusic.qq.com",
    "ff.dorado.sdo.com",
    "heartbeat.belkin.com",
    "isure.stream.qqmusic.qq.com",
    "joox.com",
    "lens.l.google.com",
    "localhost.ptlogin2.qq.com",
    "localhost.sec.qq.com",
    "mesu.apple.com",
    "mobileoc.music.tc.qq.com",
    "music.taihe.com",
    "musicapi.taihe.com",
    "na.b.g-tun.com",
    "proxy.golang.org",
    "ps.res.netease.com",
    "shark007.net",
    "songsearch.kugou.com",
    "static.adtidy.org",
    "streamoc.music.tc.qq.com",
    "swcdn.apple.com",
    "swdist.apple.com",
    "swdownload.apple.com",
    "swquery.apple.com",
    "swscan.apple.com",
    "turn.cloudflare.com",
    "trackercdn.kugou.com",
    "xnotify.xboxlive.com"
  ],
  "domain_keyword": [
    "ntp",
    "stun",
    "time"
  ],
  "domain_regex": [
    "^[^.]+$",
    "^[^.]+\\.[^.]+\\.xboxlive\\.com$",
    "^localhost\\.[^.]+\\.weixin\\.qq\\.com$",
    "^mijia\\scloud$",
    "^xbox\\.[^.]+\\.microsoft\\.com$",
    "^xbox\\.[^.]+\\.[^.]+\\.microsoft\\.com$"
  ],
  "domain_suffix": [
    "126.net",
    "3gppnetwork.org",
    "battle.net",
    "battlenet.com.cn",
    "cdn.nintendo.net",
    "cmbchina.com",
    "cmbimg.com",
    "ff14.sdo.com",
    "ffxiv.com",
    "finalfantasyxiv.com",
    "gcloudcs.com",
    "home.arpa",
    "invalid",
    "kuwo.cn",
    "lan",
    "linksys.com",
    "linksyssmartwifi.com",
    "local",
    "localdomain",
    "localhost",
    "market.xiaomi.com",
    "mcdn.bilivideo.cn",
    "media.dssott.com",
    "msftconnecttest.com",
    "msftncsi.com",
    "music.163.com",
    "music.migu.cn",
    "n0808.com",
    "nflxvideo.net",
    "oray.com",
    "orayimg.com",
    "router.asus.com",
    "sandai.net",
    "square-enix.com",
    "srv.nintendo.net",
    "steamcontent.com",
    "uu.163.com",
    "wargaming.net",
    "wggames.cn",
    "wotgame.cn",
    "wowsgame.cn",
    "xiami.com",
    "y.qq.com"
  ]
}
)json";
const QMap<QString, QStringList> kPredefinedHosts{
    {QStringLiteral("dns.google"), QStringList{
                                       QStringLiteral("8.8.8.8"),
                                       QStringLiteral("8.8.4.4"),
                                       QStringLiteral("2001:4860:4860::8888"),
                                       QStringLiteral("2001:4860:4860::8844")}},
    {QStringLiteral("dns.alidns.com"), QStringList{
                                           QStringLiteral("223.5.5.5"),
                                           QStringLiteral("223.6.6.6"),
                                           QStringLiteral("2400:3200::1"),
                                           QStringLiteral("2400:3200:baba::1")}},
    {QStringLiteral("one.one.one.one"), QStringList{
                                            QStringLiteral("1.1.1.1"),
                                            QStringLiteral("1.0.0.1"),
                                            QStringLiteral("2606:4700:4700::1111"),
                                            QStringLiteral("2606:4700:4700::1001")}},
    {QStringLiteral("1dot1dot1dot1.cloudflare-dns.com"), QStringList{
                                                              QStringLiteral("1.1.1.1"),
                                                              QStringLiteral("1.0.0.1"),
                                                              QStringLiteral("2606:4700:4700::1111"),
                                                              QStringLiteral("2606:4700:4700::1001")}},
    {QStringLiteral("cloudflare-dns.com"), QStringList{
                                               QStringLiteral("104.16.249.249"),
                                               QStringLiteral("104.16.248.249"),
                                               QStringLiteral("2606:4700::6810:f8f9"),
                                               QStringLiteral("2606:4700::6810:f9f9")}},
    {QStringLiteral("dns.cloudflare.com"), QStringList{
                                               QStringLiteral("104.16.132.229"),
                                               QStringLiteral("104.16.133.229"),
                                               QStringLiteral("2606:4700::6810:84e5"),
                                               QStringLiteral("2606:4700::6810:85e5")}},
    {QStringLiteral("dot.pub"), QStringList{
                                    QStringLiteral("1.12.12.12"),
                                    QStringLiteral("120.53.53.53")}},
    {QStringLiteral("doh.pub"), QStringList{
                                    QStringLiteral("1.12.12.12"),
                                    QStringLiteral("120.53.53.53")}},
    {QStringLiteral("dns.quad9.net"), QStringList{
                                          QStringLiteral("9.9.9.9"),
                                          QStringLiteral("149.112.112.112"),
                                          QStringLiteral("2620:fe::fe"),
                                          QStringLiteral("2620:fe::9")}},
    {QStringLiteral("dns.yandex.net"), QStringList{
                                           QStringLiteral("77.88.8.8"),
                                           QStringLiteral("77.88.8.1"),
                                           QStringLiteral("2a02:6b8::feed:0ff"),
                                           QStringLiteral("2a02:6b8:0:1::feed:0ff")}},
    {QStringLiteral("dns.sb"), QStringList{
                                   QStringLiteral("185.222.222.222"),
                                   QStringLiteral("2a09::")}},
    {QStringLiteral("dns.umbrella.com"), QStringList{
                                             QStringLiteral("208.67.220.220"),
                                             QStringLiteral("208.67.222.222"),
                                             QStringLiteral("2620:119:35::35"),
                                             QStringLiteral("2620:119:53::53")}},
    {QStringLiteral("dns.sse.cisco.com"), QStringList{
                                              QStringLiteral("208.67.220.220"),
                                              QStringLiteral("208.67.222.222"),
                                              QStringLiteral("2620:119:35::35"),
                                              QStringLiteral("2620:119:53::53")}},
    {QStringLiteral("engage.cloudflareclient.com"), QStringList{
                                                        QStringLiteral("162.159.192.1")}},
};

QPair<QString, int> parseAuthority(QString authority)
{
    authority = authority.trimmed();
    if (authority.isEmpty()) {
        return {};
    }

    if (authority.startsWith(QChar('['))) {
        const int bracketIndex = authority.indexOf(QChar(']'));
        if (bracketIndex > 0) {
            const QString host = authority.left(bracketIndex + 1);
            const QString remainder = authority.mid(bracketIndex + 1);
            if (remainder.startsWith(QChar(':'))) {
                bool ok = false;
                const int port = remainder.mid(1).toInt(&ok);
                return {host, ok ? port : 0};
            }
            return {host, 0};
        }
    }

    const int lastColon = authority.lastIndexOf(QChar(':'));
    if (lastColon > 0 && authority.indexOf(QChar(':')) == lastColon) {
        bool ok = false;
        const int port = authority.mid(lastColon + 1).toInt(&ok);
        if (ok) {
            return {authority.left(lastColon), port};
        }
    }

    return {authority, 0};
}

void appendJsonArrayValue(QJsonObject& object, const QString& key, const QString& value)
{
    QJsonArray array = object.value(key).toArray();
    array.append(value);
    object.insert(key, array);
}

QList<RoutingRule> effectiveRoutingRules(const Config& config, const RoutingItem* selectedRouting)
{
    QList<RoutingRule> rules;
    const QStringList customOrder{
        QStringLiteral("block"),
        QStringLiteral("direct"),
        QStringLiteral("proxy")};

    for (const QString& outboundTag : customOrder) {
        for (const RoutingRule& rule : config.collection().routingCustomRules) {
            if (!rule.enabled || rule.outboundTag.compare(outboundTag, Qt::CaseInsensitive) != 0) {
                continue;
            }
            rules.append(rule);
        }
    }

    if (selectedRouting != nullptr) {
        for (const RoutingRule& rule : selectedRouting->rules) {
            if (rule.enabled) {
                rules.append(rule);
            }
        }
    }

    return rules;
}

QString extractDnsHost(const QString& address)
{
    const QStringList addresses = DnsConfigFragments::splitDnsAddresses(address);
    if (addresses.isEmpty()) {
        return {};
    }

    const QString first = addresses.constFirst();
    const QUrl url(first);
    if (!url.scheme().trimmed().isEmpty()) {
        return url.host(QUrl::FullyDecoded);
    }

    return parseAuthority(first).first;
}

QJsonObject createLegacyDnsServer(
    const QString& dnsAddress,
    const QStringList& domains = {},
    bool skipFallback = true,
    const QStringList& expectedIps = {})
{
    const QStringList addresses = DnsConfigFragments::splitDnsAddresses(dnsAddress);
    if (addresses.isEmpty()) {
        return {};
    }

    const QString address = addresses.constFirst();
    const QUrl url(address);
    const QString scheme = url.scheme().trimmed();

    QJsonObject server;
    if (scheme.isEmpty() || scheme.startsWith(QStringLiteral("udp"), Qt::CaseInsensitive)) {
        const auto [host, port] = parseAuthority(scheme.isEmpty() ? address : address.section(QStringLiteral("://"), 1));
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

QMap<QString, QString> parseSystemHostsText(const QString& hosts)
{
    QMap<QString, QString> result;
    const QString normalized = QString(hosts).replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(QChar('\r'), QChar('\n'));
    const QStringList lines = normalized.split(QChar('\n'), Qt::SkipEmptyParts);
    for (const QString& rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QChar('#'))) {
            continue;
        }

        const int commentIndex = line.indexOf(QChar('#'));
        if (commentIndex >= 0) {
            line = line.left(commentIndex).trimmed();
        }
        if (line.isEmpty()) {
            continue;
        }

        const QStringList parts = line.split(QRegularExpression(QStringLiteral("[ \\t]+")), Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            continue;
        }

        const QString ipAddress = parts.constFirst().trimmed();
        if (!DnsConfigFragments::isIpAddress(ipAddress)) {
            continue;
        }

        const QString domain = parts.at(1).trimmed();
        if (domain.isEmpty() || domain.size() > 255) {
            continue;
        }

        result.insert(domain, ipAddress);
    }

    return result;
}

void mergeSystemHostsFromFile(const QString& path, QMap<QString, QString>& hosts)
{
    if (path.trimmed().isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const QByteArray content = file.readAll();
    const auto readError = file.error();
    file.close();
    if (content.isEmpty() && readError != QFileDevice::NoError) {
        return;
    }

    const QMap<QString, QString> parsedHosts = parseSystemHostsText(QString::fromUtf8(content));
    for (auto it = parsedHosts.constBegin(); it != parsedHosts.constEnd(); ++it) {
        hosts.insert(it.key(), it.value());
    }
}

} // namespace

QStringList DnsConfigFragments::splitDnsAddresses(const QString& value)
{
    QString normalized = value;
    normalized.replace(QChar(';'), QChar(','));

    QStringList result;
    const QStringList parts = normalized.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
        }
    }

    return result;
}

QJsonObject DnsConfigFragments::parseSingBoxDnsAddress(const QString& value)
{
    const QStringList addresses = splitDnsAddresses(value);
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
        const auto [host, port] = parseAuthority(address);
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
        const auto parsed = parseAuthority(remainder);
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

QString DnsConfigFragments::mapDomainStrategyToSingBox(const QString& strategy)
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

bool DnsConfigFragments::isIpAddress(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    const QStringList octets = trimmed.split(QChar('.'));
    if (octets.size() == 4) {
        for (const QString& octet : octets) {
            bool ok = false;
            const int segment = octet.toInt(&ok);
            if (!ok || segment < 0 || segment > 255) {
                return false;
            }
        }
        return true;
    }

    return trimmed.contains(QChar(':'));
}

bool DnsConfigFragments::isDomainName(const QString& value)
{
    const QString trimmed = value.trimmed();
    return !trimmed.isEmpty()
        && !isIpAddress(trimmed)
        && trimmed.contains(QChar('.'))
        && !trimmed.contains(QChar(' '));
}

QString DnsConfigFragments::mapDnsRcode(int code)
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

QMap<QString, QStringList> DnsConfigFragments::parseHostsToDictionary(const QString& hosts)
{
    QMap<QString, QStringList> result;
    const QString normalized = QString(hosts).replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(QChar('\r'), QChar('\n'));
    const QStringList lines = normalized.split(QChar('\n'), Qt::SkipEmptyParts);
    for (const QString& rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QChar('#'))) {
            continue;
        }

        const int commentIndex = line.indexOf(QChar('#'));
        if (commentIndex >= 0) {
            line = line.left(commentIndex).trimmed();
        }
        if (line.isEmpty()) {
            continue;
        }

        const QStringList parts = line.split(QRegularExpression(QStringLiteral("[ \\t]+")), Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            continue;
        }

        const QString key = parts.constFirst();
        QStringList values = result.value(key);
        for (int index = 1; index < parts.size(); ++index) {
            values.append(parts.at(index));
        }
        result.insert(key, values);
    }

    return result;
}

QMap<QString, QString> DnsConfigFragments::loadSystemHosts()
{
    QMap<QString, QString> hosts;

    const QString overridePath = qEnvironmentVariable("SONGBIRD_SYSTEM_HOSTS_PATH").trimmed();
    if (!overridePath.isEmpty()) {
        mergeSystemHostsFromFile(overridePath, hosts);
        return hosts;
    }

#ifdef Q_OS_WIN
    const QString systemRoot = qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"));
    const QString hostsDirectory = QDir::fromNativeSeparators(systemRoot) + QStringLiteral("/System32/drivers/etc/");
    mergeSystemHostsFromFile(hostsDirectory + QStringLiteral("hosts"), hosts);
    mergeSystemHostsFromFile(hostsDirectory + QStringLiteral("hosts.ics"), hosts);
#else
    mergeSystemHostsFromFile(QStringLiteral("/etc/hosts"), hosts);
#endif

    return hosts;
}

QJsonObject DnsConfigFragments::buildSingBoxFakeIpFilterRule()
{
    static const QJsonObject filterRule = []() {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray(kSingBoxFakeIpFilterJson), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            return QJsonObject{};
        }
        return document.object();
    }();
    return filterRule;
}

bool DnsConfigFragments::appendSingBoxDomainField(QJsonObject& rule, const QString& value, bool plainAsDomain)
{
    const QString domain = value.trimmed();
    if (domain.isEmpty()
        || domain.startsWith(QChar('#'))
        || domain.startsWith(QStringLiteral("ext:"), Qt::CaseInsensitive)
        || domain.startsWith(QStringLiteral("ext-domain:"), Qt::CaseInsensitive)) {
        return false;
    }

    if (domain.startsWith(QStringLiteral("geosite:"), Qt::CaseInsensitive)) {
        appendJsonArrayValue(rule, QStringLiteral("geosite"), domain.mid(QStringLiteral("geosite:").size()));
        return true;
    }
    if (domain.startsWith(QStringLiteral("regexp:"), Qt::CaseInsensitive)) {
        appendJsonArrayValue(rule, QStringLiteral("domain_regex"), domain.mid(QStringLiteral("regexp:").size()));
        return true;
    }
    if (domain.startsWith(QStringLiteral("domain:"), Qt::CaseInsensitive)) {
        appendJsonArrayValue(rule, QStringLiteral("domain_suffix"), domain.mid(QStringLiteral("domain:").size()));
        return true;
    }
    if (domain.startsWith(QStringLiteral("full:"), Qt::CaseInsensitive)) {
        appendJsonArrayValue(rule, QStringLiteral("domain"), domain.mid(QStringLiteral("full:").size()));
        return true;
    }
    if (domain.startsWith(QStringLiteral("keyword:"), Qt::CaseInsensitive)) {
        appendJsonArrayValue(rule, QStringLiteral("domain_keyword"), domain.mid(QStringLiteral("keyword:").size()));
        return true;
    }
    if (domain.startsWith(QStringLiteral("dotless:"), Qt::CaseInsensitive)) {
        appendJsonArrayValue(rule, QStringLiteral("domain_keyword"), domain.mid(QStringLiteral("dotless:").size()));
        return true;
    }

    appendJsonArrayValue(rule, plainAsDomain ? QStringLiteral("domain") : QStringLiteral("domain_keyword"), domain);
    return true;
}

bool DnsConfigFragments::usesDirectDnsAsFinalServer(const RoutingItem* routing)
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

bool DnsConfigFragments::isCustomDnsObjectText(const QString& value)
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

QJsonObject DnsConfigFragments::buildLegacyDns(const Config& config, const RoutingItem* selectedRouting)
{
    if (isCustomDnsObjectText(config.dns().remoteDns)) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(config.dns().remoteDns.trimmed().toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            return document.object();
        }
    }

    const QStringList remoteDnsAddresses = splitDnsAddresses(config.dns().remoteDns);
    const QStringList directDnsAddresses = splitDnsAddresses(config.dns().directDns);
    const QStringList bootstrapDnsAddresses = splitDnsAddresses(config.dns().bootstrapDns);
    const QMap<QString, QStringList> hostsMap = parseHostsToDictionary(config.dns().dnsHosts);
    const QMap<QString, QString> systemHostsMap = config.dns().useSystemHosts ? loadSystemHosts() : QMap<QString, QString>{};
    const bool useDirectFinal = usesDirectDnsAsFinalServer(selectedRouting);

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
        const QString host = extractDnsHost(address);
        if (!host.isEmpty() && host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) != 0 && !isIpAddress(host)) {
            dnsServerDomains.append(QStringLiteral("full:%1").arg(host));
        }
    }
    for (const QString& address : remoteDnsAddresses) {
        const QString host = extractDnsHost(address);
        if (!host.isEmpty() && host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) != 0 && !isIpAddress(host)) {
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
            for (auto it = kPredefinedHosts.constBegin(); it != kPredefinedHosts.constEnd(); ++it) {
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

QJsonObject DnsConfigFragments::buildSingBoxDns(const Config& config, const RoutingItem* selectedRouting)
{
    const QString remoteDns = config.dns().remoteDns.trimmed();
    if (!remoteDns.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(remoteDns.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError
            && document.isObject()
            && document.object().contains(QStringLiteral("servers"))) {
            return document.object();
        }
    }

    const bool useDirectFinal = usesDirectDnsAsFinalServer(selectedRouting);

    QJsonObject dns;
    dns.insert(QStringLiteral("independent_cache"), true);

    QJsonArray servers;
    QJsonObject localDns = parseSingBoxDnsAddress(config.dns().bootstrapDns);
    if (!localDns.isEmpty()) {
        localDns.insert(QStringLiteral("tag"), kSingBoxLocalDnsTag);
        servers.append(localDns);
    }

    QJsonObject remoteDnsServer = parseSingBoxDnsAddress(config.dns().remoteDns);
    if (!remoteDnsServer.isEmpty()) {
        remoteDnsServer.insert(QStringLiteral("tag"), kSingBoxRemoteDnsTag);
        remoteDnsServer.insert(QStringLiteral("detour"), QStringLiteral("proxy"));
        remoteDnsServer.insert(QStringLiteral("domain_resolver"), kSingBoxLocalDnsTag);
        servers.append(remoteDnsServer);
    }

    QJsonObject directDnsServer = parseSingBoxDnsAddress(config.dns().directDns);
    if (!directDnsServer.isEmpty()) {
        directDnsServer.insert(QStringLiteral("tag"), kSingBoxDirectDnsTag);
        directDnsServer.insert(QStringLiteral("domain_resolver"), kSingBoxLocalDnsTag);
        servers.append(directDnsServer);
    }

    const QMap<QString, QStringList> hostsMap = parseHostsToDictionary(config.dns().dnsHosts);
    const QMap<QString, QString> systemHostsMap = config.dns().useSystemHosts ? loadSystemHosts() : QMap<QString, QString>{};
    QJsonObject predefinedHosts;
    if (config.dns().addCommonHosts) {
        for (auto it = kPredefinedHosts.constBegin(); it != kPredefinedHosts.constEnd(); ++it) {
            QJsonArray answers;
            for (const QString& value : it.value()) {
                answers.append(value);
            }
            predefinedHosts.insert(it.key(), answers);
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
        if (!appendSingBoxDomainField(domainRule, it.key(), true)) {
            continue;
        }

        const QJsonArray exactDomains = domainRule.value(QStringLiteral("domain")).toArray();
        if (exactDomains.size() != 1) {
            continue;
        }

        QJsonArray answers;
        for (const QString& hostValue : it.value()) {
            if (isIpAddress(hostValue)) {
                answers.append(hostValue);
            }
        }
        if (!answers.isEmpty()) {
            predefinedHosts.insert(exactDomains.at(0).toString(), answers);
        }
    }

    const bool hasPredefinedHosts = !predefinedHosts.isEmpty();
    QJsonObject hostsDnsServer;
    if (hasPredefinedHosts) {
        hostsDnsServer.insert(QStringLiteral("tag"), kSingBoxHostsDnsTag);
        hostsDnsServer.insert(QStringLiteral("type"), QStringLiteral("hosts"));
        hostsDnsServer.insert(QStringLiteral("predefined"), predefinedHosts);
    }

    const auto applyHostsResolver = [&predefinedHosts](QJsonObject& dnsServer) {
        const QString serverName = dnsServer.value(QStringLiteral("server")).toString().trimmed();
        if (!serverName.isEmpty() && predefinedHosts.contains(serverName)) {
            dnsServer.insert(QStringLiteral("domain_resolver"), kSingBoxHostsDnsTag);
        }
    };
    applyHostsResolver(localDns);
    applyHostsResolver(remoteDnsServer);
    applyHostsResolver(directDnsServer);
    for (qsizetype index = 0; index < servers.size(); ++index) {
        const QString tag = servers.at(index).toObject().value(QStringLiteral("tag")).toString();
        if (tag == kSingBoxLocalDnsTag) {
            servers[index] = localDns;
        } else if (tag == kSingBoxRemoteDnsTag) {
            servers[index] = remoteDnsServer;
        } else if (tag == kSingBoxDirectDnsTag) {
            servers[index] = directDnsServer;
        }
    }
    if (hasPredefinedHosts) {
        servers.append(hostsDnsServer);
    }

    if (config.dns().fakeIp) {
        QJsonObject fakeDnsServer;
        fakeDnsServer.insert(QStringLiteral("tag"), kSingBoxFakeDnsTag);
        fakeDnsServer.insert(QStringLiteral("type"), QStringLiteral("fakeip"));
        fakeDnsServer.insert(QStringLiteral("inet4_range"), QStringLiteral("198.18.0.0/15"));
        fakeDnsServer.insert(QStringLiteral("inet6_range"), QStringLiteral("fc00::/18"));
        servers.append(fakeDnsServer);
    }

    if (servers.isEmpty()) {
        return {};
    }

    dns.insert(QStringLiteral("servers"), servers);
    QString finalServerTag;
    if (!remoteDnsServer.isEmpty() && !useDirectFinal) {
        finalServerTag = kSingBoxRemoteDnsTag;
    } else if (!directDnsServer.isEmpty()) {
        finalServerTag = kSingBoxDirectDnsTag;
    } else if (hasPredefinedHosts) {
        finalServerTag = kSingBoxHostsDnsTag;
    }
    if (!finalServerTag.isEmpty()) {
        dns.insert(QStringLiteral("final"), finalServerTag);
    }

    QJsonArray rules;
    if (hasPredefinedHosts) {
        QJsonObject hostsRule;
        hostsRule.insert(QStringLiteral("ip_accept_any"), true);
        hostsRule.insert(QStringLiteral("server"), kSingBoxHostsDnsTag);
        rules.append(hostsRule);
    }

    if (!remoteDnsServer.isEmpty()) {
        QJsonObject proxyRule;
        proxyRule.insert(QStringLiteral("server"), kSingBoxRemoteDnsTag);
        proxyRule.insert(QStringLiteral("clash_mode"), QStringLiteral("Global"));
        const QString proxyStrategy = mapDomainStrategyToSingBox(config.dns().domainStrategyForProxy);
        if (!proxyStrategy.isEmpty()) {
            proxyRule.insert(QStringLiteral("strategy"), proxyStrategy);
        }
        rules.append(proxyRule);
    }

    if (!directDnsServer.isEmpty()) {
        QJsonObject directRule;
        directRule.insert(QStringLiteral("server"), kSingBoxDirectDnsTag);
        directRule.insert(QStringLiteral("clash_mode"), QStringLiteral("Direct"));
        const QString directStrategy = mapDomainStrategyToSingBox(config.dns().domainStrategyForFreedom);
        if (!directStrategy.isEmpty()) {
            directRule.insert(QStringLiteral("strategy"), directStrategy);
        }
        rules.append(directRule);
    }

    for (auto it = hostsMap.constBegin(); it != hostsMap.constEnd(); ++it) {
        if (it.value().isEmpty()) {
            continue;
        }

        const QString predefined = it.value().constFirst().trimmed();
        if (predefined.isEmpty()) {
            continue;
        }

        QJsonObject rule;
        rule.insert(QStringLiteral("query_type"), QJsonArray{1, 5, 28});
        rule.insert(QStringLiteral("action"), QStringLiteral("predefined"));
        rule.insert(QStringLiteral("rcode"), QStringLiteral("NOERROR"));
        if (!appendSingBoxDomainField(rule, it.key(), true)) {
            continue;
        }

        if (predefined.startsWith(QChar('#'))) {
            bool ok = false;
            const int rcode = predefined.mid(1).toInt(&ok);
            rule.insert(QStringLiteral("rcode"), mapDnsRcode(ok ? rcode : 0));
        } else if (isDomainName(predefined)) {
            rule.insert(QStringLiteral("answer"), QJsonArray{QStringLiteral("*. IN CNAME %1.").arg(predefined)});
        } else if (isIpAddress(predefined) && rule.value(QStringLiteral("domain")).toArray().isEmpty()) {
            rule.insert(
                QStringLiteral("answer"),
                QJsonArray{
                    QStringLiteral("*. IN %1 %2").arg(predefined.contains(QChar(':')) ? QStringLiteral("AAAA") : QStringLiteral("A"), predefined)});
        } else {
            continue;
        }

        rules.append(rule);
    }

    if (config.dns().blockBindingQuery) {
        QJsonObject blockBindingRule;
        blockBindingRule.insert(QStringLiteral("query_type"), QJsonArray{64, 65});
        blockBindingRule.insert(QStringLiteral("action"), QStringLiteral("predefined"));
        blockBindingRule.insert(QStringLiteral("rcode"), QStringLiteral("NOERROR"));
        rules.append(blockBindingRule);
    }

    if (config.dns().fakeIp && config.dns().globalFakeIp) {
        QJsonObject filterRule = buildSingBoxFakeIpFilterRule();
        if (!filterRule.isEmpty()) {
            filterRule.insert(QStringLiteral("invert"), true);

            QJsonObject fakeDnsRule;
            fakeDnsRule.insert(QStringLiteral("server"), kSingBoxFakeDnsTag);
            fakeDnsRule.insert(QStringLiteral("type"), QStringLiteral("logical"));
            fakeDnsRule.insert(QStringLiteral("mode"), QStringLiteral("and"));
            fakeDnsRule.insert(QStringLiteral("rewrite_ttl"), 1);
            fakeDnsRule.insert(
                QStringLiteral("rules"),
                QJsonArray{
                    QJsonObject{{QStringLiteral("query_type"), QJsonArray{1, 28}}},
                    filterRule});
            rules.append(fakeDnsRule);
        }
    }

    QStringList expectedIpCidrs;
    QStringList expectedIpGeoips;
    QSet<QString> expectedIpRegionNames;
    for (const QString& item : config.dns().directExpectedIps.split(QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts)) {
        const QString trimmed = item.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("geoip:"), Qt::CaseInsensitive)) {
            const QString region = trimmed.mid(QStringLiteral("geoip:").size()).trimmed();
            if (region.isEmpty()) {
                continue;
            }
            expectedIpGeoips.append(region);
            expectedIpRegionNames.insert(region);
            expectedIpRegionNames.insert(QStringLiteral("geolocation-%1").arg(region));
            expectedIpRegionNames.insert(QStringLiteral("tld-%1").arg(region));
        } else {
            expectedIpCidrs.append(trimmed);
        }
    }

    const QList<RoutingRule> effectiveRules = effectiveRoutingRules(config, selectedRouting);
    if (!effectiveRules.isEmpty()) {
        const QString directStrategy = mapDomainStrategyToSingBox(config.dns().domainStrategyForFreedom);
        const QString proxyStrategy = mapDomainStrategyToSingBox(config.dns().domainStrategyForProxy);

        for (const RoutingRule& sourceRule : effectiveRules) {
            if (!sourceRule.enabled) {
                continue;
            }

            const QStringList domains = RoutingRuleJsonMapper::normalizeRuleValues(sourceRule.domain, true, true);
            if (domains.isEmpty()) {
                continue;
            }

            QJsonObject rule;
            int validDomainCount = 0;
            for (const QString& domain : domains) {
                if (appendSingBoxDomainField(rule, domain, true)) {
                    ++validDomainCount;
                }
            }
            if (validDomainCount <= 0) {
                continue;
            }

            const QString outboundTag = sourceRule.outboundTag.trimmed();
            if (outboundTag.compare(QStringLiteral("direct"), Qt::CaseInsensitive) == 0) {
                rule.insert(QStringLiteral("server"), kSingBoxDirectDnsTag);
                if (!directStrategy.isEmpty()) {
                    rule.insert(QStringLiteral("strategy"), directStrategy);
                }

                if (!expectedIpGeoips.isEmpty()) {
                    QSet<QString> geositeValues;
                    for (const QJsonValue& geositeValue : rule.value(QStringLiteral("geosite")).toArray()) {
                        geositeValues.insert(geositeValue.toString());
                    }
                    bool matchedExpectedRegion = false;
                    for (const QString& geositeValue : geositeValues) {
                        if (expectedIpRegionNames.contains(geositeValue)) {
                            matchedExpectedRegion = true;
                            break;
                        }
                    }
                    if (matchedExpectedRegion) {
                        if (!expectedIpGeoips.isEmpty()) {
                            QJsonArray geoipArray;
                            for (const QString& region : expectedIpGeoips) {
                                geoipArray.append(region);
                            }
                            rule.insert(QStringLiteral("geoip"), geoipArray);
                        }
                        if (!expectedIpCidrs.isEmpty()) {
                            QJsonArray ipCidrArray;
                            for (const QString& cidr : expectedIpCidrs) {
                                ipCidrArray.append(cidr);
                            }
                            rule.insert(QStringLiteral("ip_cidr"), ipCidrArray);
                        }
                    }
                }
            } else if (outboundTag.compare(QStringLiteral("block"), Qt::CaseInsensitive) == 0) {
                rule.insert(QStringLiteral("action"), QStringLiteral("predefined"));
                rule.insert(QStringLiteral("rcode"), QStringLiteral("NXDOMAIN"));
            } else {
                if (config.dns().fakeIp && !config.dns().globalFakeIp) {
                    QJsonObject fakeRule = rule;
                    fakeRule.insert(QStringLiteral("server"), kSingBoxFakeDnsTag);
                    fakeRule.insert(QStringLiteral("query_type"), QJsonArray{1, 28});
                    fakeRule.insert(QStringLiteral("rewrite_ttl"), 1);
                    rules.append(fakeRule);
                }
                rule.insert(QStringLiteral("server"), kSingBoxRemoteDnsTag);
                if (!proxyStrategy.isEmpty()) {
                    rule.insert(QStringLiteral("strategy"), proxyStrategy);
                }
            }

            rules.append(rule);
        }
    }

    if (config.dns().fakeIp && !config.dns().globalFakeIp && !useDirectFinal) {
        QJsonObject fakeDnsRule;
        fakeDnsRule.insert(QStringLiteral("server"), kSingBoxFakeDnsTag);
        fakeDnsRule.insert(QStringLiteral("query_type"), QJsonArray{1, 28});
        fakeDnsRule.insert(QStringLiteral("rewrite_ttl"), 1);
        rules.append(fakeDnsRule);
    }

    if (!rules.isEmpty()) {
        dns.insert(QStringLiteral("rules"), rules);
    }

    return dns;
}
