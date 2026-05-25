#include "backends/xray/XrayConfigFragments.h"

#include <QJsonArray>

#include "backends/xray/XrayOutboundConfigFragments.h"
#include "backends/xray/XrayStreamConfigFragments.h"

namespace {

const QString kLocationProbeTag = QStringLiteral("location-probe");

} // namespace

QJsonObject XrayConfigFragments::buildSocksInbound(const Config& config, bool allowLan, int offset)
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
        settings.insert(QStringLiteral("auth"), QStringLiteral("password"));
    }
    inbound.insert(QStringLiteral("settings"), settings);
    inbound.insert(QStringLiteral("sniffing"), buildSniffing(config));
    return inbound;
}

QJsonObject XrayConfigFragments::buildHttpInbound(const Config& config, bool allowLan, int offset)
{
    return buildHttpInboundWithTag(
        config,
        allowLan ? QStringLiteral("http-lan") : QStringLiteral("http"),
        offset);
}

QJsonObject XrayConfigFragments::buildHttpInboundWithTag(const Config& config, const QString& tag, int offset)
{
    QJsonObject inbound;
    const bool allowLan = tag.endsWith(QStringLiteral("-lan"));
    int port = config.localPort + offset;
    if (!allowLan && offset == 1 && config.localHttpPort > 0) {
        port = config.localHttpPort;
    } else if (tag == kLocationProbeTag && config.localLocationProbePort > 0) {
        port = config.localLocationProbePort;
    }
    inbound.insert(QStringLiteral("tag"), tag);
    inbound.insert(QStringLiteral("port"), port);
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

QJsonObject XrayConfigFragments::buildSniffing(const Config& config)
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

QJsonObject XrayConfigFragments::buildStreamSettings(const Config& config, const VmessItem& server)
{
    return XrayStreamConfigFragments::buildStreamSettings(config, server);
}

QJsonObject XrayConfigFragments::buildPrimaryOutbound(const Config& config, const VmessItem& server)
{
    return XrayOutboundConfigFragments::buildPrimaryOutbound(config, server);
}

QJsonObject XrayConfigFragments::buildDirectOutbound(const Config& config)
{
    return XrayOutboundConfigFragments::buildDirectOutbound(config);
}

QJsonObject XrayConfigFragments::buildBlackholeOutbound()
{
    return XrayOutboundConfigFragments::buildBlackholeOutbound();
}

QJsonObject XrayConfigFragments::buildFragmentOutbound()
{
    return XrayOutboundConfigFragments::buildFragmentOutbound();
}
