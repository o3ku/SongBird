#include "runtime/ClientConfigWriter.h"

#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>

#include <utility>

#include "runtime/TunCompatCoreRequirement.h"
#include "runtime/ClashConfigWriter.h"

namespace {

const QString kStatisticsTag = QStringLiteral("api");
const QString kLoopbackAddress = QStringLiteral("127.0.0.1");
const QString kStatisticsProtocol = QStringLiteral("dokodemo-door");
const QString kStatisticsService = QStringLiteral("StatsService");
const QString kDefaultAccessLogFileName = QStringLiteral("Vaccess.log");
const QString kDefaultErrorLogFileName = QStringLiteral("Verror.log");
const QString kPemBeginMarker = QStringLiteral("-----BEGIN CERTIFICATE-----");
const QString kPemEndMarker = QStringLiteral("-----END CERTIFICATE-----");
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

bool isSupportedSingBoxNetwork(const QString& network)
{
    static const QSet<QString> supportedNetworks{
        QStringLiteral("tcp"),
        QStringLiteral("ws"),
        QStringLiteral("grpc"),
        QStringLiteral("h2"),
        QStringLiteral("httpupgrade"),
        QStringLiteral("quic")};
    return supportedNetworks.contains(network);
}

bool isSupportedSingBoxNonTcpTransport(ConfigType type)
{
    switch (type) {
    case ConfigType::VMess:
    case ConfigType::VLESS:
    case ConfigType::Trojan:
    case ConfigType::Shadowsocks:
    case ConfigType::Hysteria2:
    case ConfigType::TUIC:
    case ConfigType::AnyTLS:
    case ConfigType::Naive:
        return true;
    case ConfigType::WireGuard:
    case ConfigType::Socks:
    case ConfigType::HTTP:
    case ConfigType::Custom:
    case ConfigType::Unknown:
    default:
        return false;
    }
}

bool isSupportedSingBoxShadowsocksTransport(const QString& network)
{
    static const QSet<QString> supportedNetworks{
        QStringLiteral("tcp"),
        QStringLiteral("ws"),
        QStringLiteral("quic")};
    return supportedNetworks.contains(network);
}

bool resolveMuxEnabled(const Config& config, const VmessItem& server)
{
    return server.muxEnabled.value_or(config.muxEnabled);
}

bool isValidJsonObjectText(const QString& value)
{
    if (value.trimmed().isEmpty()) {
        return true;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(value.toUtf8(), &parseError);
    return parseError.error == QJsonParseError::NoError && document.isObject();
}

QString resolveLegacyUserAgent(const Config& config, const VmessItem& server)
{
    return server.userAgent.trimmed().isEmpty()
        ? config.defaultUserAgent.trimmed()
        : server.userAgent.trimmed();
}

QStringList splitPemCertificates(const QString& value)
{
    QString text = value;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QChar('\r'), QChar('\n'));

    QStringList certificates;
    int searchFrom = 0;
    while (true) {
        const int begin = text.indexOf(kPemBeginMarker, searchFrom);
        if (begin < 0) {
            break;
        }

        const int end = text.indexOf(kPemEndMarker, begin);
        if (end < 0) {
            break;
        }

        const int afterEnd = end + kPemEndMarker.size();
        const QString certificate = text.mid(begin, afterEnd - begin).trimmed();
        if (!certificate.isEmpty()) {
            certificates.append(certificate);
        }
        searchFrom = afterEnd;
    }

    return certificates;
}

QJsonArray buildLegacyTlsCertificates(const QStringList& certificates)
{
    QJsonArray result;
    for (const QString& certificate : certificates) {
        QJsonArray certificateLines;
        const QStringList lines = certificate.split(QChar('\n'));
        for (const QString& line : lines) {
            const QString trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                certificateLines.append(trimmed);
            }
        }
        if (certificateLines.isEmpty()) {
            continue;
        }

        QJsonObject item;
        item.insert(QStringLiteral("certificate"), certificateLines);
        item.insert(QStringLiteral("usage"), QStringLiteral("verify"));
        result.append(item);
    }
    return result;
}

QStringList splitDnsAddresses(const QString& value)
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

QJsonObject parseSingBoxDnsAddress(const QString& value)
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

QString mapDomainStrategyToSingBox(const QString& strategy)
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

bool isIpAddress(const QString& value)
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

bool isDomainName(const QString& value)
{
    const QString trimmed = value.trimmed();
    return !trimmed.isEmpty()
        && !isIpAddress(trimmed)
        && trimmed.contains(QChar('.'))
        && !trimmed.contains(QChar(' '));
}

QString mapDnsRcode(int code)
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

QMap<QString, QStringList> parseHostsToDictionary(const QString& hosts)
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
        if (!isIpAddress(ipAddress)) {
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

    const QMap<QString, QString> parsedHosts = parseSystemHostsText(QString::fromUtf8(file.readAll()));
    for (auto it = parsedHosts.constBegin(); it != parsedHosts.constEnd(); ++it) {
        hosts.insert(it.key(), it.value());
    }
}

QMap<QString, QString> loadSystemHosts()
{
    QMap<QString, QString> hosts;

    const QString overridePath = qEnvironmentVariable("V2RAYQ_SYSTEM_HOSTS_PATH").trimmed();
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

void appendJsonArrayValue(QJsonObject& object, const QString& key, const QString& value)
{
    QJsonArray array = object.value(key).toArray();
    array.append(value);
    object.insert(key, array);
}

QJsonObject buildSingBoxFakeIpFilterRule()
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

bool appendSingBoxDomainField(QJsonObject& rule, const QString& value, bool plainAsDomain)
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

QString extractDnsHost(const QString& address)
{
    const QStringList addresses = splitDnsAddresses(address);
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
    const QStringList addresses = splitDnsAddresses(dnsAddress);
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

    server.insert(QStringLiteral("skipFallback"), skipFallback);
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
        server.insert(QStringLiteral("expectedIPs"), expectedIpArray);
    }
    return server;
}

} // namespace

ClientConfigWriter::ClientConfigWriter(QString customConfigDirectory)
    : customConfigDirectory_(std::move(customConfigDirectory))
{
}

OperationResult ClientConfigWriter::writeClientConfig(
    const Config& config,
    const VmessItem& server,
    const QString& filePath,
    int statisticsPort) const
{
    return writeClientConfigs(config, server, filePath, statisticsPort, nullptr);
}

OperationResult ClientConfigWriter::writeClientConfigs(
    const Config& config,
    const VmessItem& server,
    const QString& filePath,
    int statisticsPort,
    QStringList* auxiliaryPaths) const
{
    if (server.address.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Cannot generate config for an empty server address."));
    }

    if (filePath.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Runtime config path is empty."));
    }

    const OperationResult validationResult = validateServer(server);
    if (!validationResult.success) {
        return validationResult;
    }

    if (server.configType == ConfigType::Custom) {
        return writeCustomConfig(server, filePath);
    }

    CoreType effectiveCore = resolveRuntimeCoreType(server.coreType);
    for (const CoreTypeItem& item : config.coreTypeItems) {
        if (item.configType == static_cast<int>(server.configType)) {
            effectiveCore = resolveRuntimeCoreType(static_cast<CoreType>(item.coreType));
            break;
        }
    }
    if (effectiveCore == CoreType::Clash || effectiveCore == CoreType::ClashMeta) {
        return ClashConfigWriter::writeClientConfig(config, server, filePath, statisticsPort);
    }

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return OperationResult::fail(QStringLiteral("Failed to create runtime config directory."));
    }

    const GeneratedConfigSet generated = generateClientConfigs(config, server, statisticsPort);
    const OperationResult primaryWriteResult = writeGeneratedConfig(generated.primary, filePath);
    if (!primaryWriteResult.success) {
        return primaryWriteResult;
    }

    if (auxiliaryPaths != nullptr) {
        auxiliaryPaths->clear();
    }

    const QDir outputDirectory = fileInfo.dir();
    for (const GeneratedConfig& auxiliaryConfig : generated.auxiliary) {
        const QString auxiliaryPath = outputDirectory.filePath(auxiliaryConfig.fileName);
        const OperationResult auxiliaryWriteResult = writeGeneratedConfig(auxiliaryConfig, auxiliaryPath);
        if (!auxiliaryWriteResult.success) {
            return auxiliaryWriteResult;
        }
        if (auxiliaryPaths != nullptr) {
            auxiliaryPaths->append(auxiliaryPath);
        }
    }

    return primaryWriteResult;
}

OperationResult ClientConfigWriter::writeCustomConfig(const VmessItem& server, const QString& filePath) const
{
    const QString sourcePath = resolveCustomConfigPath(server.address);
    if (sourcePath.trimmed().isEmpty() || !QFileInfo::exists(sourcePath)) {
        return OperationResult::fail(QStringLiteral("Custom config file does not exist."));
    }

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return OperationResult::fail(QStringLiteral("Failed to create runtime config directory."));
    }

    if (QFileInfo(sourcePath).absoluteFilePath().compare(fileInfo.absoluteFilePath(), Qt::CaseInsensitive) == 0) {
        return OperationResult::ok(QStringLiteral("Runtime config generated: %1").arg(QDir::toNativeSeparators(filePath)));
    }

    if (QFileInfo::exists(filePath) && !QFile::remove(filePath)) {
        return OperationResult::fail(QStringLiteral("Failed to replace existing runtime custom config file."));
    }

    if (!QFile::copy(sourcePath, filePath)) {
        return OperationResult::fail(QStringLiteral("Failed to copy custom config file to runtime path."));
    }

    return OperationResult::ok(QStringLiteral("Runtime config generated: %1").arg(QDir::toNativeSeparators(filePath)));
}

ClientConfigWriter::GeneratedConfigSet ClientConfigWriter::generateClientConfigs(
    const Config& config,
    const VmessItem& server,
    int statisticsPort) const
{
    GeneratedConfigSet generated;
    generated.primary.fileName = QStringLiteral("config.generated.json");
    generated.primary.root = buildRoot(config, server, statisticsPort);

    if (requiresTunCompatSingBoxCore(config, server)) {
        // The local rewrite has not split upstream's non-legacy Xray TUN relay path yet.
        // Keep emitting the sing-box sidecar whenever Xray runs with TUN so `singbox_tun`
        // can still be created instead of silently starting Xray alone with no TUN inbound.
        GeneratedConfig compat;
        compat.fileName = QStringLiteral("tun-singbox-compat.json");
        compat.root = buildTunCompatSingBoxRoot(config);
        generated.auxiliary.append(compat);
    }

    return generated;
}

OperationResult ClientConfigWriter::validateServer(const VmessItem& server) const
{
    const CoreType runtimeCore = resolveRuntimeCoreType(server.coreType);
    const QString transportSecurity = server.streamSecurity.trimmed();
    const bool realityTransport = isRealityTransport(transportSecurity);

    if (realityTransport) {
        if (runtimeCore == CoreType::V2Fly) {
            return OperationResult::fail(QStringLiteral("Reality transport requires Xray or sing-box core."));
        }

        if (server.publicKey.trimmed().isEmpty()) {
            return OperationResult::fail(QStringLiteral("Reality transport requires a public key."));
        }

        if (!isValidRealityShortId(server.shortId)) {
            return OperationResult::fail(
                QStringLiteral("Reality short ID must be a hexadecimal string with an even length up to 16 characters."));
        }
    }

    if (server.network.compare(QStringLiteral("xhttp"), Qt::CaseInsensitive) == 0
        && !server.extra.trimmed().isEmpty()) {
        if (!isValidJsonObjectText(server.extra)) {
            return OperationResult::fail(QStringLiteral("XHTTP extra must be a valid JSON object."));
        }
    }

    if (!isValidJsonObjectText(server.finalmask)) {
        return OperationResult::fail(QStringLiteral("Finalmask must be a valid JSON object."));
    }

    if (runtimeCore != CoreType::SingBox) {
        return OperationResult::ok();
    }

    if (resolveSingBoxOutboundType(server.configType).isEmpty()) {
        return OperationResult::fail(QStringLiteral("The selected server type is not supported by the current sing-box generator."));
    }

    const QString network = server.network.trimmed().isEmpty()
        ? QStringLiteral("tcp")
        : server.network.trimmed().toLower();
    if (!isSupportedSingBoxNetwork(network)) {
        return OperationResult::fail(
            QStringLiteral("sing-box config generation does not support network %1 yet.").arg(network));
    }

    if (network != QStringLiteral("tcp") && !isSupportedSingBoxNonTcpTransport(server.configType)) {
        return OperationResult::fail(
            QStringLiteral("sing-box does not support %1 transport for %2 nodes.")
                .arg(network, configTypeDisplayName(server.configType)));
    }

    if (server.configType == ConfigType::Shadowsocks
        && !isSupportedSingBoxShadowsocksTransport(network)) {
        return OperationResult::fail(
            QStringLiteral("sing-box does not support %1 transport for %2 nodes.")
                .arg(network, configTypeDisplayName(server.configType)));
    }

    return OperationResult::ok();
}

OperationResult ClientConfigWriter::writeGeneratedConfig(const GeneratedConfig& generatedConfig, const QString& filePath) const
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return OperationResult::fail(QStringLiteral("Failed to open runtime config file for writing."));
    }

    const QJsonDocument document(generatedConfig.root);
    if (file.write(document.toJson(QJsonDocument::Indented)) < 0) {
        return OperationResult::fail(QStringLiteral("Failed to write runtime config file."));
    }

    if (!file.commit()) {
        return OperationResult::fail(QStringLiteral("Failed to commit runtime config file."));
    }

    return OperationResult::ok(QStringLiteral("Runtime config generated: %1").arg(QDir::toNativeSeparators(filePath)));
}

