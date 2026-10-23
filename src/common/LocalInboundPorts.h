#pragma once

#include "common/LocalPortOption.h"
#include "common/PortValidator.h"
#include "domain/models/Config.h"

// The local inbound ports, as a single pure policy. They used to be re-derived at every reader --
// the system proxy wrote `localPort + 1`, four other services assumed the same, the backends
// honoured `localHttpPort` when it was set, and the location probe settled it a third way. A
// `localHttpPort` left over from an earlier run (the Auto front end persists one) then outlived the
// `localPort` it was derived from, and the applied system proxy pointed at a port nothing listened
// on. One function, called by every reader, is what keeps that from coming back.
//
// Every derivation starts from `effectiveLocalPort()`, not from `config.localPort`: an unset port
// (0 on disk) is an unset preference, and the ports the core actually binds are derived from the
// number the program would use. Deriving from the stored value let the backend bind 10808 while a
// reader that had already normalised dialled 10808 -- or, after a reset, neither.
namespace LocalInboundPorts {

// The dedicated inbound the runtime config reserves for the display-only location probe.
constexpr int kLocationProbeOffset = 103;

// `localHttpPort` is an override, not a source of truth: when it is unset (0) the HTTP inbound sits
// one above the SOCKS port, which is where every backend's main client config puts it. A stale
// override must not survive a port change, so the resolved value is the only thing readers may use.
inline int httpPort(const Config& config)
{
    return isValidTcpPort(config.localHttpPort) ? config.localHttpPort : effectiveLocalPort(config.localPort) + 1;
}

inline int locationProbePort(const Config& config)
{
    return isValidTcpPort(config.localLocationProbePort)
        ? config.localLocationProbePort
        : effectiveLocalPort(config.localPort) + kLocationProbeOffset;
}

// Drops overrides that disagree with the current base port, so a config that has been carried
// across a `localPort` change resolves both ports from the same base.
inline void clearStaleOverrides(Config& config)
{
    const int basePort = effectiveLocalPort(config.localPort);
    if (config.localHttpPort > 0 && config.localHttpPort != basePort + 1) {
        config.localHttpPort = 0;
    }
    if (config.localLocationProbePort > 0 && config.localLocationProbePort != basePort + kLocationProbeOffset) {
        config.localLocationProbePort = 0;
    }
}

} // namespace LocalInboundPorts
