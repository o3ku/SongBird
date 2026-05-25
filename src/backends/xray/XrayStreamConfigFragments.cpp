#include "backends/xray/XrayStreamConfigFragments.h"

#include "backends/xray/XraySecurityConfigFragments.h"
#include "backends/xray/XrayTransportConfigFragments.h"

QJsonObject XrayStreamConfigFragments::buildStreamSettings(const Config& config, const VmessItem& server)
{
    if (server.configType == ConfigType::Hysteria2) {
        return XrayTransportConfigFragments::buildHysteria2StreamSettings(server);
    }

    QJsonObject streamSettings;
    const QString network = XrayTransportConfigFragments::resolveNetwork(server);
    const QString transportSecurity = server.streamSecurity.trimmed();
    streamSettings.insert(QStringLiteral("network"), network);

    if (!transportSecurity.isEmpty()) {
        streamSettings.insert(QStringLiteral("security"), transportSecurity);
    }

    XrayTransportConfigFragments::appendTransportSettings(streamSettings, config, server, network);
    XraySecurityConfigFragments::appendFinalmask(streamSettings, server);
    XraySecurityConfigFragments::appendSecuritySettings(streamSettings, config, server, transportSecurity);

    return streamSettings;
}
