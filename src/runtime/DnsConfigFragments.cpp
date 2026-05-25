#include "runtime/DnsConfigFragments.h"

#include "runtime/DnsAddressParser.h"
#include "runtime/DnsHosts.h"
#include "runtime/LegacyDnsConfigBuilder.h"
#include "runtime/SingBoxDnsConfigBuilder.h"
#include "runtime/SingBoxDnsConfigSupport.h"

namespace DnsSupport = SingBoxDnsConfigSupport;

QStringList DnsConfigFragments::splitDnsAddresses(const QString& value)
{
    return DnsAddressParser::splitDnsAddresses(value);
}

QJsonObject DnsConfigFragments::parseSingBoxDnsAddress(const QString& value)
{
    return DnsSupport::parseDnsAddress(value);
}

QString DnsConfigFragments::mapDomainStrategyToSingBox(const QString& strategy)
{
    return DnsSupport::mapDomainStrategy(strategy);
}

bool DnsConfigFragments::isIpAddress(const QString& value)
{
    return DnsHosts::isIpAddress(value);
}

bool DnsConfigFragments::isDomainName(const QString& value)
{
    return DnsHosts::isDomainName(value);
}

QString DnsConfigFragments::mapDnsRcode(int code)
{
    return DnsSupport::mapRcode(code);
}

QMap<QString, QStringList> DnsConfigFragments::parseHostsToDictionary(const QString& hosts)
{
    return DnsHosts::parseConfigured(hosts);
}

QMap<QString, QString> DnsConfigFragments::loadSystemHosts()
{
    return DnsHosts::loadSystem();
}

QJsonObject DnsConfigFragments::buildSingBoxFakeIpFilterRule()
{
    return DnsSupport::fakeIpFilterRule();
}

bool DnsConfigFragments::appendSingBoxDomainField(QJsonObject& rule, const QString& value, bool plainAsDomain)
{
    return DnsSupport::appendDomainField(rule, value, plainAsDomain);
}

bool DnsConfigFragments::usesDirectDnsAsFinalServer(const RoutingItem* routing)
{
    return DnsSupport::usesDirectDnsAsFinalServer(routing);
}

bool DnsConfigFragments::isCustomDnsObjectText(const QString& value)
{
    return DnsSupport::isCustomDnsObjectText(value);
}

QJsonObject DnsConfigFragments::buildLegacyDns(const Config& config, const RoutingItem* selectedRouting)
{
    return LegacyDnsConfigBuilder::build(config, selectedRouting);
}

QJsonObject DnsConfigFragments::buildSingBoxDns(const Config& config, const RoutingItem* selectedRouting)
{
    return SingBoxDnsConfigBuilder::build(config, selectedRouting);
}