QJsonObject ClientConfigWriter::buildRoot(const Config& config, const VmessItem& server, int statisticsPort) const
{
    const bool requiresSingBox = server.configType == ConfigType::AnyTLS
        || server.configType == ConfigType::Naive;
    CoreType effectiveCore = resolveRuntimeCoreType(server.coreType);
    for (const CoreTypeItem& item : config.coreTypeItems) {
        if (item.configType == static_cast<int>(server.configType)) {
            effectiveCore = resolveRuntimeCoreType(static_cast<CoreType>(item.coreType));
            break;
        }
    }
    const bool useSingBox = requiresSingBox || effectiveCore == CoreType::SingBox;
    return useSingBox
        ? buildSingBoxRoot(config, server, statisticsPort)
        : buildLegacyRoot(config, server, statisticsPort);
}

QJsonObject ClientConfigWriter::buildLegacyRoot(const Config& config, const VmessItem& server, int statisticsPort) const
{
    QJsonObject root;
    root.insert(QStringLiteral("log"), buildLog(config));
    QJsonArray inbounds = buildInbounds(config);
    if (config.enableStatistics && statisticsPort > 0) {
        appendLegacyStatisticsInbound(inbounds, statisticsPort);
    }
    root.insert(QStringLiteral("inbounds"), inbounds);
    root.insert(QStringLiteral("outbounds"), buildOutbounds(config, server));

    QJsonObject routing = buildLegacyRouting(config);
    if (config.enableStatistics && statisticsPort > 0) {
        appendLegacyStatisticsRouteRule(routing);
    }
    if (!routing.isEmpty()) {
        root.insert(QStringLiteral("routing"), routing);
    }

    const QJsonObject dns = buildLegacyDns(config);
    if (!dns.isEmpty()) {
        root.insert(QStringLiteral("dns"), dns);
    }

    if (config.enableStatistics && statisticsPort > 0) {
        root.insert(QStringLiteral("stats"), QJsonObject());
        root.insert(QStringLiteral("api"), buildLegacyApi());
        root.insert(QStringLiteral("policy"), buildLegacyPolicy());
    }

    return root;
}

QJsonObject ClientConfigWriter::buildSingBoxRoot(const Config& config, const VmessItem& server, int statisticsPort) const
{
    QJsonObject root;
    root.insert(QStringLiteral("log"), buildSingBoxLog(config));
    root.insert(QStringLiteral("inbounds"), buildSingBoxInbounds(config));
    root.insert(QStringLiteral("outbounds"), buildSingBoxOutbounds(config, server));
    root.insert(QStringLiteral("route"), buildSingBoxRoute(config));

    const QJsonObject dns = buildSingBoxDns(config);
    if (!dns.isEmpty()) {
        root.insert(QStringLiteral("dns"), dns);
    }

    const QJsonObject experimental = buildSingBoxExperimental(config, statisticsPort);
    if (!experimental.isEmpty()) {
        root.insert(QStringLiteral("experimental"), experimental);
    }

    migrateGeoToRuleSet(root);
    return root;
}

QJsonObject ClientConfigWriter::buildTunCompatSingBoxRoot(const Config& config) const
{
    QJsonObject root;
    root.insert(QStringLiteral("log"), buildSingBoxLog(config));

    QJsonArray inbounds;
    inbounds.append(buildSingBoxTunInbound(config));
    root.insert(QStringLiteral("inbounds"), inbounds);

    root.insert(QStringLiteral("outbounds"), buildTunCompatSingBoxOutbounds(config));
    root.insert(QStringLiteral("route"), buildTunCompatSingBoxRoute(config));

    const QJsonObject dns = buildTunCompatSingBoxDns();
    if (!dns.isEmpty()) {
        root.insert(QStringLiteral("dns"), dns);
    }

    return root;
}

