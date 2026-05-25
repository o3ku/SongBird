#include "subscription/ShareUrlBuilder.h"

#include "subscription/ShareUrlProtocolBuilders.h"

QString ShareUrlBuilder::build(const VmessItem& item)
{
    switch (item.configType) {
    case ConfigType::VMess:
        return ShareUrlProtocolBuilders::buildVmess(item);
    case ConfigType::Shadowsocks:
        return ShareUrlProtocolBuilders::buildShadowsocks(item);
    case ConfigType::Socks:
        return ShareUrlProtocolBuilders::buildSocks(item);
    case ConfigType::Trojan:
        return ShareUrlProtocolBuilders::buildTrojan(item);
    case ConfigType::VLESS:
        return ShareUrlProtocolBuilders::buildVless(item);
    case ConfigType::Hysteria2:
        return ShareUrlProtocolBuilders::buildHysteria2(item);
    case ConfigType::TUIC:
        return ShareUrlProtocolBuilders::buildTuic(item);
    case ConfigType::WireGuard:
        return ShareUrlProtocolBuilders::buildWireguard(item);
    case ConfigType::AnyTLS:
        return ShareUrlProtocolBuilders::buildAnytls(item);
    case ConfigType::Naive:
        return ShareUrlProtocolBuilders::buildNaive(item);
    case ConfigType::Custom:
    case ConfigType::Unknown:
    default:
        return {};
    }
}
