#include "runtime/RoutingRuleJsonMapper.h"

#include <QJsonObject>
#include <QJsonValue>

namespace {

QStringList splitCsv(const QString& value)
{
    QStringList items;
    const QStringList rawItems = value.split(',', Qt::SkipEmptyParts);
    for (const QString& rawItem : rawItems) {
        const QString item = rawItem.trimmed();
        if (!item.isEmpty()) {
            items.append(item);
        }
    }

    return items;
}

void populateSingBoxPortFields(QJsonObject& rule, const QString& port)
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

void populateSingBoxNetworkFields(QJsonObject& rule, const QString& network)
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

void populateSingBoxDomainFields(QJsonObject& rule, const QStringList& domains)
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

void populateSingBoxIpFields(QJsonObject& rule, const QStringList& ips)
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

QJsonObject createLegacyRoutingRule(
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

QJsonObject createSingBoxRoutingRule(
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

} // namespace

void RoutingRuleJsonMapper::appendLegacyRoutingRule(QJsonArray& rules, const RoutingRule& source)
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

void RoutingRuleJsonMapper::appendSingBoxRoutingRule(QJsonArray& rules, const RoutingRule& source)
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

void RoutingRuleJsonMapper::appendSingBoxRoutingIpRule(QJsonArray& rules, const RoutingRule& source)
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

QStringList RoutingRuleJsonMapper::normalizeRuleValues(
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