QJsonObject ClientConfigWriter::buildLegacyDns(const Config& config) const
{
    if (isCustomDnsObjectText(config.remoteDns)) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(config.remoteDns.trimmed().toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            return document.object();
        }
    }

    const QStringList remoteDnsAddresses = splitDnsAddresses(config.remoteDns);
    const QStringList directDnsAddresses = splitDnsAddresses(config.directDns);
    const QStringList bootstrapDnsAddresses = splitDnsAddresses(config.bootstrapDns);
    const QMap<QString, QStringList> hostsMap = parseHostsToDictionary(config.dnsHosts);
    const QMap<QString, QString> systemHostsMap = config.useSystemHosts ? loadSystemHosts() : QMap<QString, QString>{};
    const RoutingItem* selectedRouting = resolveSelectedRouting(config);
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
    for (const QString& item : config.directExpectedIps.split(QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts)) {
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

            const QStringList domains = normalizeRuleValues(rule.domain, true, true);
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
    if (config.serveStale) {
        dns.insert(QStringLiteral("serveStale"), true);
    }
    if (config.parallelQuery) {
        dns.insert(QStringLiteral("enableParallelQuery"), true);
    }
    if (config.addCommonHosts || config.useSystemHosts || !hostsMap.isEmpty()) {
        QJsonObject hostsObject;
        if (config.addCommonHosts) {
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

QJsonObject ClientConfigWriter::buildLegacyRouting(const Config& config) const
{
    QJsonObject routing;

    if (!config.domainStrategy.trimmed().isEmpty()) {
        routing.insert(QStringLiteral("domainStrategy"), config.domainStrategy.trimmed());
    }

    if (!config.domainMatcher.trimmed().isEmpty()) {
        routing.insert(QStringLiteral("domainMatcher"), config.domainMatcher.trimmed());
    }

    QJsonArray rules;
    const RoutingItem* selectedRouting = resolveSelectedRouting(config);
    if (selectedRouting != nullptr) {
        for (const RoutingRule& rule : selectedRouting->rules) {
            if (!rule.enabled) {
                continue;
            }
            appendLegacyRoutingRule(rules, rule);
        }
    }

    if (!isCustomDnsObjectText(config.remoteDns)) {
        const QStringList directDnsAddresses = splitDnsAddresses(config.directDns);
        QStringList directDomains;
        if (selectedRouting != nullptr) {
            for (const RoutingRule& rule : selectedRouting->rules) {
                if (rule.enabled && rule.outboundTag.compare(QStringLiteral("direct"), Qt::CaseInsensitive) == 0) {
                    directDomains.append(normalizeRuleValues(rule.domain, true, true));
                }
            }
        }

        int directDnsTagIndex = 1;
        if (!directDomains.isEmpty()) {
            for (qsizetype index = 0; index < directDnsAddresses.size(); ++index) {
                QJsonObject rule;
                rule.insert(QStringLiteral("type"), QStringLiteral("field"));
                rule.insert(QStringLiteral("outboundTag"), QStringLiteral("direct"));
                rule.insert(
                    QStringLiteral("inboundTag"),
                    QJsonArray{QStringLiteral("%1-%2").arg(kLegacyDirectDnsTagPrefix).arg(directDnsTagIndex++)});
                rules.append(rule);
            }
        }
        if (usesDirectDnsAsFinalServer(selectedRouting)) {
            for (qsizetype index = 0; index < directDnsAddresses.size(); ++index) {
                QJsonObject rule;
                rule.insert(QStringLiteral("type"), QStringLiteral("field"));
                rule.insert(QStringLiteral("outboundTag"), QStringLiteral("direct"));
                rule.insert(
                    QStringLiteral("inboundTag"),
                    QJsonArray{QStringLiteral("%1-%2").arg(kLegacyDirectDnsTagPrefix).arg(directDnsTagIndex++)});
                rules.append(rule);
            }
        }

        if (!directDnsAddresses.isEmpty() || !splitDnsAddresses(config.remoteDns).isEmpty()) {
            QJsonObject dnsFinalRule;
            dnsFinalRule.insert(QStringLiteral("type"), QStringLiteral("field"));
            dnsFinalRule.insert(
                QStringLiteral("outboundTag"),
                usesDirectDnsAsFinalServer(selectedRouting) ? QStringLiteral("direct") : QStringLiteral("proxy"));
            dnsFinalRule.insert(QStringLiteral("inboundTag"), QJsonArray{kLegacyDnsTag});
            rules.append(dnsFinalRule);
        }
    }

    if (!rules.isEmpty()) {
        routing.insert(QStringLiteral("rules"), rules);
    }

    return routing;
}

QJsonObject ClientConfigWriter::buildLegacyApi() const
{
    QJsonObject api;
    api.insert(QStringLiteral("tag"), kStatisticsTag);

    QJsonArray services;
    services.append(kStatisticsService);
    api.insert(QStringLiteral("services"), services);
    return api;
}

QJsonObject ClientConfigWriter::buildLegacyPolicy() const
{
    QJsonObject policy;
    QJsonObject system;
    system.insert(QStringLiteral("statsOutboundDownlink"), true);
    system.insert(QStringLiteral("statsOutboundUplink"), true);
    policy.insert(QStringLiteral("system"), system);
    return policy;
}

QJsonObject ClientConfigWriter::buildSingBoxExperimental(const Config& config, int statisticsPort) const
{
    QJsonObject experimental;

    if (config.enableStatistics && statisticsPort > 0) {
        QJsonObject stats;
        stats.insert(QStringLiteral("enabled"), true);
        stats.insert(QStringLiteral("inbounds"), buildSingBoxStatisticsInboundTags(config));
        stats.insert(QStringLiteral("outbounds"), buildSingBoxStatisticsOutboundTags());

        QJsonObject api;
        api.insert(QStringLiteral("listen"), QStringLiteral("%1:%2").arg(kLoopbackAddress).arg(statisticsPort));
        api.insert(QStringLiteral("stats"), stats);
        experimental.insert(QStringLiteral("v2ray_api"), api);
    }

    if (config.enableCacheFile4Sbox) {
        QJsonObject cacheFile;
        cacheFile.insert(QStringLiteral("enabled"), true);
        cacheFile.insert(
            QStringLiteral("path"),
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("cache.db")));
        cacheFile.insert(QStringLiteral("store_fakeip"), config.fakeIp);
        experimental.insert(QStringLiteral("cache_file"), cacheFile);
    }

    return experimental;
}

void ClientConfigWriter::appendLegacyStatisticsInbound(QJsonArray& inbounds, int statisticsPort) const
{
    if (statisticsPort <= 0 || containsLegacyApiInbound(inbounds)) {
        return;
    }

    QJsonObject inbound;
    inbound.insert(QStringLiteral("tag"), kStatisticsTag);
    inbound.insert(QStringLiteral("listen"), kLoopbackAddress);
    inbound.insert(QStringLiteral("port"), statisticsPort);
    inbound.insert(QStringLiteral("protocol"), kStatisticsProtocol);

    QJsonObject settings;
    settings.insert(QStringLiteral("address"), kLoopbackAddress);
    inbound.insert(QStringLiteral("settings"), settings);
    inbounds.append(inbound);
}

void ClientConfigWriter::appendLegacyStatisticsRouteRule(QJsonObject& routing) const
{
    QJsonArray rules = routing.value(QStringLiteral("rules")).toArray();
    if (!containsLegacyApiRoute(rules)) {
        QJsonObject rule;
        rule.insert(QStringLiteral("type"), QStringLiteral("field"));
        rule.insert(QStringLiteral("outboundTag"), kStatisticsTag);

        QJsonArray inboundTags;
        inboundTags.append(kStatisticsTag);
        rule.insert(QStringLiteral("inboundTag"), inboundTags);
        rules.append(rule);
    }

    if (!rules.isEmpty()) {
        routing.insert(QStringLiteral("rules"), rules);
    }
}

QJsonObject ClientConfigWriter::buildSingBoxDns(const Config& config) const
{
    const QString remoteDns = config.remoteDns.trimmed();
    if (!remoteDns.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(remoteDns.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError
            && document.isObject()
            && document.object().contains(QStringLiteral("servers"))) {
            return document.object();
        }
    }

    const RoutingItem* selectedRouting = resolveSelectedRouting(config);
    const bool useDirectFinal = usesDirectDnsAsFinalServer(selectedRouting);

    QJsonObject dns;
    dns.insert(QStringLiteral("independent_cache"), true);

    QJsonArray servers;
    QJsonObject localDns = parseSingBoxDnsAddress(config.bootstrapDns);
    if (!localDns.isEmpty()) {
        localDns.insert(QStringLiteral("tag"), kSingBoxLocalDnsTag);
        servers.append(localDns);
    }

    QJsonObject remoteDnsServer = parseSingBoxDnsAddress(config.remoteDns);
    if (!remoteDnsServer.isEmpty()) {
        remoteDnsServer.insert(QStringLiteral("tag"), kSingBoxRemoteDnsTag);
        remoteDnsServer.insert(QStringLiteral("detour"), QStringLiteral("proxy"));
        remoteDnsServer.insert(QStringLiteral("domain_resolver"), kSingBoxLocalDnsTag);
        servers.append(remoteDnsServer);
    }

    QJsonObject directDnsServer = parseSingBoxDnsAddress(config.directDns);
    if (!directDnsServer.isEmpty()) {
        directDnsServer.insert(QStringLiteral("tag"), kSingBoxDirectDnsTag);
        directDnsServer.insert(QStringLiteral("domain_resolver"), kSingBoxLocalDnsTag);
        servers.append(directDnsServer);
    }

    const QMap<QString, QStringList> hostsMap = parseHostsToDictionary(config.dnsHosts);
    const QMap<QString, QString> systemHostsMap = config.useSystemHosts ? loadSystemHosts() : QMap<QString, QString>{};
    QJsonObject predefinedHosts;
    if (config.addCommonHosts) {
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

    QJsonObject hostsDnsServer;
    hostsDnsServer.insert(QStringLiteral("tag"), kSingBoxHostsDnsTag);
    hostsDnsServer.insert(QStringLiteral("type"), QStringLiteral("hosts"));
    hostsDnsServer.insert(QStringLiteral("predefined"), predefinedHosts);

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
    servers.append(hostsDnsServer);

    if (config.fakeIp) {
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
    dns.insert(
        QStringLiteral("final"),
        useDirectFinal || remoteDnsServer.isEmpty() ? kSingBoxDirectDnsTag : kSingBoxRemoteDnsTag);

    QJsonArray rules;
    QJsonObject hostsRule;
    hostsRule.insert(QStringLiteral("ip_accept_any"), true);
    hostsRule.insert(QStringLiteral("server"), kSingBoxHostsDnsTag);
    rules.append(hostsRule);

    if (!remoteDnsServer.isEmpty()) {
        QJsonObject proxyRule;
        proxyRule.insert(QStringLiteral("server"), kSingBoxRemoteDnsTag);
        proxyRule.insert(QStringLiteral("clash_mode"), QStringLiteral("Global"));
        const QString proxyStrategy = mapDomainStrategyToSingBox(config.domainStrategyForProxy);
        if (!proxyStrategy.isEmpty()) {
            proxyRule.insert(QStringLiteral("strategy"), proxyStrategy);
        }
        rules.append(proxyRule);
    }

    if (!directDnsServer.isEmpty()) {
        QJsonObject directRule;
        directRule.insert(QStringLiteral("server"), kSingBoxDirectDnsTag);
        directRule.insert(QStringLiteral("clash_mode"), QStringLiteral("Direct"));
        const QString directStrategy = mapDomainStrategyToSingBox(config.domainStrategyForFreedom);
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

    if (config.blockBindingQuery) {
        QJsonObject blockBindingRule;
        blockBindingRule.insert(QStringLiteral("query_type"), QJsonArray{64, 65});
        blockBindingRule.insert(QStringLiteral("action"), QStringLiteral("predefined"));
        blockBindingRule.insert(QStringLiteral("rcode"), QStringLiteral("NOERROR"));
        rules.append(blockBindingRule);
    }

    if (config.fakeIp && config.globalFakeIp) {
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
    for (const QString& item : config.directExpectedIps.split(QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts)) {
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

    if (selectedRouting != nullptr) {
        const QString directStrategy = mapDomainStrategyToSingBox(config.domainStrategyForFreedom);
        const QString proxyStrategy = mapDomainStrategyToSingBox(config.domainStrategyForProxy);

        for (const RoutingRule& sourceRule : selectedRouting->rules) {
            if (!sourceRule.enabled) {
                continue;
            }

            const QStringList domains = normalizeRuleValues(sourceRule.domain, true, true);
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
                if (config.fakeIp && !config.globalFakeIp) {
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

    if (config.fakeIp && !config.globalFakeIp && !useDirectFinal) {
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

QJsonObject ClientConfigWriter::buildSingBoxRoute(const Config& config) const
{
    QJsonObject route;
    route.insert(QStringLiteral("final"), QStringLiteral("proxy"));
    const bool customDnsObject = isCustomDnsObjectText(config.remoteDns);
    if (!customDnsObject) {
        QJsonObject defaultDomainResolver;
        defaultDomainResolver.insert(QStringLiteral("server"), kSingBoxDirectDnsTag);
        const QString directDnsStrategy = mapDomainStrategyToSingBox(config.domainStrategyForFreedom);
        if (!directDnsStrategy.isEmpty()) {
            defaultDomainResolver.insert(QStringLiteral("strategy"), directDnsStrategy);
        }
        route.insert(QStringLiteral("default_domain_resolver"), defaultDomainResolver);
    }

    QJsonArray rules;
    if (config.tunModeItem.enableTun) {
        route.insert(QStringLiteral("auto_detect_interface"), true);
        rules = buildTunCompatRejectRules();
        appendTunCompatProcessRules(rules);
        appendTunIcmpRouteRule(rules, config.tunModeItem);
    }
    appendSingBoxSniffRules(rules, config);

    if (!customDnsObject) {
        QJsonObject hostsResolveRule;
        hostsResolveRule.insert(QStringLiteral("action"), QStringLiteral("resolve"));
        int hostsMatchCount = 0;
        const QMap<QString, QStringList> hostsMap = parseHostsToDictionary(config.dnsHosts);
        const QMap<QString, QString> systemHostsMap = config.useSystemHosts ? loadSystemHosts() : QMap<QString, QString>{};
        for (auto it = systemHostsMap.constBegin(); it != systemHostsMap.constEnd(); ++it) {
            if (appendSingBoxDomainField(hostsResolveRule, it.key(), true)) {
                ++hostsMatchCount;
            }
        }
        for (auto it = hostsMap.constBegin(); it != hostsMap.constEnd(); ++it) {
            if (appendSingBoxDomainField(hostsResolveRule, it.key(), true)) {
                ++hostsMatchCount;
            }
        }
        if (hostsMatchCount > 0) {
            rules.append(hostsResolveRule);
        }
    }

    QJsonObject directModeRule;
    directModeRule.insert(QStringLiteral("outbound"), QStringLiteral("direct"));
    directModeRule.insert(QStringLiteral("clash_mode"), QStringLiteral("Direct"));
    rules.append(directModeRule);

    QJsonObject globalModeRule;
    globalModeRule.insert(QStringLiteral("outbound"), QStringLiteral("proxy"));
    globalModeRule.insert(QStringLiteral("clash_mode"), QStringLiteral("Global"));
    rules.append(globalModeRule);

    const RoutingItem* selectedRouting = resolveSelectedRouting(config);
    QString singBoxDomainStrategy = config.domainStrategy4Singbox.trimmed();
    if (selectedRouting != nullptr && !selectedRouting->domainStrategy4Singbox.trimmed().isEmpty()) {
        singBoxDomainStrategy = selectedRouting->domainStrategy4Singbox.trimmed();
    }

    auto appendResolveRule = [&rules, &singBoxDomainStrategy]() {
        QJsonObject resolveRule;
        resolveRule.insert(QStringLiteral("action"), QStringLiteral("resolve"));
        if (!singBoxDomainStrategy.isEmpty()) {
            resolveRule.insert(QStringLiteral("strategy"), singBoxDomainStrategy);
        }
        rules.append(resolveRule);
    };

    const QString legacyDomainStrategy = config.domainStrategy.trimmed();
    const bool reapplyIpRulesAfterResolve = legacyDomainStrategy.compare(QStringLiteral("IPIfNonMatch"), Qt::CaseInsensitive) == 0;
    QList<RoutingRule> ipRulesAfterResolve;
    if (legacyDomainStrategy.compare(QStringLiteral("IPOnDemand"), Qt::CaseInsensitive) == 0) {
        appendResolveRule();
    }

    if (selectedRouting != nullptr) {
        for (const RoutingRule& rule : selectedRouting->rules) {
            if (!rule.enabled) {
                continue;
            }
            appendSingBoxRoutingRule(rules, rule);
            if (reapplyIpRulesAfterResolve && !normalizeRuleValues(rule.ip).isEmpty()) {
                ipRulesAfterResolve.append(rule);
            }
        }
    }

    if (reapplyIpRulesAfterResolve) {
        appendResolveRule();
        for (const RoutingRule& rule : ipRulesAfterResolve) {
            appendSingBoxRoutingIpRule(rules, rule);
        }
    }

    if (!rules.isEmpty()) {
        route.insert(QStringLiteral("rules"), rules);
    }

    return route;
}

QJsonObject ClientConfigWriter::buildTunCompatSingBoxDns() const
{
    QJsonObject server;
    server.insert(QStringLiteral("tag"), QStringLiteral("local"));
    server.insert(QStringLiteral("type"), QStringLiteral("local"));

    QJsonArray servers;
    servers.append(server);

    QJsonObject dns;
    dns.insert(QStringLiteral("servers"), servers);
    dns.insert(QStringLiteral("final"), QStringLiteral("local"));
    return dns;
}

QJsonObject ClientConfigWriter::buildTunCompatSingBoxRoute(const Config& config) const
{
    QJsonObject route;
    route.insert(QStringLiteral("final"), QStringLiteral("proxy"));
    route.insert(QStringLiteral("auto_detect_interface"), true);

    QJsonArray rules = buildTunCompatRejectRules();
    appendTunCompatProcessRules(rules);
    appendTunIcmpRouteRule(rules, config.tunModeItem);
    appendSingBoxSniffRules(rules, config);

    route.insert(QStringLiteral("rules"), rules);
    return route;
}

QJsonArray ClientConfigWriter::buildTunCompatRejectRules() const
{
    QJsonArray rules;

    QJsonObject udpRejectRule;
    udpRejectRule.insert(QStringLiteral("network"), QJsonArray{QStringLiteral("udp")});
    udpRejectRule.insert(QStringLiteral("port"), QJsonArray{135, 137, 138, 139, 5353});
    udpRejectRule.insert(QStringLiteral("action"), QStringLiteral("reject"));
    rules.append(udpRejectRule);

    QJsonObject multicastRejectRule;
    multicastRejectRule.insert(QStringLiteral("ip_cidr"), QJsonArray{
                                                          QStringLiteral("224.0.0.0/3"),
                                                          QStringLiteral("ff00::/8")});
    multicastRejectRule.insert(QStringLiteral("action"), QStringLiteral("reject"));
    rules.append(multicastRejectRule);

    return rules;
}

void ClientConfigWriter::appendTunCompatProcessRules(QJsonArray& rules) const
{
    QJsonObject dnsHijackRule;
    dnsHijackRule.insert(QStringLiteral("port"), QJsonArray{53});
    dnsHijackRule.insert(QStringLiteral("action"), QStringLiteral("hijack-dns"));
    QJsonArray dnsProcessNames;
    for (const QString& processName : buildTunCompatDnsProcessNames()) {
        dnsProcessNames.append(processName);
    }
    dnsHijackRule.insert(QStringLiteral("process_name"), dnsProcessNames);
    rules.append(dnsHijackRule);

    QJsonObject directProcessRule;
    directProcessRule.insert(QStringLiteral("outbound"), QStringLiteral("direct"));
    QJsonArray processNames;
    for (const QString& processName : buildTunCompatDirectProcessNames()) {
        processNames.append(processName);
    }
    directProcessRule.insert(QStringLiteral("process_name"), processNames);
    rules.append(directProcessRule);
}

void ClientConfigWriter::appendTunIcmpRouteRule(QJsonArray& rules, const TunModeItem& tun) const
{
    const QString policy = tun.icmpRouting.trimmed().toLower();
    const QStringList supportedPolicies{
        QStringLiteral("rule"),
        QStringLiteral("direct"),
        QStringLiteral("unreachable"),
        QStringLiteral("drop"),
        QStringLiteral("reply")};
    if (policy.isEmpty()
        || !supportedPolicies.contains(policy)
        || policy == QStringLiteral("rule")) {
        return;
    }

    QJsonObject rule;
    rule.insert(QStringLiteral("network"), QJsonArray{QStringLiteral("icmp")});

    if (policy == QStringLiteral("direct")) {
        rule.insert(QStringLiteral("outbound"), QStringLiteral("direct"));
        rules.append(rule);
        return;
    }

    rule.insert(QStringLiteral("action"), QStringLiteral("reject"));
    rule.insert(
        QStringLiteral("method"),
        policy == QStringLiteral("drop")
            ? QStringLiteral("drop")
            : policy == QStringLiteral("unreachable")
                ? QStringLiteral("default")
                : QStringLiteral("reply"));
    rules.append(rule);
}

void ClientConfigWriter::appendSingBoxSniffRules(QJsonArray& rules, const Config& config) const
{
    if (config.sniffingEnabled) {
        QJsonObject sniffRule;
        sniffRule.insert(QStringLiteral("action"), QStringLiteral("sniff"));
        rules.append(sniffRule);

        QJsonObject dnsHijackRule;
        dnsHijackRule.insert(QStringLiteral("action"), QStringLiteral("hijack-dns"));
        dnsHijackRule.insert(QStringLiteral("protocol"), QJsonArray{QStringLiteral("dns")});
        rules.append(dnsHijackRule);
        return;
    }

    QJsonObject dnsHijackRule;
    dnsHijackRule.insert(QStringLiteral("port"), QJsonArray{53});
    dnsHijackRule.insert(QStringLiteral("network"), QJsonArray{QStringLiteral("udp")});
    dnsHijackRule.insert(QStringLiteral("action"), QStringLiteral("hijack-dns"));
    rules.append(dnsHijackRule);
}

QJsonObject ClientConfigWriter::buildLog(const Config& config) const
{
    QJsonObject log;
    log.insert(QStringLiteral("loglevel"), config.logLevel.trimmed().isEmpty() ? QStringLiteral("warning") : config.logLevel);
    // Match WinForms behavior: empty paths keep access/error on the captured console output,
    // which allows the Information panel to display proxy/direct hit lines in real time.
    log.insert(QStringLiteral("access"), config.logEnabled ? kDefaultAccessLogFileName : QString());
    log.insert(QStringLiteral("error"), config.logEnabled ? kDefaultErrorLogFileName : QString());
    return log;
}

QJsonObject ClientConfigWriter::buildSingBoxLog(const Config& config) const
{
    QJsonObject log;
    log.insert(QStringLiteral("disabled"), false);
    log.insert(QStringLiteral("level"), QStringLiteral("info"));
    return log;
}

QJsonArray ClientConfigWriter::buildInbounds(const Config& config) const
{
    QJsonArray inbounds;
    inbounds.append(buildSocksInbound(config, false, 0));
    inbounds.append(buildHttpInbound(config, false, 1));

    if (config.allowLanConnection) {
        inbounds.append(buildSocksInbound(config, true, 2));
        inbounds.append(buildHttpInbound(config, true, 3));
    }

    return inbounds;
}

QJsonArray ClientConfigWriter::buildSingBoxInbounds(const Config& config) const
{
    QJsonArray inbounds;
    if (config.tunModeItem.enableTun) {
        inbounds.append(buildSingBoxTunInbound(config));
    }
    inbounds.append(buildSingBoxSocksInbound(config, false, 0));
    inbounds.append(buildSingBoxHttpInbound(config, false, 1));

    if (config.allowLanConnection) {
        inbounds.append(buildSingBoxSocksInbound(config, true, 2));
        inbounds.append(buildSingBoxHttpInbound(config, true, 3));
    }

    return inbounds;
}

QJsonArray ClientConfigWriter::buildOutbounds(const Config& config, const VmessItem& server) const
{
    QJsonArray outbounds;
    QJsonObject primaryOutbound = buildPrimaryOutbound(config, server);
    if (config.enableFragment) {
        const QJsonObject streamSettings = primaryOutbound.value(QStringLiteral("streamSettings")).toObject();
        if (!streamSettings.value(QStringLiteral("security")).toString().trimmed().isEmpty()) {
            QJsonObject updatedStreamSettings = streamSettings;
            QJsonObject sockopt = updatedStreamSettings.value(QStringLiteral("sockopt")).toObject();
            if (sockopt.value(QStringLiteral("dialerProxy")).toString().trimmed().isEmpty()) {
                sockopt.insert(QStringLiteral("dialerProxy"), QStringLiteral("frag-proxy"));
                updatedStreamSettings.insert(QStringLiteral("sockopt"), sockopt);
                primaryOutbound.insert(QStringLiteral("streamSettings"), updatedStreamSettings);
                outbounds.append(buildLegacyFragmentOutbound());
            }
        }
    }
    outbounds.append(primaryOutbound);
    outbounds.append(buildDirectOutbound(config));
    outbounds.append(buildBlackholeOutbound());
    return outbounds;
}

QJsonArray ClientConfigWriter::buildSingBoxOutbounds(const Config& config, const VmessItem& server) const
{
    QJsonArray outbounds;
    outbounds.append(buildSingBoxPrimaryOutbound(config, server));
    outbounds.append(buildSingBoxDirectOutbound());
    outbounds.append(buildSingBoxBlockOutbound());
    return outbounds;
}

QJsonObject ClientConfigWriter::buildPrimaryOutbound(const Config& config, const VmessItem& server) const
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("proxy"));
    outbound.insert(QStringLiteral("streamSettings"), buildStreamSettings(config, server));
    if (!config.domainStrategyForProxy.trimmed().isEmpty()
        && server.configType != ConfigType::Socks
        && server.configType != ConfigType::HTTP
        && server.configType != ConfigType::Custom
        && server.configType != ConfigType::Unknown
        && server.configType != ConfigType::Hysteria2
        && server.configType != ConfigType::TUIC
        && server.configType != ConfigType::WireGuard) {
        outbound.insert(QStringLiteral("targetStrategy"), config.domainStrategyForProxy.trimmed());
    }

    if (server.configType == ConfigType::Shadowsocks) {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("shadowsocks"));
        QJsonObject settings;
        QJsonArray servers;
        QJsonObject item;
        item.insert(QStringLiteral("address"), server.address);
        item.insert(QStringLiteral("port"), server.port);
        item.insert(QStringLiteral("method"), server.security.trimmed().isEmpty() ? QStringLiteral("aes-128-gcm") : server.security);
        item.insert(QStringLiteral("password"), server.id);
        servers.append(item);
        settings.insert(QStringLiteral("servers"), servers);
        outbound.insert(QStringLiteral("settings"), settings);
        return outbound;
    }

    if (server.configType == ConfigType::Socks) {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("socks"));
        QJsonObject settings;
        QJsonArray servers;
        QJsonObject item;
        item.insert(QStringLiteral("address"), server.address);
        item.insert(QStringLiteral("port"), server.port);
        if (!server.id.trimmed().isEmpty()) {
            QJsonArray users;
            QJsonObject user;
            user.insert(QStringLiteral("user"), server.id);
            user.insert(QStringLiteral("pass"), server.security);
            users.append(user);
            item.insert(QStringLiteral("users"), users);
        }
        servers.append(item);
        settings.insert(QStringLiteral("servers"), servers);
        outbound.insert(QStringLiteral("settings"), settings);
        return outbound;
    }

    if (server.configType == ConfigType::HTTP) {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("http"));
        QJsonObject settings;
        QJsonArray servers;
        QJsonObject item;
        item.insert(QStringLiteral("address"), server.address);
        item.insert(QStringLiteral("port"), server.port);
        if (!server.id.trimmed().isEmpty()) {
            QJsonArray users;
            QJsonObject user;
            user.insert(QStringLiteral("user"), server.id);
            user.insert(QStringLiteral("pass"), server.security);
            users.append(user);
            item.insert(QStringLiteral("users"), users);
        }
        servers.append(item);
        settings.insert(QStringLiteral("servers"), servers);
        outbound.insert(QStringLiteral("settings"), settings);
        return outbound;
    }

    if (server.configType == ConfigType::Trojan) {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("trojan"));
        QJsonObject settings;
        QJsonArray servers;
        QJsonObject item;
        item.insert(QStringLiteral("address"), server.address);
        item.insert(QStringLiteral("port"), server.port);
        item.insert(QStringLiteral("password"), server.id);
        if (!server.flow.trimmed().isEmpty()) {
            item.insert(QStringLiteral("flow"), server.flow);
        }
        servers.append(item);
        settings.insert(QStringLiteral("servers"), servers);
        outbound.insert(QStringLiteral("settings"), settings);
        return outbound;
    }

    if (server.configType == ConfigType::Hysteria2) {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("hysteria2"));
        QJsonObject settings;
        QJsonArray servers;
        QJsonObject item;
        item.insert(QStringLiteral("address"), server.address);
        item.insert(QStringLiteral("port"), server.port);
        item.insert(QStringLiteral("password"), server.id);
        if (!server.obfsPassword.trimmed().isEmpty()) {
            QJsonObject obfs;
            obfs.insert(QStringLiteral("type"), QStringLiteral("salamander"));
            obfs.insert(QStringLiteral("password"), server.obfsPassword);
            item.insert(QStringLiteral("obfs"), obfs);
        }
        if (!server.upMbps.trimmed().isEmpty()) {
            bool okUp = false;
            const int up = server.upMbps.toInt(&okUp);
            if (okUp && up > 0) {
                item.insert(QStringLiteral("up_mbps"), up);
            }
        }
        if (!server.downMbps.trimmed().isEmpty()) {
            bool okDown = false;
            const int down = server.downMbps.toInt(&okDown);
            if (okDown && down > 0) {
                item.insert(QStringLiteral("down_mbps"), down);
            }
        }
        servers.append(item);
        settings.insert(QStringLiteral("servers"), servers);
        outbound.insert(QStringLiteral("settings"), settings);
        return outbound;
    }

    if (server.configType == ConfigType::WireGuard) {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("wireguard"));
        QJsonObject settings;
        QJsonArray peers;
        QJsonObject peer;
        if (!server.peerPublicKey.trimmed().isEmpty()) {
            peer.insert(QStringLiteral("publicKey"), server.peerPublicKey);
        }
        if (!server.reserved.trimmed().isEmpty()) {
            QJsonArray reservedArr;
            const QStringList parts = server.reserved.split(QChar(','), Qt::SkipEmptyParts);
            for (const QString& part : parts) {
                bool okR = false;
                const int val = part.trimmed().toInt(&okR);
                if (okR) {
                    reservedArr.append(val);
                }
            }
            if (!reservedArr.isEmpty()) {
                peer.insert(QStringLiteral("reserved"), reservedArr);
            }
        }
        peer.insert(QStringLiteral("endpoint"), server.address + QStringLiteral(":") + QString::number(server.port));
        peers.append(peer);
        settings.insert(QStringLiteral("peers"), peers);
        if (!server.localAddress.trimmed().isEmpty()) {
            const QJsonArray addresses = QJsonArray::fromStringList(
                server.localAddress.split(QChar(','), Qt::SkipEmptyParts));
            settings.insert(QStringLiteral("address"), addresses);
        }
        if (!server.privateKey.trimmed().isEmpty()) {
            settings.insert(QStringLiteral("secretKey"), server.privateKey);
        }
        if (server.wireguardMtu > 0) {
            settings.insert(QStringLiteral("mtu"), server.wireguardMtu);
        }
        outbound.insert(QStringLiteral("settings"), settings);
        return outbound;
    }

    QJsonObject settings;
    QJsonArray vnext;
    QJsonObject remote;
    remote.insert(QStringLiteral("address"), server.address);
    remote.insert(QStringLiteral("port"), server.port);

    QJsonArray users;
    QJsonObject user;
    user.insert(QStringLiteral("id"), server.id);

    if (server.configType == ConfigType::VLESS) {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("vless"));
        user.insert(QStringLiteral("encryption"), QStringLiteral("none"));
        if (!server.flow.trimmed().isEmpty()) {
            user.insert(QStringLiteral("flow"), server.flow);
        }
    } else {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("vmess"));
        user.insert(QStringLiteral("alterId"), server.alterId);
        user.insert(QStringLiteral("security"), server.security.trimmed().isEmpty() ? QStringLiteral("auto") : server.security);
        const bool muxEnabled = resolveMuxEnabled(config, server);
        QJsonObject mux;
        mux.insert(QStringLiteral("enabled"), muxEnabled);
        mux.insert(QStringLiteral("concurrency"), muxEnabled ? 8 : -1);
        outbound.insert(QStringLiteral("mux"), mux);
    }

    users.append(user);
    remote.insert(QStringLiteral("users"), users);
    vnext.append(remote);
    settings.insert(QStringLiteral("vnext"), vnext);
    outbound.insert(QStringLiteral("settings"), settings);
    return outbound;
}

QJsonObject ClientConfigWriter::buildLegacyFragmentOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("frag-proxy"));
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("freedom"));

    QJsonObject settings;
    QJsonObject fragment;
    fragment.insert(QStringLiteral("packets"), QStringLiteral("tlshello"));
    fragment.insert(QStringLiteral("length"), QStringLiteral("100-200"));
    fragment.insert(QStringLiteral("interval"), QStringLiteral("10-20"));
    settings.insert(QStringLiteral("fragment"), fragment);
    outbound.insert(QStringLiteral("settings"), settings);
    return outbound;
}

