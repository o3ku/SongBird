#include "services/SpeedTestBatchConfig.h"

#include <QJsonArray>
#include <QJsonValue>

namespace {

const QString kLoopbackAddress = QStringLiteral("127.0.0.1");

QJsonObject buildLegacySocksInbound(int port, const QString& tag)
{
    QJsonObject inbound;
    inbound.insert(QStringLiteral("tag"), tag);
    inbound.insert(QStringLiteral("port"), port);
    inbound.insert(QStringLiteral("listen"), kLoopbackAddress);
    inbound.insert(QStringLiteral("protocol"), QStringLiteral("socks"));

    QJsonObject settings;
    settings.insert(QStringLiteral("auth"), QStringLiteral("noauth"));
    settings.insert(QStringLiteral("udp"), false);
    settings.insert(QStringLiteral("allowTransparent"), false);
    inbound.insert(QStringLiteral("settings"), settings);
    return inbound;
}

QJsonObject buildLegacyDirectOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("direct"));
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("freedom"));
    outbound.insert(QStringLiteral("settings"), QJsonObject{});
    return outbound;
}

QJsonObject buildLegacyBlackholeOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("block"));
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("blackhole"));
    outbound.insert(QStringLiteral("settings"), QJsonObject{});
    return outbound;
}

QJsonObject buildLegacyRoutingRule(const QString& inboundTag, const QString& outboundTag)
{
    QJsonObject rule;
    rule.insert(QStringLiteral("type"), QStringLiteral("field"));
    QJsonArray tags;
    tags.append(inboundTag);
    rule.insert(QStringLiteral("inboundTag"), tags);
    rule.insert(QStringLiteral("outboundTag"), outboundTag);
    return rule;
}

QJsonObject buildSingBoxSocksInbound(int port, const QString& tag)
{
    QJsonObject inbound;
    inbound.insert(QStringLiteral("type"), QStringLiteral("socks"));
    inbound.insert(QStringLiteral("tag"), tag);
    inbound.insert(QStringLiteral("listen"), kLoopbackAddress);
    inbound.insert(QStringLiteral("listen_port"), port);
    return inbound;
}

QJsonObject buildSingBoxDirectOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("type"), QStringLiteral("direct"));
    outbound.insert(QStringLiteral("tag"), QStringLiteral("direct"));
    return outbound;
}

QJsonObject buildSingBoxRoutingRule(const QString& inboundTag, const QString& outboundTag)
{
    QJsonObject rule;
    QJsonArray tags;
    tags.append(inboundTag);
    rule.insert(QStringLiteral("inbound"), tags);
    rule.insert(QStringLiteral("outbound"), outboundTag);
    return rule;
}

} // namespace

namespace SpeedTestBatchConfig {

std::optional<std::pair<BatchFlavor, QJsonObject>> extractPrimaryOutbound(const QJsonObject& root)
{
    const QJsonValue outboundsValue = root.value(QStringLiteral("outbounds"));
    if (!outboundsValue.isArray()) {
        return std::nullopt;
    }
    const QJsonArray outboundsArray = outboundsValue.toArray();
    if (outboundsArray.isEmpty()) {
        return std::nullopt;
    }
    const QJsonValue firstValue = outboundsArray.first();
    if (!firstValue.isObject()) {
        return std::nullopt;
    }
    const QJsonObject outbound = firstValue.toObject();
    if (outbound.contains(QStringLiteral("type"))) {
        return std::make_pair(BatchFlavor::SingBox, outbound);
    }
    if (outbound.contains(QStringLiteral("protocol"))) {
        return std::make_pair(BatchFlavor::Legacy, outbound);
    }
    return std::nullopt;
}

QJsonObject assembleLegacyConfig(const QList<ProbeEntry>& entries)
{
    QJsonObject root;
    QJsonObject log;
    log.insert(QStringLiteral("loglevel"), QStringLiteral("warning"));
    root.insert(QStringLiteral("log"), log);

    QJsonArray inbounds;
    QJsonArray outbounds;
    QJsonArray rules;
    for (const ProbeEntry& entry : entries) {
        inbounds.append(buildLegacySocksInbound(entry.socksPort, entry.inboundTag));
        outbounds.append(entry.outbound);
        rules.append(buildLegacyRoutingRule(entry.inboundTag, entry.outboundTag));
    }
    outbounds.append(buildLegacyDirectOutbound());
    outbounds.append(buildLegacyBlackholeOutbound());
    root.insert(QStringLiteral("inbounds"), inbounds);
    root.insert(QStringLiteral("outbounds"), outbounds);

    QJsonObject routing;
    routing.insert(QStringLiteral("domainStrategy"), QStringLiteral("AsIs"));
    routing.insert(QStringLiteral("rules"), rules);
    root.insert(QStringLiteral("routing"), routing);

    return root;
}

QJsonObject assembleSingBoxConfig(const QList<ProbeEntry>& entries)
{
    QJsonObject root;
    QJsonObject log;
    log.insert(QStringLiteral("level"), QStringLiteral("warn"));
    root.insert(QStringLiteral("log"), log);

    QJsonArray inbounds;
    QJsonArray outbounds;
    QJsonArray rules;
    for (const ProbeEntry& entry : entries) {
        inbounds.append(buildSingBoxSocksInbound(entry.socksPort, entry.inboundTag));
        outbounds.append(entry.outbound);
        rules.append(buildSingBoxRoutingRule(entry.inboundTag, entry.outboundTag));
    }
    outbounds.append(buildSingBoxDirectOutbound());
    root.insert(QStringLiteral("inbounds"), inbounds);
    root.insert(QStringLiteral("outbounds"), outbounds);

    QJsonObject route;
    route.insert(QStringLiteral("final"), QStringLiteral("direct"));
    route.insert(QStringLiteral("rules"), rules);
    root.insert(QStringLiteral("route"), route);

    return root;
}

} // namespace SpeedTestBatchConfig
