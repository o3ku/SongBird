#include "auto/AutoRuntimeDefaults.h"

#include "common/LocalInboundPorts.h"
#include "common/LocalPortOption.h"
#include "common/PortValidator.h"
#include "common/SystemProxyMode.h"

void applyAutoRuntimeDefaults(Config& config)
{
    config.sysProxyType = toLegacySystemProxyModeValue(SystemProxyMode::ForcedChange);
    config.ui().mainProxyEnabled = true;
    // Auto binds several ports derived from this one (HTTP on +1, the location probe on +103), so a
    // port near the top of the range would push one of them past 65535. Out of range, or without
    // headroom, fall back to the shared default rather than inventing a number.
    if (effectiveLocalPort(config.localPort) != config.localPort
        || config.localPort > kMaxTcpPort - LocalInboundPorts::kLocationProbeOffset) {
        config.localPort = kDefaultLocalPort;
    }
    // Both derived ports are overrides, and httpPort()/locationProbePort() return an override that is
    // already set. A config carried over from an earlier run can hold one derived from a base port
    // this function just replaced -- `localPort = 0` normalising to 10808 while the stored
    // `localHttpPort` still says 1 is the case the tests pin. Clearing first means the two ports
    // below are always derived from the base port this run settled on.
    LocalInboundPorts::clearStaleOverrides(config);
    config.localHttpPort = LocalInboundPorts::httpPort(config);
    config.localLocationProbePort = LocalInboundPorts::locationProbePort(config);
}
