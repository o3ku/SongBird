#include "runtime/SingBoxRoutingRuleMapper.h"

#include <QJsonObject>
#include <QJsonValue>

#include "runtime/RoutingRuleJsonSupport.h"

namespace {

void populatePortFields(QJsonObject& rule, const QString& port)
{
    QJsonArray ports;
    QJsonArray portRanges;

    for (const QString& item : RoutingRuleJsonSupport::splitCsv(port)) {
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

void populateNetworkFields(QJsonObject& rule, const QString& network)
{
    const QString trimmed = network.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    QJsonArray networks;
    for (const QString& value : RoutingRuleJsonSupport::splitCsv(trimmed)) {
        networks.append(value);
    }
    if (!networks.isEmpty()) {
        rule.insert(QStringLiteral("network"), networks);
    }
}

void populateDomainFields(QJsonObject& rule, const QStringList& domains)
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

void populateIpFields(QJsonObject& rule, const QStringList& ips)
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

QJsonObject createRoutingRule(
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

    RoutingRuleJsonSupport::insertStringArray(rule, QStringLiteral("inbound"), inboundTags);

    if (!port.isEmpty()) {
        populatePortFields(rule, port);
    }

    populateNetworkFields(rule, source.network);
    RoutingRuleJsonSupport::insertStringArray(rule, QStringLiteral("protocol"), protocols);

    if (!domains.isEmpty()) {
        populateDomainFields(rule, domains);
    }

    if (!ips.isEmpty()) {
        populateIpFields(rule, ips);
    }

    return rule;
}

void appendProcessRules(
    QJsonArray& rules,
    const QJsonObject& baseRule,
    const QStringList& processes)
{
    QJsonObject processNameRule = baseRule;
    QJsonArray processNames;
    QJsonObject processPathRule = baseRule;
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

} // namespace

void SingBoxRoutingRuleMapper::appendRoutingRule(QJsonArray& rules, const RoutingRule& source)
{
    const QStringList domains = RoutingRuleJsonSupport::normalizeRuleValues(source.domain, true, true);
    const QStringList ips = RoutingRuleJsonSupport::normalizeRuleValues(source.ip);
    const QStringList protocols = RoutingRuleJsonSupport::normalizeRuleValues(source.protocol);
    const QStringList processes = RoutingRuleJsonSupport::normalizeRuleValues(source.process);
    const QStringList inboundTags = RoutingRuleJsonSupport::normalizeRuleValues(source.inboundTag);
    const QString port = source.port.trimmed().isEmpty() ? QString() : source.port.trimmed();
    const QString network = source.network.trimmed();
    const bool hasDomainOrIpOrProcess = !domains.isEmpty() || !ips.isEmpty() || !processes.isEmpty();

    if (!domains.isEmpty()) {
        rules.append(createRoutingRule(source, inboundTags, port, domains, {}, protocols));
    }

    if (!ips.isEmpty()) {
        rules.append(createRoutingRule(source, inboundTags, port, {}, ips, protocols));
    }

    if (!processes.isEmpty()) {
        const QJsonObject processRule = createRoutingRule(source, inboundTags, port, {}, {}, protocols);
        appendProcessRules(rules, processRule, processes);
    }

    if (!hasDomainOrIpOrProcess
        && (!port.isEmpty() || !network.isEmpty() || !protocols.isEmpty() || !inboundTags.isEmpty())) {
        rules.append(createRoutingRule(source, inboundTags, port, {}, {}, protocols));
    }
}

void SingBoxRoutingRuleMapper::appendIpRule(QJsonArray& rules, const RoutingRule& source)
{
    const QStringList ips = RoutingRuleJsonSupport::normalizeRuleValues(source.ip);
    if (ips.isEmpty()) {
        return;
    }

    const QStringList protocols = RoutingRuleJsonSupport::normalizeRuleValues(source.protocol);
    const QStringList inboundTags = RoutingRuleJsonSupport::normalizeRuleValues(source.inboundTag);
    const QString port = source.port.trimmed().isEmpty() ? QString() : source.port.trimmed();
    rules.append(createRoutingRule(source, inboundTags, port, {}, ips, protocols));
}
