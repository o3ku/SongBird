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
bool isProxyPortReady(int port);
std::optional<SpeedTestServiceInternal::ReadyProxy> waitForProxy(
    int socksPort,
    int httpPort,
    int timeoutMs,
    const std::atomic_bool& cancelled,
    const std::function<bool()>& hasProcessExited = {});

} // namespace SpeedTestPortReservation
