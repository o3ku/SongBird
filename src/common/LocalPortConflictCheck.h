#pragma once

#include <functional>

#include <QHostAddress>
#include <QList>
#include <QTcpServer>

#include "common/LocalInboundPorts.h"
#include "common/PortValidator.h"
#include "domain/models/Config.h"

// The pre-start check for the ports the core is about to bind. The core used to fail with its
// own bind error when another program already held the local port, and the failure read as a
// broken node rather than a local conflict. Checking first turns it into a sentence that names
// the port.
//
// The check is a loopback bind, not a connect: a connect only says something answers, which a
// leftover listener from a previous run also does, while a bind says the core could not have
// taken the port. The bind is a probe, not a reservation -- another process can still take the
// port between the check and the core's own bind, and then the core's failure is the backstop.
// A listener bound to one LAN address only is also missed (the probe binds loopback, which is
// what the core binds by default); both gaps are inherent to probing and are the reason the
// check reports a conflict instead of promising a start.
//
// The `isFree` predicate is injected so the check is testable without a socket: production
// passes `isLocalTcpPortFree`, tests pass a scripted answer.
namespace LocalPortConflictCheck {

enum class Role {
    Socks,
    Http,
    LocationProbe
};

struct Port {
    int port = 0;
    Role role = Role::Socks;
};

// The three loopback ports the core binds for one activation, in the order a conflict is
// reported: the SOCKS inbound first, then the HTTP inbound, then the dedicated location-probe
// inbound. Every derivation starts from effectiveLocalPort(), so an unset localPort (0 on disk)
// still resolves to the ports the core would actually bind.
inline QList<Port> localInboundPorts(const Config& config)
{
    return {
        {effectiveLocalPort(config.localPort), Role::Socks},
        {LocalInboundPorts::httpPort(config), Role::Http},
        {LocalInboundPorts::locationProbePort(config), Role::LocationProbe},
    };
}

// The production probe: true when nothing holds the port on loopback. An invalid port is never
// free -- it is skipped by firstOccupiedPort rather than reported, so a broken config cannot
// turn into a conflict message for a port that does not exist.
inline bool isLocalTcpPortFree(int port)
{
    if (!isValidTcpPort(port)) {
        return false;
    }
    QTcpServer probe;
    return probe.listen(QHostAddress::LocalHost, static_cast<quint16>(port));
}

// The first port in `ports` that `isFree` answers false for, or a Port with port 0 when all
// are free. Ports outside 1..65535 are skipped, never reported.
inline Port firstOccupiedPort(const QList<Port>& ports, const std::function<bool(int)>& isFree)
{
    for (const Port& entry : ports) {
        if (!isValidTcpPort(entry.port)) {
            continue;
        }
        if (!isFree(entry.port)) {
            return entry;
        }
    }
    return Port{};
}

} // namespace LocalPortConflictCheck