QJsonObject ClientConfigWriter::buildSingBoxPrimaryOutbound(const Config& config, const VmessItem& server) const
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("proxy"));
    outbound.insert(QStringLiteral("type"), resolveSingBoxOutboundType(server.configType));
    outbound.insert(QStringLiteral("server"), server.address);
    outbound.insert(QStringLiteral("server_port"), server.port);

    // Hysteria2/TUIC use QUIC internally, WireGuard has no network concept
    const bool needsNetworkField = server.configType != ConfigType::Hysteria2
        && server.configType != ConfigType::TUIC
        && server.configType != ConfigType::WireGuard;
    if (needsNetworkField) {
        outbound.insert(QStringLiteral("network"), resolveSingBoxNetwork(server));
    }

    switch (server.configType) {
    case ConfigType::VMess:
        outbound.insert(QStringLiteral("uuid"), server.id);
        outbound.insert(QStringLiteral("security"), server.security.trimmed().isEmpty() ? QStringLiteral("auto") : server.security);
        break;
    case ConfigType::VLESS:
        outbound.insert(QStringLiteral("uuid"), server.id);
        if (!server.flow.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("flow"), server.flow);
        }
        break;
    case ConfigType::Trojan:
        outbound.insert(QStringLiteral("password"), server.id);
        break;
    case ConfigType::Shadowsocks:
        outbound.insert(QStringLiteral("method"), server.security.trimmed().isEmpty() ? QStringLiteral("aes-128-gcm") : server.security);
        outbound.insert(QStringLiteral("password"), server.id);
        break;
    case ConfigType::Socks:
        if (!server.id.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("username"), server.id);
        }
        if (!server.security.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("password"), server.security);
        }
        break;
    case ConfigType::Hysteria2:
        outbound.insert(QStringLiteral("password"), server.id);
        if (!server.obfsPassword.trimmed().isEmpty()) {
            QJsonObject obfs;
            obfs.insert(QStringLiteral("type"), QStringLiteral("salamander"));
            obfs.insert(QStringLiteral("password"), server.obfsPassword);
            outbound.insert(QStringLiteral("obfs"), obfs);
        }
        if (!server.upMbps.trimmed().isEmpty()) {
            bool okUp = false;
            const int up = server.upMbps.toInt(&okUp);
            if (okUp && up > 0) {
                outbound.insert(QStringLiteral("up_mbps"), up);
            }
        }
        if (!server.downMbps.trimmed().isEmpty()) {
            bool okDown = false;
            const int down = server.downMbps.toInt(&okDown);
            if (okDown && down > 0) {
                outbound.insert(QStringLiteral("down_mbps"), down);
            }
        }
        break;
    case ConfigType::TUIC:
        outbound.insert(QStringLiteral("uuid"), server.id);
        if (!server.security.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("password"), server.security);
        }
        if (!server.congestionControl.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("congestion_control"), server.congestionControl);
        }
        if (!server.udpRelayMode.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("udp_relay_mode"), server.udpRelayMode);
        }
        if (server.zeroRttHandshake) {
            outbound.insert(QStringLiteral("zero_rtt_handshake"), true);
        }
        break;
    case ConfigType::WireGuard: {
        if (!server.privateKey.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("private_key"), server.privateKey);
        }
        if (!server.peerPublicKey.trimmed().isEmpty()) {
            QJsonArray peersArr;
            QJsonObject peerObj;
            peerObj.insert(QStringLiteral("public_key"), server.peerPublicKey);
            if (!server.reserved.trimmed().isEmpty()) {
                QJsonArray reservedArr;
                const QStringList parts = server.reserved.split(QChar(','), Qt::SkipEmptyParts);
                for (const QString& part : parts) {
                    bool okR = false;
                    const int val = part.trimmed().toInt(&okR);
                    if (okR) {
                        reservedArr.append(val);
                    }
                }
                if (!reservedArr.isEmpty()) {
                    peerObj.insert(QStringLiteral("reserved"), reservedArr);
                }
            }
            peersArr.append(peerObj);
            outbound.insert(QStringLiteral("peers"), peersArr);
        }
        if (!server.localAddress.trimmed().isEmpty()) {
            const QStringList addresses = server.localAddress.split(QChar(','), Qt::SkipEmptyParts);
            QJsonArray addrArr;
            for (const QString& addr : addresses) {
                addrArr.append(addr.trimmed());
            }
            outbound.insert(QStringLiteral("local_address"), addrArr);
        }
        if (server.wireguardMtu > 0) {
            outbound.insert(QStringLiteral("mtu"), server.wireguardMtu);
        }
        break;
    }
    case ConfigType::AnyTLS:
        outbound.insert(QStringLiteral("password"), server.id);
        break;
    case ConfigType::Naive:
        if (!server.username.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("username"), server.username);
        }
        outbound.insert(QStringLiteral("password"), server.id);
        if (server.naiveQuic) {
            outbound.insert(QStringLiteral("quic"), true);
            if (!server.congestionControl.trimmed().isEmpty()) {
                outbound.insert(QStringLiteral("quic_congestion_control"), server.congestionControl);
            }
        }
        if (server.insecureConcurrency > 0) {
            outbound.insert(QStringLiteral("insecure_concurrency"), server.insecureConcurrency);
        }
        break;
    case ConfigType::Custom:
    case ConfigType::Unknown:
    default:
        break;
    }

    const QString singBoxMuxProtocol = config.mux4SboxProtocol.trimmed();
    if (resolveMuxEnabled(config, server) && !singBoxMuxProtocol.isEmpty()) {
        const bool supportsMultiplex = server.configType == ConfigType::VMess
            || server.configType == ConfigType::Trojan
            || server.configType == ConfigType::Shadowsocks
            || (server.configType == ConfigType::VLESS && server.flow.trimmed().isEmpty());
        if (supportsMultiplex) {
            QJsonObject multiplex;
            multiplex.insert(QStringLiteral("enabled"), true);
            multiplex.insert(QStringLiteral("protocol"), singBoxMuxProtocol);
            multiplex.insert(
                QStringLiteral("max_connections"),
                config.mux4SboxMaxConnections > 0 ? config.mux4SboxMaxConnections : 8);
            if (config.mux4SboxPadding.has_value()) {
                multiplex.insert(QStringLiteral("padding"), config.mux4SboxPadding.value());
            }
            outbound.insert(QStringLiteral("multiplex"), multiplex);
        }
    }

    const bool needsTls = server.configType != ConfigType::WireGuard;
    if (needsTls) {
        const QJsonObject tls = buildSingBoxTls(config, server);
        if (!tls.isEmpty()) {
            outbound.insert(QStringLiteral("tls"), tls);
        }
    }

    // Hysteria2 and TUIC use QUIC internally, WireGuard has no transport layer
    const bool needsTransport = server.configType != ConfigType::Hysteria2
        && server.configType != ConfigType::TUIC
        && server.configType != ConfigType::WireGuard;
    if (needsTransport) {
        const QJsonObject transport = buildSingBoxTransport(server);
        if (!transport.isEmpty()) {
            outbound.insert(QStringLiteral("transport"), transport);
        }
    }

    return outbound;
}

