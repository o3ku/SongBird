#pragma once

#include <QString>

#include "common/OperationResult.h"

// There is deliberately no `tunEnabled` member. There used to be, and `check()` refused outright
// when it was set -- but the HTTP inbound the check talks to is not conditional on TUN. Every
// backend's *main* client config adds it unconditionally at `localPort + 1`
// (SingBoxCoreBackend::buildInbounds, XrayCoreBackend::buildInbounds, MihomoConfigFragments), and
// the TUN sidecar core is built with AuxiliaryTunRouting::RelayToLocalProxy -- it relays *into*
// that very inbound, so it cannot exist without it. The guard made the check unusable in TUN mode
// for no reason; SongBirdAutoCoordinator worked around it by asserting `tunEnabled = false`, which
// was a caller lying to the service rather than a constraint.
struct ProxyAvailabilityCheckConfig {
    int localPort = 0;
    QString speedPingTestUrl;
};

class ProxyAvailabilityCheckService {
public:
    OperationResult check(const ProxyAvailabilityCheckConfig& config) const;
};
