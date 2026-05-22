#pragma once

#include <QJsonObject>
#include <QMap>
#include <QStringList>

#include "domain/models/Config.h"
#include "domain/models/RoutingItem.h"

class DnsConfigFragments {
public:
    static QJsonObject buildLegacyDns(const Config& config, const RoutingItem* selectedRouting);
    static QJsonObject buildSingBoxDns(const Config& config, const RoutingItem* selectedRouting);

    static QStringList splitDnsAddresses(const QString& value);
    static bool isCustomDnsObjectText(const QString& value);
    static QJsonObject parseSingBoxDnsAddress(const QString& value);
    static QString mapDomainStrategyToSingBox(const QString& strategy);
    static bool usesDirectDnsAsFinalServer(const RoutingItem* routing);
    static bool appendSingBoxDomainField(QJsonObject& rule, const QString& value, bool plainAsDomain);
    static QJsonObject buildSingBoxFakeIpFilterRule();
    static QString mapDnsRcode(int code);
    static bool isIpAddress(const QString& value);
    static bool isDomainName(const QString& value);
    static QMap<QString, QStringList> parseHostsToDictionary(const QString& hosts);
    static QMap<QString, QString> loadSystemHosts();
};
