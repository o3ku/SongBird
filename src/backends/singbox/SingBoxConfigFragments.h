#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

class SingBoxConfigFragments {
public:
    static QJsonObject buildDirectOutbound();
    static QJsonObject buildBlockOutbound();
    static QJsonObject buildTunInbound(const Config& config);
    static QJsonObject buildSocksInbound(const Config& config, bool allowLan, int offset);
    static QJsonObject buildHttpInbound(const Config& config, bool allowLan, int offset);
    static QJsonObject buildHttpInboundWithTag(const Config& config, const QString& tag, int offset);
    static QJsonObject buildPrimaryOutbound(const Config& config, const VmessItem& server);
    static QJsonObject buildTls(const Config& config, const VmessItem& server);
    static QJsonObject buildTransport(const VmessItem& server);
    static QJsonObject buildSocksOutbound(
        const QString& tag,
        const QString& server,
        int port,
        bool udpOverTcp = false);

    static QJsonObject buildTunCompatDns();
    static QJsonObject buildTunCompatRoute(const Config& config);
    static QJsonArray buildTunCompatOutbounds(const Config& config);
    static QJsonObject buildTunCompatPrivateAddressDirectRule();
    static QJsonArray buildTunCompatRejectRules();
    static void appendTunCompatProcessRules(QJsonArray& rules);
    static void appendTunIcmpRouteRule(QJsonArray& rules, const TunModeItem& tun);
    static void appendTunUdpRouteRule(QJsonArray& rules, const TunModeItem& tun);
    static void appendSniffRules(QJsonArray& rules, const Config& config);
    static QStringList buildTunCompatDnsProcessNames();
    static QStringList buildTunCompatDirectProcessNames();
};
