#include "auto/AutoRuntimeDefaults.h"

#include "app/OutboundLocationProbeService.h"
#include "common/PortValidator.h"
#include "common/SystemProxyMode.h"

void applyAutoRuntimeDefaults(Config& config)
{
    config.sysProxyType = toLegacySystemProxyModeValue(SystemProxyMode::ForcedChange);
    config.ui().mainProxyEnabled = true;
    if (!isValidTcpPort(config.localPort)
        || config.localPort > kMaxTcpPort - OutboundLocationProbeService::LocationProbePortOffset) {
        config.localPort = 10808;
    }
    config.localHttpPort = config.localPort + 1;
    config.localLocationProbePort = config.localPort + OutboundLocationProbeService::LocationProbePortOffset;
}