QJsonObject ClientConfigWriter::buildStreamSettings(const Config& config, const VmessItem& server) const
{
    QJsonObject streamSettings;
    const QString network = server.network.trimmed().isEmpty() ? QStringLiteral("tcp") : server.network.trimmed();
    const QString transportSecurity = server.streamSecurity.trimmed();
    const QString userAgent = resolveLegacyUserAgent(config, server);
    const QString fingerprint = resolveFingerprint(config, server);
    streamSettings.insert(QStringLiteral("network"), network);

    if (!transportSecurity.isEmpty()) {
        streamSettings.insert(QStringLiteral("security"), transportSecurity);
    }

    if (network.compare(QStringLiteral("kcp"), Qt::CaseInsensitive) == 0) {
        QJsonObject kcpSettings;
        QJsonObject header;
        header.insert(
            QStringLiteral("type"),
            server.headerType.trimmed().isEmpty() ? QStringLiteral("none") : server.headerType.trimmed());
        kcpSettings.insert(QStringLiteral("header"), header);
        if (!server.path.trimmed().isEmpty()) {
            kcpSettings.insert(QStringLiteral("seed"), server.path.trimmed());
        }
        streamSettings.insert(QStringLiteral("kcpSettings"), kcpSettings);
    } else if (network.compare(QStringLiteral("quic"), Qt::CaseInsensitive) == 0) {
        QJsonObject quicSettings;
        quicSettings.insert(
            QStringLiteral("security"),
            server.requestHost.trimmed().isEmpty() ? QStringLiteral("none") : server.requestHost.trimmed());
        quicSettings.insert(QStringLiteral("key"), server.path.trimmed());
        QJsonObject header;
        header.insert(
            QStringLiteral("type"),
            server.headerType.trimmed().isEmpty() ? QStringLiteral("none") : server.headerType.trimmed());
        quicSettings.insert(QStringLiteral("header"), header);
        streamSettings.insert(QStringLiteral("quicSettings"), quicSettings);
    } else if (network.compare(QStringLiteral("ws"), Qt::CaseInsensitive) == 0) {
        QJsonObject wsSettings;
        if (!server.path.trimmed().isEmpty()) {
            wsSettings.insert(QStringLiteral("path"), server.path);
        }
        QJsonObject headers;
        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            headers.insert(QStringLiteral("Host"), hosts.constFirst());
        }
        if (!userAgent.isEmpty()) {
            headers.insert(QStringLiteral("User-Agent"), userAgent);
        }
        if (!headers.isEmpty()) {
            wsSettings.insert(QStringLiteral("headers"), headers);
        }
        streamSettings.insert(QStringLiteral("wsSettings"), wsSettings);
    } else if (network.compare(QStringLiteral("grpc"), Qt::CaseInsensitive) == 0) {
        QJsonObject grpcSettings;
        grpcSettings.insert(QStringLiteral("serviceName"), server.path);
        grpcSettings.insert(QStringLiteral("multiMode"), server.headerType.compare(QStringLiteral("multi"), Qt::CaseInsensitive) == 0);
        streamSettings.insert(QStringLiteral("grpcSettings"), grpcSettings);
    } else if (network.compare(QStringLiteral("h2"), Qt::CaseInsensitive) == 0) {
        QJsonObject httpSettings;
        httpSettings.insert(QStringLiteral("path"), server.path);
        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonArray hostArray;
            for (const QString& host : hosts) {
                hostArray.append(host);
            }
            httpSettings.insert(QStringLiteral("host"), hostArray);
        }
        streamSettings.insert(QStringLiteral("httpSettings"), httpSettings);
    } else if (network.compare(QStringLiteral("httpupgrade"), Qt::CaseInsensitive) == 0) {
        QJsonObject httpupgradeSettings;
        if (!server.path.trimmed().isEmpty()) {
            httpupgradeSettings.insert(QStringLiteral("path"), server.path);
        }
        if (!server.requestHost.trimmed().isEmpty()) {
            httpupgradeSettings.insert(QStringLiteral("host"), server.requestHost);
        }
        if (!userAgent.isEmpty()) {
            QJsonObject headers;
            headers.insert(QStringLiteral("User-Agent"), userAgent);
            httpupgradeSettings.insert(QStringLiteral("headers"), headers);
        }
        streamSettings.insert(QStringLiteral("httpupgradeSettings"), httpupgradeSettings);
    } else if (network.compare(QStringLiteral("xhttp"), Qt::CaseInsensitive) == 0) {
        streamSettings.insert(QStringLiteral("network"), QStringLiteral("xhttp"));
        QJsonObject xhttpSettings;
        if (!server.path.trimmed().isEmpty()) {
            xhttpSettings.insert(QStringLiteral("path"), server.path);
        }
        if (!server.requestHost.trimmed().isEmpty()) {
            xhttpSettings.insert(QStringLiteral("host"), server.requestHost);
        }
        if (!server.headerType.trimmed().isEmpty()) {
            xhttpSettings.insert(QStringLiteral("mode"), server.headerType);
        }
        if (!server.extra.trimmed().isEmpty()) {
            QJsonParseError parseError;
            const QJsonDocument extraDoc = QJsonDocument::fromJson(server.extra.toUtf8(), &parseError);
            if (parseError.error == QJsonParseError::NoError && extraDoc.isObject()) {
                xhttpSettings.insert(QStringLiteral("extra"), extraDoc.object());
            }
        }
        streamSettings.insert(QStringLiteral("xhttpSettings"), xhttpSettings);
    } else if (network.compare(QStringLiteral("tcp"), Qt::CaseInsensitive) == 0
        && server.headerType.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0) {
        QJsonObject tcpSettings;
        QJsonObject header;
        header.insert(QStringLiteral("type"), QStringLiteral("http"));

        QJsonObject request;
        QJsonArray paths;
        const QStringList rawPaths = splitCsv(server.path);
        if (rawPaths.isEmpty()) {
            paths.append(QStringLiteral("/"));
        } else {
            for (const QString& path : rawPaths) {
                paths.append(path);
            }
        }
        request.insert(QStringLiteral("path"), paths);

        QJsonObject headers;
        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonArray hostArray;
            for (const QString& host : hosts) {
                hostArray.append(host);
            }
            headers.insert(QStringLiteral("Host"), hostArray);
        }
        if (!userAgent.isEmpty()) {
            headers.insert(QStringLiteral("User-Agent"), QJsonArray{userAgent});
        }
        if (!headers.isEmpty()) {
            request.insert(QStringLiteral("headers"), headers);
        }

        header.insert(QStringLiteral("request"), request);
        tcpSettings.insert(QStringLiteral("header"), header);
        streamSettings.insert(QStringLiteral("tcpSettings"), tcpSettings);
    }

    if (!server.finalmask.trimmed().isEmpty()) {
        const QJsonDocument finalmaskDocument = QJsonDocument::fromJson(server.finalmask.toUtf8());
        if (finalmaskDocument.isObject()) {
            streamSettings.insert(QStringLiteral("finalmask"), finalmaskDocument.object());
        }
    }

    if (isRealityTransport(transportSecurity)) {
        QJsonObject realitySettings;
        realitySettings.insert(QStringLiteral("serverName"), resolveServerName(server));
        realitySettings.insert(QStringLiteral("fingerprint"), resolveRealityFingerprint(config, server));
        realitySettings.insert(QStringLiteral("password"), server.publicKey.trimmed());
        realitySettings.insert(QStringLiteral("shortId"), server.shortId.trimmed());
        realitySettings.insert(QStringLiteral("spiderX"), server.spiderX.trimmed());
        if (!server.mldsa65Verify.trimmed().isEmpty()) {
            realitySettings.insert(QStringLiteral("mldsa65Verify"), server.mldsa65Verify.trimmed());
        }
        streamSettings.insert(QStringLiteral("realitySettings"), realitySettings);
    } else if (transportSecurity.compare(QStringLiteral("tls"), Qt::CaseInsensitive) == 0
        || transportSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) == 0) {
        QJsonObject tlsSettings;
        const QStringList certificates = splitPemCertificates(server.cert);
        tlsSettings.insert(QStringLiteral("serverName"), resolveServerName(server));
        tlsSettings.insert(
            QStringLiteral("allowInsecure"),
            resolveAllowInsecure(server.allowInsecure, config.defaultAllowInsecure));
        if (!fingerprint.isEmpty()) {
            tlsSettings.insert(QStringLiteral("fingerprint"), fingerprint);
        }
        if (!server.echConfigList.trimmed().isEmpty()) {
            tlsSettings.insert(QStringLiteral("echConfigList"), server.echConfigList.trimmed());
        }
        if (!server.echForceQuery.trimmed().isEmpty()) {
            tlsSettings.insert(QStringLiteral("echForceQuery"), server.echForceQuery.trimmed());
        }
        if (!certificates.isEmpty()) {
            tlsSettings.insert(QStringLiteral("certificates"), buildLegacyTlsCertificates(certificates));
            tlsSettings.insert(QStringLiteral("disableSystemRoot"), true);
            tlsSettings.insert(QStringLiteral("allowInsecure"), false);
        } else if (!server.certSha.trimmed().isEmpty()) {
            tlsSettings.insert(QStringLiteral("pinnedPeerCertSha256"), server.certSha.trimmed());
            tlsSettings.insert(QStringLiteral("allowInsecure"), false);
        }

        if (!server.alpn.isEmpty()) {
            QJsonArray alpnArray;
            for (const QString& item : server.alpn) {
                alpnArray.append(item);
            }
            tlsSettings.insert(QStringLiteral("alpn"), alpnArray);
        }

        if (transportSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) == 0) {
            streamSettings.insert(QStringLiteral("xtlsSettings"), tlsSettings);
        } else {
            streamSettings.insert(QStringLiteral("tlsSettings"), tlsSettings);
        }
    }

    return streamSettings;
}

