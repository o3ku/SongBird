#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/ICoreBackend.h"

// Access to the core-specific configuration of a TUN device.
//
// A TUN device is created by a core process of its own, so the JSON that describes it is a core
// artifact. Front ends ask this layer for it instead of assembling core JSON themselves, which is
// what keeps them from having to include a concrete backend.
namespace AuxiliaryTunConfig {

// Root for an auxiliary core process that creates the TUN device, in the given routing mode.
// Returns an empty object when `coreType` cannot back a TUN device or is not registered.
inline QJsonObject buildRoot(CoreType coreType, const Config& config, AuxiliaryTunRouting routing)
{
    const ICoreBackend* backend = coreBackend(coreType);
    if (backend == nullptr) {
        return {};
    }

    return backend->buildAuxiliaryTunClientRoot(config, routing);
}

} // namespace AuxiliaryTunConfig
