#pragma once

#include <QString>

#include "common/OperationResult.h"
#include "common/PortValidator.h"

// There is deliberately no `tunEnabled` member. There used to be, and `check()` refused outright
// when it was set -- but the HTTP inbound the check talks to is not conditional on TUN. Every
// backend's *main* client config adds it unconditionally (SingBoxCoreBackend::buildInbounds,
// XrayCoreBackend::buildInbounds, MihomoConfigFragments), and the TUN sidecar core is built with
// AuxiliaryTunRouting::RelayToLocalProxy -- it relays *into* that very inbound, so it cannot exist
// without it. The guard made the check unusable in TUN mode for no reason.
//
// `httpPort` is resolved, not derived: `localPort + 1` used to be spelled out here as well, and
// a config carrying a `localHttpPort` override had the core bound to one port and this check
// dialling another.
//
// `timeoutMs` is injectable for the same reason every other timing constant in the probe services
// is: the production value is 30 s, and a memory/regression soak that has to wait it out thousands
// of times cannot exist. Non-positive keeps the production default.
struct ProxyAvailabilityCheckConfig {
    int httpPort = 0;
    QString speedPingTestUrl;
    int timeoutMs = 0;
};

class ProxyAvailabilityCheckService {
public:
    OperationResult check(const ProxyAvailabilityCheckConfig& config) const;
};