QJsonObject ClientConfigWriter::buildSingBoxTls(const Config& config, const VmessItem& server) const
{
    const QString transportSecurity = server.streamSecurity.trimmed();
    if (transportSecurity.isEmpty()
        || (transportSecurity.compare(QStringLiteral("tls"), Qt::CaseInsensitive) != 0
            && transportSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) != 0
            && !isRealityTransport(transportSecurity))) {
        return {};
    }

    QJsonObject tls;
    tls.insert(QStringLiteral("enabled"), true);
    tls.insert(QStringLiteral("insecure"), resolveAllowInsecure(server.allowInsecure, config.defaultAllowInsecure));

    const QString serverName = resolveServerName(server).trimmed();
    if (!serverName.isEmpty()) {
        tls.insert(QStringLiteral("server_name"), serverName);
    }

    if (!server.alpn.isEmpty()) {
        QJsonArray alpnArray;
        for (const QString& item : server.alpn) {
            if (!item.trimmed().isEmpty()) {
                alpnArray.append(item.trimmed());
            }
        }
        if (!alpnArray.isEmpty()) {
            tls.insert(QStringLiteral("alpn"), alpnArray);
        }
    }

    const QStringList certificates = splitPemCertificates(server.cert);
    if (!certificates.isEmpty() && !isRealityTransport(transportSecurity)) {
        QJsonArray certificateArray;
        for (const QString& certificate : certificates) {
            certificateArray.append(certificate);
        }
        tls.insert(QStringLiteral("certificate"), certificateArray);
        tls.insert(QStringLiteral("insecure"), false);
    }

    if (isRealityTransport(transportSecurity)) {
        QJsonObject utls;
        utls.insert(QStringLiteral("enabled"), true);
        utls.insert(QStringLiteral("fingerprint"), resolveRealityFingerprint(config, server));
        tls.insert(QStringLiteral("utls"), utls);

        QJsonObject reality;
        reality.insert(QStringLiteral("enabled"), true);
        reality.insert(QStringLiteral("public_key"), server.publicKey.trimmed());
        reality.insert(QStringLiteral("short_id"), server.shortId.trimmed());
        tls.insert(QStringLiteral("reality"), reality);
        tls.insert(QStringLiteral("insecure"), false);
    } else {
        const QString fingerprint = resolveFingerprint(config, server);
        if (!fingerprint.isEmpty()) {
        QJsonObject utls;
        utls.insert(QStringLiteral("enabled"), true);
        utls.insert(QStringLiteral("fingerprint"), fingerprint);
        tls.insert(QStringLiteral("utls"), utls);
        }
    }

    return tls;
}

QJsonObject ClientConfigWriter::buildSingBoxTransport(const VmessItem& server) const
{
    const QString network = server.network.trimmed().isEmpty() ? QStringLiteral("tcp") : server.network.trimmed();
    QJsonObject transport;

    if (network.compare(QStringLiteral("ws"), Qt::CaseInsensitive) == 0) {
        transport.insert(QStringLiteral("type"), QStringLiteral("ws"));
        QString wsPath = server.path.trimmed();
        if (!wsPath.isEmpty()) {
            const int querySeparator = wsPath.indexOf(QLatin1Char('?'));
            if (querySeparator >= 0) {
                const QString basePath = wsPath.left(querySeparator);
                QUrlQuery query(wsPath.mid(querySeparator + 1));

                bool hasEarlyData = false;
                const int maxEarlyData = query.queryItemValue(QStringLiteral("ed")).toInt(&hasEarlyData);
                if (hasEarlyData) {
                    transport.insert(QStringLiteral("max_early_data"), maxEarlyData);
                    transport.insert(QStringLiteral("early_data_header_name"), QStringLiteral("Sec-WebSocket-Protocol"));
                    query.removeAllQueryItems(QStringLiteral("ed"));
                }

                const QString earlyDataHeader = query.queryItemValue(QStringLiteral("eh"));
                if (!earlyDataHeader.isEmpty()) {
                    transport.insert(QStringLiteral("early_data_header_name"), earlyDataHeader);
                }

                wsPath = basePath;
                const QString encodedQuery = query.toString(QUrl::FullyEncoded);
                if (!encodedQuery.isEmpty()) {
                    wsPath += QStringLiteral("?") + encodedQuery;
                }
            }
        }

        if (!wsPath.isEmpty()) {
            transport.insert(QStringLiteral("path"), wsPath);
        }

        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonObject headers;
            headers.insert(QStringLiteral("Host"), hosts.constFirst());
            transport.insert(QStringLiteral("headers"), headers);
        }

        return transport;
    }

    if (network.compare(QStringLiteral("kcp"), Qt::CaseInsensitive) == 0) {
        return {};
    }

    if (network.compare(QStringLiteral("quic"), Qt::CaseInsensitive) == 0) {
        transport.insert(QStringLiteral("type"), QStringLiteral("quic"));
        return transport;
    }

    if (network.compare(QStringLiteral("grpc"), Qt::CaseInsensitive) == 0) {
        transport.insert(QStringLiteral("type"), QStringLiteral("grpc"));
        if (!server.path.trimmed().isEmpty()) {
            transport.insert(QStringLiteral("service_name"), server.path.trimmed());
        }
        return transport;
    }

    if (network.compare(QStringLiteral("httpupgrade"), Qt::CaseInsensitive) == 0) {
        transport.insert(QStringLiteral("type"), QStringLiteral("httpupgrade"));
        if (!server.path.trimmed().isEmpty()) {
            transport.insert(QStringLiteral("path"), server.path.trimmed());
        }
        if (!server.requestHost.trimmed().isEmpty()) {
            transport.insert(QStringLiteral("host"), server.requestHost.trimmed());
        }
        return transport;
    }

    if (network.compare(QStringLiteral("h2"), Qt::CaseInsensitive) == 0
        || (network.compare(QStringLiteral("tcp"), Qt::CaseInsensitive) == 0
            && server.headerType.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0)) {
        transport.insert(QStringLiteral("type"), QStringLiteral("http"));

        const QString path = resolvePrimaryPath(server.path);
        if (!path.isEmpty()) {
            transport.insert(QStringLiteral("path"), path);
        }

        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonArray hostArray;
            for (const QString& host : hosts) {
                hostArray.append(host);
            }
            transport.insert(QStringLiteral("host"), hostArray);
        }

        return transport;
    }

    return {};
}

QJsonObject ClientConfigWriter::buildSocksInbound(const Config& config, bool allowLan, int offset)
{
    QJsonObject inbound;
    inbound.insert(QStringLiteral("tag"), allowLan ? QStringLiteral("socks-lan") : QStringLiteral("socks"));
    inbound.insert(QStringLiteral("port"), config.localPort + offset);
    inbound.insert(QStringLiteral("listen"), allowLan ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1"));
    inbound.insert(QStringLiteral("protocol"), QStringLiteral("socks"));

    QJsonObject settings;
    settings.insert(QStringLiteral("auth"), QStringLiteral("noauth"));
    settings.insert(QStringLiteral("udp"), config.udpEnabled);
    settings.insert(QStringLiteral("allowTransparent"), false);

    if (allowLan && !config.inboundUser.trimmed().isEmpty() && !config.inboundPassword.trimmed().isEmpty()) {
        settings.insert(QStringLiteral("auth"), QStringLiteral("password"));
        QJsonArray accounts;
        QJsonObject account;
        account.insert(QStringLiteral("user"), config.inboundUser);
        account.insert(QStringLiteral("pass"), config.inboundPassword);
        accounts.append(account);
        settings.insert(QStringLiteral("accounts"), accounts);
    }

    inbound.insert(QStringLiteral("settings"), settings);
    inbound.insert(QStringLiteral("sniffing"), buildSniffing(config));
    return inbound;
}

QJsonObject ClientConfigWriter::buildHttpInbound(const Config& config, bool allowLan, int offset)
{
    QJsonObject inbound;
    inbound.insert(QStringLiteral("tag"), allowLan ? QStringLiteral("http-lan") : QStringLiteral("http"));
    inbound.insert(QStringLiteral("port"), config.localPort + offset);
    inbound.insert(QStringLiteral("listen"), allowLan ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1"));
    inbound.insert(QStringLiteral("protocol"), QStringLiteral("http"));

    QJsonObject settings;
    if (allowLan && !config.inboundUser.trimmed().isEmpty() && !config.inboundPassword.trimmed().isEmpty()) {
        settings.insert(QStringLiteral("auth"), QStringLiteral("password"));
        QJsonArray accounts;
        QJsonObject account;
        account.insert(QStringLiteral("user"), config.inboundUser);
        account.insert(QStringLiteral("pass"), config.inboundPassword);
        accounts.append(account);
        settings.insert(QStringLiteral("accounts"), accounts);
    }

    inbound.insert(QStringLiteral("settings"), settings);
    inbound.insert(QStringLiteral("sniffing"), buildSniffing(config));
    return inbound;
}

QJsonObject ClientConfigWriter::buildSniffing(const Config& config)
{
    QJsonObject sniffing;
    sniffing.insert(QStringLiteral("enabled"), config.sniffingEnabled);
    sniffing.insert(QStringLiteral("routeOnly"), config.routeOnly);
    QJsonArray destOverride;
    destOverride.append(QStringLiteral("http"));
    destOverride.append(QStringLiteral("tls"));
    sniffing.insert(QStringLiteral("destOverride"), destOverride);
    return sniffing;
}

QJsonObject ClientConfigWriter::buildSingBoxSocksInbound(const Config& config, bool allowLan, int offset)
{
    QJsonObject inbound;
    inbound.insert(QStringLiteral("type"), QStringLiteral("socks"));
    inbound.insert(QStringLiteral("tag"), allowLan ? QStringLiteral("socks-lan") : QStringLiteral("socks"));
    inbound.insert(QStringLiteral("listen"), allowLan ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1"));
    inbound.insert(QStringLiteral("listen_port"), config.localPort + offset);

    if (allowLan && !config.inboundUser.trimmed().isEmpty() && !config.inboundPassword.trimmed().isEmpty()) {
        QJsonArray users;
        QJsonObject user;
        user.insert(QStringLiteral("username"), config.inboundUser);
        user.insert(QStringLiteral("password"), config.inboundPassword);
        users.append(user);
        inbound.insert(QStringLiteral("users"), users);
    }

    return inbound;
}

QJsonObject ClientConfigWriter::buildSingBoxHttpInbound(const Config& config, bool allowLan, int offset)
{
    QJsonObject inbound;
    inbound.insert(QStringLiteral("type"), QStringLiteral("http"));
    inbound.insert(QStringLiteral("tag"), allowLan ? QStringLiteral("http-lan") : QStringLiteral("http"));
    inbound.insert(QStringLiteral("listen"), allowLan ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1"));
    inbound.insert(QStringLiteral("listen_port"), config.localPort + offset);

    if (allowLan && !config.inboundUser.trimmed().isEmpty() && !config.inboundPassword.trimmed().isEmpty()) {
        QJsonArray users;
        QJsonObject user;
        user.insert(QStringLiteral("username"), config.inboundUser);
        user.insert(QStringLiteral("password"), config.inboundPassword);
        users.append(user);
        inbound.insert(QStringLiteral("users"), users);
    }

    return inbound;
}

QJsonObject ClientConfigWriter::buildDirectOutbound(const Config& config)
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("direct"));
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("freedom"));

    QJsonObject settings;
    if (!config.domainStrategyForFreedom.trimmed().isEmpty()) {
        settings.insert(QStringLiteral("domainStrategy"), config.domainStrategyForFreedom.trimmed());
        settings.insert(QStringLiteral("userLevel"), 0);
    }

    outbound.insert(QStringLiteral("settings"), settings);
    return outbound;
}

QJsonObject ClientConfigWriter::buildBlackholeOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("block"));
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("blackhole"));
    outbound.insert(QStringLiteral("settings"), QJsonObject());
    return outbound;
}

