#pragma once

#include <QJsonObject>
#include <QMap>
#include <QStringList>

#include "domain/models/Config.h"
#include "domain/models/RoutingItem.h"

namespace SingBoxDnsConfigSupport {

QString directDnsTag();
QString remoteDnsTag();
QString localDnsTag();
QString hostsDnsTag();
QString fakeDnsTag();

QJsonObject parseDnsAddress(const QString& value);
bool isCustomDnsObjectText(const QString& value);
QString mapDomainStrategy(const QString& strategy);
bool usesDirectDnsAsFinalServer(const RoutingItem* routing);

/// Adds the match field for one `RoutingRule::domain` value to a sing-box DNS rule.
///
/// Returns false when the value cannot be expressed as a DNS match at all, so the caller can
/// skip it instead of emitting a rule that matches nothing.
///
/// `plainAsDomain` picks the field for a value that carries no prefix: a hosts file entry is a
/// literal hostname and wants the exact-match "domain" field, while a routing rule's bare value
/// is a keyword and wants "domain_keyword". Every caller currently passes true.
bool appendDomainField(QJsonObject& rule, const QString& value, bool plainAsDomain);

QString mapRcode(int code);
QJsonObject fakeIpFilterRule();
QJsonObject predefinedHosts(const Config& config, const QMap<QString, QStringList>& hostsMap);
QJsonObject hostsDnsServer(const QJsonObject& predefinedHosts);
void applyHostsResolver(QJsonObject& dnsServer, const QJsonObject& predefinedHosts);
QString finalDnsServerTag(
    bool hasRemoteDnsServer,
    bool hasDirectDnsServer,
    bool hasPredefinedHosts,
    bool useDirectFinal);

} // namespace SingBoxDnsConfigSupport
