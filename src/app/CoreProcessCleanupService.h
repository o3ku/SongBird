#pragma once

#include <QStringList>

#include "domain/models/Config.h"

class CoreProcessCleanupService
{
public:
    QStringList cleanupOrphanCoreProcesses() const;
    QStringList cleanupCoreProcessesUsingConfiguredPorts(const Config& config, int locationProbePortOffset) const;
};