QJsonObject ClientConfigWriter::buildSingBoxDirectOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("direct"));
    outbound.insert(QStringLiteral("type"), QStringLiteral("direct"));
    return outbound;
}

QJsonObject ClientConfigWriter::buildSingBoxBlockOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("block"));
    outbound.insert(QStringLiteral("type"), QStringLiteral("block"));
    return outbound;
}

QJsonArray ClientConfigWriter::buildTunCompatSingBoxOutbounds(const Config& config) const
{
    QJsonArray outbounds;
    outbounds.append(buildSingBoxSocksOutbound(
        QStringLiteral("proxy"),
        QStringLiteral("127.0.0.1"),
        config.localPort));
    outbounds.append(buildSingBoxDirectOutbound());
    outbounds.append(buildSingBoxBlockOutbound());
    return outbounds;
}

QJsonObject ClientConfigWriter::buildSingBoxTunInbound(const Config& config)
{
    const TunModeItem& tun = config.tunModeItem;
    QJsonObject inbound;
    inbound.insert(QStringLiteral("type"), QStringLiteral("tun"));
    inbound.insert(QStringLiteral("tag"), QStringLiteral("tun-in"));
    inbound.insert(QStringLiteral("interface_name"), QStringLiteral("singbox_tun"));

    QJsonArray addresses;
    if (tun.enableIPv6Address) {
        addresses.append(QStringLiteral("fdfe:dcba:9876::1/126"));
    }
    addresses.append(QStringLiteral("172.18.0.1/30"));
    inbound.insert(QStringLiteral("address"), addresses);

    inbound.insert(QStringLiteral("mtu"), tun.mtu > 0 ? tun.mtu : 9000);
    inbound.insert(QStringLiteral("auto_route"), tun.autoRoute);
    inbound.insert(QStringLiteral("strict_route"), tun.strictRoute);
    inbound.insert(QStringLiteral("stack"), tun.stack.isEmpty() ? QStringLiteral("system") : tun.stack);

    return inbound;
}

QJsonObject ClientConfigWriter::buildSingBoxSocksOutbound(
    const QString& tag,
    const QString& server,
    int port,
    bool udpOverTcp)
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), tag);
    outbound.insert(QStringLiteral("type"), QStringLiteral("socks"));
    outbound.insert(QStringLiteral("server"), server);
    outbound.insert(QStringLiteral("server_port"), port);
    if (udpOverTcp) {
        outbound.insert(QStringLiteral("udp_over_tcp"), true);
    }
    return outbound;
}

const RoutingItem* ClientConfigWriter::resolveSelectedRouting(const Config& config)
{
    if (config.enableRoutingAdvanced) {
        if (config.routingIndex >= 0 && config.routingIndex < config.routingItems.size()) {
            return &config.routingItems.at(config.routingIndex);
        }
        return nullptr;
    }

    for (const RoutingItem& item : config.routingItems) {
        if (item.locked) {
            return &item;
        }
    }

    return nullptr;
}

void ClientConfigWriter::appendLegacyRoutingRule(QJsonArray& rules, const RoutingRule& source)
{
    const QStringList domains = normalizeRuleValues(source.domain, true, true);
    const QStringList ips = normalizeRuleValues(source.ip);
    const QStringList protocols = normalizeRuleValues(source.protocol);
    const QStringList processes = normalizeRuleValues(source.process);
    const QStringList inboundTags = normalizeRuleValues(source.inboundTag);
    const QString port = source.port.trimmed().isEmpty() ? QString() : source.port.trimmed();
    const QString network = source.network.trimmed();
    const bool hasDomainOrIpOrProcess = !domains.isEmpty() || !ips.isEmpty() || !processes.isEmpty();

    if (!domains.isEmpty()) {
        rules.append(createLegacyRoutingRule(source, inboundTags, port, domains, {}, protocols, {}));
    }

    if (!ips.isEmpty()) {
        rules.append(createLegacyRoutingRule(source, inboundTags, port, {}, ips, protocols, {}));
    }

    if (!processes.isEmpty()) {
        rules.append(createLegacyRoutingRule(source, inboundTags, port, {}, {}, protocols, processes));
    }

    if (!hasDomainOrIpOrProcess
        && (!port.isEmpty() || !network.isEmpty() || !protocols.isEmpty() || !inboundTags.isEmpty())) {
        rules.append(createLegacyRoutingRule(source, inboundTags, port, {}, {}, protocols, {}));
    }
}

void ClientConfigWriter::appendSingBoxRoutingRule(QJsonArray& rules, const RoutingRule& source)
{
    const QStringList domains = normalizeRuleValues(source.domain, true, true);
    const QStringList ips = normalizeRuleValues(source.ip);
    const QStringList protocols = normalizeRuleValues(source.protocol);
    const QStringList processes = normalizeRuleValues(source.process);
    const QStringList inboundTags = normalizeRuleValues(source.inboundTag);
    const QString port = source.port.trimmed().isEmpty() ? QString() : source.port.trimmed();
    const QString network = source.network.trimmed();
    const bool hasDomainOrIpOrProcess = !domains.isEmpty() || !ips.isEmpty() || !processes.isEmpty();

    if (!domains.isEmpty()) {
        rules.append(createSingBoxRoutingRule(source, inboundTags, port, domains, {}, protocols));
    }

    if (!ips.isEmpty()) {
        rules.append(createSingBoxRoutingRule(source, inboundTags, port, {}, ips, protocols));
    }

    if (!processes.isEmpty()) {
        const QJsonObject processRule = createSingBoxRoutingRule(source, inboundTags, port, {}, {}, protocols);

        QJsonObject processNameRule = processRule;
        QJsonArray processNames;
        QJsonObject processPathRule = processRule;
        QJsonArray processPaths;

        for (const QString& process : processes) {
            if (process.compare(QStringLiteral("self/"), Qt::CaseInsensitive) == 0
                || process.compare(QStringLiteral("xray/"), Qt::CaseInsensitive) == 0) {
                processNames.append(QStringLiteral("sing-box.exe"));
                continue;
            }

            if (process.contains(QChar('/')) || process.contains(QChar('\\'))) {
                QString normalizedPath = process;
                normalizedPath.replace(QChar('/'), QChar('\\'));
                processPaths.append(normalizedPath);
                continue;
            }

            QString processName = process;
            if (!processName.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
                processName.append(QStringLiteral(".exe"));
            }
            processNames.append(processName);
        }

        if (!processNames.isEmpty()) {
            processNameRule.insert(QStringLiteral("process_name"), processNames);
            rules.append(processNameRule);
        }
        if (!processPaths.isEmpty()) {
            processPathRule.insert(QStringLiteral("process_path"), processPaths);
            rules.append(processPathRule);
        }
    }

    if (!hasDomainOrIpOrProcess
        && (!port.isEmpty() || !network.isEmpty() || !protocols.isEmpty() || !inboundTags.isEmpty())) {
        rules.append(createSingBoxRoutingRule(source, inboundTags, port, {}, {}, protocols));
    }
}

void ClientConfigWriter::appendSingBoxRoutingIpRule(QJsonArray& rules, const RoutingRule& source)
{
    const QStringList ips = normalizeRuleValues(source.ip);
    if (ips.isEmpty()) {
        return;
    }

    const QStringList protocols = normalizeRuleValues(source.protocol);
    const QStringList inboundTags = normalizeRuleValues(source.inboundTag);
    const QString port = source.port.trimmed().isEmpty() ? QString() : source.port.trimmed();
    rules.append(createSingBoxRoutingRule(source, inboundTags, port, {}, ips, protocols));
}

QJsonObject ClientConfigWriter::createLegacyRoutingRule(
    const RoutingRule& source,
    const QStringList& inboundTags,
    const QString& port,
    const QStringList& domains,
    const QStringList& ips,
    const QStringList& protocols,
    const QStringList& processes)
{
    QJsonObject rule;
    rule.insert(QStringLiteral("type"), QStringLiteral("field"));

    if (!inboundTags.isEmpty()) {
        QJsonArray inboundTagArray;
        for (const QString& item : inboundTags) {
            inboundTagArray.append(item);
        }
        rule.insert(QStringLiteral("inboundTag"), inboundTagArray);
    }

    if (!source.outboundTag.trimmed().isEmpty()) {
        rule.insert(QStringLiteral("outboundTag"), source.outboundTag.trimmed());
    }

    if (!port.isEmpty()) {
        rule.insert(QStringLiteral("port"), port);
    }

    if (!source.network.trimmed().isEmpty()) {
        rule.insert(QStringLiteral("network"), source.network.trimmed());
    }

    if (!domains.isEmpty()) {
        QJsonArray domainArray;
        for (const QString& item : domains) {
            domainArray.append(item);
        }
        rule.insert(QStringLiteral("domain"), domainArray);
    }

    if (!ips.isEmpty()) {
        QJsonArray ipArray;
        for (const QString& item : ips) {
            ipArray.append(item);
        }
        rule.insert(QStringLiteral("ip"), ipArray);
    }

    if (!protocols.isEmpty()) {
        QJsonArray protocolArray;
        for (const QString& item : protocols) {
            protocolArray.append(item);
        }
        rule.insert(QStringLiteral("protocol"), protocolArray);
    }

    if (!processes.isEmpty()) {
        QJsonArray processArray;
        for (const QString& item : processes) {
            processArray.append(item);
        }
        rule.insert(QStringLiteral("process"), processArray);
    }

    return rule;
}

QJsonObject ClientConfigWriter::createSingBoxRoutingRule(
    const RoutingRule& source,
    const QStringList& inboundTags,
    const QString& port,
    const QStringList& domains,
    const QStringList& ips,
    const QStringList& protocols)
{
    QJsonObject rule;
    const QString outboundTag = source.outboundTag.trimmed();
    if (outboundTag.compare(QStringLiteral("block"), Qt::CaseInsensitive) == 0) {
        rule.insert(QStringLiteral("action"), QStringLiteral("reject"));
    } else {
        rule.insert(QStringLiteral("action"), QStringLiteral("route"));
        if (!outboundTag.isEmpty()) {
            rule.insert(QStringLiteral("outbound"), outboundTag);
        }
    }

    if (!inboundTags.isEmpty()) {
        QJsonArray inboundArray;
        for (const QString& item : inboundTags) {
            inboundArray.append(item);
        }
        rule.insert(QStringLiteral("inbound"), inboundArray);
    }

    if (!port.isEmpty()) {
        populateSingBoxPortFields(rule, port);
    }

    populateSingBoxNetworkFields(rule, source.network);

    if (!protocols.isEmpty()) {
        QJsonArray protocolArray;
        for (const QString& item : protocols) {
            protocolArray.append(item);
        }
        rule.insert(QStringLiteral("protocol"), protocolArray);
    }

    if (!domains.isEmpty()) {
        populateSingBoxDomainFields(rule, domains);
    }

    if (!ips.isEmpty()) {
        populateSingBoxIpFields(rule, ips);
    }

    return rule;
}

void ClientConfigWriter::populateSingBoxPortFields(QJsonObject& rule, const QString& port)
{
    QJsonArray ports;
    QJsonArray portRanges;

    for (const QString& item : splitCsv(port)) {
        if (item.contains(QChar('-'))) {
            portRanges.append(item);
            continue;
        }

        bool ok = false;
        const int value = item.toInt(&ok);
        if (ok) {
            ports.append(value);
        }
    }

    if (!ports.isEmpty()) {
        rule.insert(QStringLiteral("port"), ports);
    }
    if (!portRanges.isEmpty()) {
        QJsonArray normalizedRanges;
        for (const QJsonValue& value : portRanges) {
            normalizedRanges.append(value.toString().replace(QChar('-'), QChar(':')));
        }
        rule.insert(QStringLiteral("port_range"), normalizedRanges);
    }
}

void ClientConfigWriter::populateSingBoxNetworkFields(QJsonObject& rule, const QString& network)
{
    const QString trimmed = network.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    QJsonArray networks;
    for (const QString& value : splitCsv(trimmed)) {
        networks.append(value);
    }
    if (!networks.isEmpty()) {
        rule.insert(QStringLiteral("network"), networks);
    }
}

