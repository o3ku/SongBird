#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

struct Config;
struct TunModeItem;

namespace SingBoxTunCompatConfigFragments {

QJsonObject buildDns();
QJsonObject buildRoute(const Config& config);
QJsonArray buildOutbounds(const Config& config);
QJsonArray buildRejectRules();
void appendProcessRules(QJsonArray& rules);
void appendIcmpRouteRule(QJsonArray& rules, const TunModeItem& tun);
void appendUdpRouteRule(QJsonArray& rules, const TunModeItem& tun);
void appendSniffRules(QJsonArray& rules, const Config& config);
QStringList buildDnsProcessNames();
QStringList buildDirectProcessNames();

} // namespace SingBoxTunCompatConfigFragments
