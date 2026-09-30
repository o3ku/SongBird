#pragma once

#include <atomic>
#include <functional>
#include <optional>

#include "services/SpeedTestServiceInternal.h"

namespace SpeedTestPortReservation {

struct Ports {
    int socksPort = 0;
    int httpPort = 0;
    int locationProbePort = 0;
};

Ports takeAvailable();
void release(const Ports& ports);

// Single-port variant for the batch runner: it only opens one SOCKS inbound
// per entry, so reserving the full socks/http/location-probe triple per entry
// wasted two thirds of the temporary ports and made the "find three free
// ports" scan fail more often on large batches, which silently downgraded
// the whole group to the slow per-item path.
int takeSocksPort();
void releaseSocksPort(int socksPort);
bool isProxyPortReady(int port);
std::optional<SpeedTestServiceInternal::ReadyProxy> waitForProxy(
    int socksPort,
    int httpPort,
    int timeoutMs,
    const std::atomic_bool& cancelled,
    const std::function<bool()>& hasProcessExited = {});

} // namespace SpeedTestPortReservation
