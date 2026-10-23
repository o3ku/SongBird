#pragma once

#include <QStringList>

#include "domain/models/Config.h"

class CoreProcessCleanupService
{
public:
    QStringList cleanupOrphanCoreProcesses() const;
    // The ports come from LocalInboundPorts, not from a caller-supplied offset: a second way to
    // spell the probe port is exactly what let the applied proxy port and the bound port diverge.
    QStringList cleanupCoreProcessesUsingConfiguredPorts(const Config& config) const;
};