void ClientConfigWriter::populateSingBoxDomainFields(QJsonObject& rule, const QStringList& domains)
{
    QJsonArray exactDomains;
    QJsonArray domainSuffixes;
    QJsonArray domainKeywords;
    QJsonArray domainRegexes;
    QJsonArray geositeValues;

    for (const QString& domain : domains) {
        if (domain.startsWith(QStringLiteral("ext:"), Qt::CaseInsensitive)
            || domain.startsWith(QStringLiteral("ext-domain:"), Qt::CaseInsensitive)) {
            continue;
        }

        if (domain.startsWith(QStringLiteral("geosite:"), Qt::CaseInsensitive)) {
            geositeValues.append(domain.mid(QStringLiteral("geosite:").size()));
            continue;
        }

        if (domain.startsWith(QStringLiteral("full:"), Qt::CaseInsensitive)) {
            exactDomains.append(domain.mid(QStringLiteral("full:").size()));
            continue;
        }

        if (domain.startsWith(QStringLiteral("domain:"), Qt::CaseInsensitive)) {
            domainSuffixes.append(domain.mid(QStringLiteral("domain:").size()));
            continue;
        }

        if (domain.startsWith(QStringLiteral("keyword:"), Qt::CaseInsensitive)) {
            domainKeywords.append(domain.mid(QStringLiteral("keyword:").size()));
            continue;
        }

        if (domain.startsWith(QStringLiteral("dotless:"), Qt::CaseInsensitive)) {
            domainKeywords.append(domain.mid(QStringLiteral("dotless:").size()));
            continue;
        }

        if (domain.startsWith(QStringLiteral("regexp:"), Qt::CaseInsensitive)) {
            domainRegexes.append(domain.mid(QStringLiteral("regexp:").size()));
            continue;
        }

        domainKeywords.append(domain);
    }

    if (!exactDomains.isEmpty()) {
        rule.insert(QStringLiteral("domain"), exactDomains);
    }
    if (!domainSuffixes.isEmpty()) {
        rule.insert(QStringLiteral("domain_suffix"), domainSuffixes);
    }
    if (!domainKeywords.isEmpty()) {
        rule.insert(QStringLiteral("domain_keyword"), domainKeywords);
    }
    if (!domainRegexes.isEmpty()) {
        rule.insert(QStringLiteral("domain_regex"), domainRegexes);
    }
    if (!geositeValues.isEmpty()) {
        rule.insert(QStringLiteral("geosite"), geositeValues);
    }
}

void ClientConfigWriter::populateSingBoxIpFields(QJsonObject& rule, const QStringList& ips)
{
    QJsonArray ipCidrs;
    QJsonArray geoipValues;
    bool privateIp = false;

    for (const QString& ip : ips) {
        if (ip.startsWith(QStringLiteral("geoip:"), Qt::CaseInsensitive)) {
            const QString value = ip.mid(QStringLiteral("geoip:").size());
            if (value.compare(QStringLiteral("private"), Qt::CaseInsensitive) == 0) {
                privateIp = true;
            } else {
                geoipValues.append(value);
            }
            continue;
        }

        ipCidrs.append(ip);
    }

    if (!ipCidrs.isEmpty()) {
        rule.insert(QStringLiteral("ip_cidr"), ipCidrs);
    }
    if (!geoipValues.isEmpty()) {
        rule.insert(QStringLiteral("geoip"), geoipValues);
    }
    if (privateIp) {
        rule.insert(QStringLiteral("ip_is_private"), true);
    }
}

bool ClientConfigWriter::containsLegacyApiInbound(const QJsonArray& inbounds)
{
    for (const QJsonValue& value : inbounds) {
        if (!value.isObject()) {
            continue;
        }

        if (value.toObject().value(QStringLiteral("tag")).toString() == kStatisticsTag) {
            return true;
        }
    }

    return false;
}

bool ClientConfigWriter::containsLegacyApiRoute(const QJsonArray& rules)
{
    for (const QJsonValue& value : rules) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("outboundTag")).toString() != kStatisticsTag) {
            continue;
        }

        const QJsonArray inboundTags = rule.value(QStringLiteral("inboundTag")).toArray();
        for (const QJsonValue& inboundTag : inboundTags) {
            if (inboundTag.toString() == kStatisticsTag) {
                return true;
            }
        }
    }

    return false;
}

void ClientConfigWriter::migrateGeoToRuleSet(QJsonObject& root)
{
    QSet<QString> allRuleSetTags;

    auto processRules = [&allRuleSetTags](QJsonArray& rules) {
        for (int i = 0; i < rules.size(); ++i) {
            QJsonObject rule = rules[i].toObject();
            QJsonArray ruleSetTags = rule.take(QStringLiteral("rule_set")).toArray();
            bool modified = false;

            if (rule.contains(QStringLiteral("geosite"))) {
                QJsonArray values = rule.take(QStringLiteral("geosite")).toArray();
                for (const QJsonValue& v : values) {
                    const QString tag = QStringLiteral("geosite-%1").arg(v.toString());
                    ruleSetTags.append(tag);
                    allRuleSetTags.insert(tag);
                }
                modified = true;
            }

            if (rule.contains(QStringLiteral("geoip"))) {
                QJsonArray values = rule.take(QStringLiteral("geoip")).toArray();
                for (const QJsonValue& v : values) {
                    const QString tag = QStringLiteral("geoip-%1").arg(v.toString());
                    ruleSetTags.append(tag);
                    allRuleSetTags.insert(tag);
                }
                modified = true;
            }

            if (modified) {
                rule.insert(QStringLiteral("rule_set"), ruleSetTags);
                rules[i] = rule;
            }
        }
    };

    if (root.contains(QStringLiteral("dns"))) {
        QJsonObject dns = root.value(QStringLiteral("dns")).toObject();
        if (dns.contains(QStringLiteral("rules"))) {
            QJsonArray dnsRules = dns.take(QStringLiteral("rules")).toArray();
            processRules(dnsRules);
            dns.insert(QStringLiteral("rules"), dnsRules);
            root.insert(QStringLiteral("dns"), dns);
        }
    }

    QJsonObject route = root.take(QStringLiteral("route")).toObject();
    if (route.contains(QStringLiteral("rules"))) {
        QJsonArray routeRules = route.take(QStringLiteral("rules")).toArray();
        processRules(routeRules);
        route.insert(QStringLiteral("rules"), routeRules);
    }

    if (!allRuleSetTags.isEmpty()) {
        QJsonArray ruleSetDefinitions;
        for (const QString& tag : allRuleSetTags) {
            QJsonObject def;
            def.insert(QStringLiteral("tag"), tag);
            def.insert(QStringLiteral("type"), QStringLiteral("remote"));
            def.insert(QStringLiteral("format"), QStringLiteral("binary"));
            if (tag.startsWith(QStringLiteral("geosite-"))) {
                def.insert(QStringLiteral("url"),
                    QStringLiteral("https://raw.githubusercontent.com/SagerNet/sing-geosite/rule-set/%1.srs").arg(tag));
            } else if (tag.startsWith(QStringLiteral("geoip-"))) {
                def.insert(QStringLiteral("url"),
                    QStringLiteral("https://raw.githubusercontent.com/SagerNet/sing-geoip/rule-set/%1.srs").arg(tag));
            }
            ruleSetDefinitions.append(def);
        }
        route.insert(QStringLiteral("rule_set"), ruleSetDefinitions);
    }

    root.insert(QStringLiteral("route"), route);
}

QJsonArray ClientConfigWriter::buildSingBoxStatisticsInboundTags(const Config& config)
{
    QJsonArray tags;
    tags.append(QStringLiteral("socks"));
    tags.append(QStringLiteral("http"));
    if (config.allowLanConnection) {
        tags.append(QStringLiteral("socks-lan"));
        tags.append(QStringLiteral("http-lan"));
    }
    return tags;
}

QJsonArray ClientConfigWriter::buildSingBoxStatisticsOutboundTags()
{
    return QJsonArray{
        QStringLiteral("proxy"),
        QStringLiteral("direct"),
        QStringLiteral("block")};
}

QStringList ClientConfigWriter::normalizeRuleValues(
    const QStringList& values,
    bool replaceCommaPlaceholder,
    bool removeCommentEntries)
{
    QStringList result;
    for (const QString& value : values) {
        QString normalized = value.trimmed();
        if (normalized.isEmpty()) {
            continue;
        }

        if (removeCommentEntries && normalized.startsWith(QChar('#'))) {
            continue;
        }

        if (replaceCommaPlaceholder) {
            normalized.replace(QStringLiteral("<COMMA>"), QStringLiteral(","));
        }

        result.append(normalized);
    }

    return result;
}

QString ClientConfigWriter::resolveSingBoxNetwork(const VmessItem& server)
{
    return server.network.trimmed().isEmpty() ? QStringLiteral("tcp") : server.network.trimmed();
}

QString ClientConfigWriter::resolveSingBoxOutboundType(ConfigType type)
{
    switch (type) {
    case ConfigType::VMess:
        return QStringLiteral("vmess");
    case ConfigType::VLESS:
        return QStringLiteral("vless");
    case ConfigType::Trojan:
        return QStringLiteral("trojan");
    case ConfigType::Shadowsocks:
        return QStringLiteral("shadowsocks");
    case ConfigType::Socks:
        return QStringLiteral("socks");
    case ConfigType::Hysteria2:
        return QStringLiteral("hysteria2");
    case ConfigType::TUIC:
        return QStringLiteral("tuic");
    case ConfigType::WireGuard:
        return QStringLiteral("wireguard");
    case ConfigType::AnyTLS:
        return QStringLiteral("anytls");
    case ConfigType::Naive:
        return QStringLiteral("naive");
    case ConfigType::Custom:
    case ConfigType::Unknown:
    default:
        return {};
    }
}

QString ClientConfigWriter::normalizeSingBoxLogLevel(const QString& level)
{
    const QString normalized = level.trimmed().toLower();
    if (normalized.isEmpty()) {
        return QStringLiteral("warn");
    }
    if (normalized == QStringLiteral("warning")) {
        return QStringLiteral("warn");
    }
    return normalized;
}

QString ClientConfigWriter::resolvePrimaryPath(const QString& value)
{
    const QStringList paths = splitCsv(value);
    return paths.isEmpty() ? QStringLiteral("/") : paths.constFirst();
}

QStringList ClientConfigWriter::splitCsv(const QString& value)
{
    QStringList result;
    const QStringList parts = value.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
        }
    }

    return result;
}

QString ClientConfigWriter::resolveServerName(const VmessItem& server)
{
    if (!server.sni.trimmed().isEmpty()) {
        return server.sni;
    }

    const QStringList hosts = splitCsv(server.requestHost);
    if (!hosts.isEmpty()) {
        return hosts.constFirst();
    }

    return server.address;
}

QString ClientConfigWriter::resolveFingerprint(const Config& config, const VmessItem& server)
{
    return server.fingerprint.trimmed().isEmpty()
        ? config.defaultFingerprint.trimmed()
        : server.fingerprint.trimmed();
}

QString ClientConfigWriter::resolveRealityFingerprint(const Config& config, const VmessItem& server)
{
    const QString fingerprint = resolveFingerprint(config, server);
    return fingerprint.isEmpty() ? QStringLiteral("chrome") : fingerprint;
}

QStringList ClientConfigWriter::buildTunCompatDnsProcessNames()
{
    QStringList processNames{
        QStringLiteral("xray.exe"),
        QStringLiteral("v2ray.exe"),
        QStringLiteral("wv2ray.exe"),
        QStringLiteral("SagerNet.exe"),
        QStringLiteral("hysteria-windows-amd64.exe"),
        QStringLiteral("hysteria-windows-386.exe"),
        QStringLiteral("hysteria.exe"),
        QStringLiteral("naiveproxy.exe"),
        QStringLiteral("naive.exe"),
        QStringLiteral("mihomo-windows-amd64-v1.exe"),
        QStringLiteral("mihomo-windows-amd64-compatible.exe"),
        QStringLiteral("mihomo-windows-amd64.exe"),
        QStringLiteral("mihomo-windows-arm64.exe"),
        QStringLiteral("clash-windows-amd64-v3.exe"),
        QStringLiteral("clash-windows-amd64.exe"),
        QStringLiteral("clash-windows-386.exe"),
        QStringLiteral("clash.exe"),
        QStringLiteral("mihomo.exe")};

    QSet<QString> seen;
    QStringList deduplicated;
    for (const QString& processName : processNames) {
        if (seen.contains(processName)) {
            continue;
        }
        seen.insert(processName);
        deduplicated.append(processName);
    }
    return deduplicated;
}

QStringList ClientConfigWriter::buildTunCompatDirectProcessNames()
{
    QStringList processNames{QStringLiteral("v2rayq.exe")};
    processNames.append(buildTunCompatDnsProcessNames());
    processNames.append(QStringLiteral("sing-box-client.exe"));
    processNames.append(QStringLiteral("sing-box.exe"));
    return processNames;
}

bool ClientConfigWriter::isRealityTransport(const QString& value)
{
    return value.compare(QStringLiteral("reality"), Qt::CaseInsensitive) == 0;
}

bool ClientConfigWriter::isValidRealityShortId(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return true;
    }

    if (trimmed.size() > 16 || (trimmed.size() % 2) != 0) {
        return false;
    }

    for (const QChar ch : trimmed) {
        if (!ch.isDigit() && (ch.toLower() < QChar('a') || ch.toLower() > QChar('f'))) {
            return false;
        }
    }

    return true;
}

bool ClientConfigWriter::resolveAllowInsecure(const QString& value, bool fallbackValue)
{
    if (value.trimmed().isEmpty()) {
        return fallbackValue;
    }

    return value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("1"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0;
}

QString ClientConfigWriter::resolveCustomConfigPath(const QString& address) const
{
    const QString trimmed = address.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (QFileInfo::exists(trimmed)) {
        return QFileInfo(trimmed).absoluteFilePath();
    }

    if (!customConfigDirectory_.trimmed().isEmpty()) {
        const QString managedPath = QDir(customConfigDirectory_).filePath(trimmed);
        if (QFileInfo::exists(managedPath)) {
            return QFileInfo(managedPath).absoluteFilePath();
        }
    }

    return trimmed;
}
