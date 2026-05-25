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
