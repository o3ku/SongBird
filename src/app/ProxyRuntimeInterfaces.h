#pragma once

#include <optional>

#include <QString>
#include <QStringList>

#include "common/OperationResult.h"
#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"
#include "runtime/CoreInfo.h"

class IRuntimeProfileResolver
{
public:
    virtual ~IRuntimeProfileResolver() = default;

    virtual std::optional<VmessItem> resolveActiveServer() const = 0;
    virtual CoreType resolveLaunchCoreType(const VmessItem& server) const = 0;
    virtual CoreInfo resolveCoreInfo(const VmessItem& server) const = 0;
    virtual QString resolveRuntimeConfigPath(const VmessItem& server) const = 0;
    virtual QStringList resolveCoreCandidates(CoreType coreType) const = 0;
    virtual QString locateFirstExistingFile(const QStringList& candidates) const = 0;
    virtual QString currentIndexId() const = 0;
    virtual CoreType resolveRuntimeCoreType(CoreType coreType) const = 0;
    virtual QString resolveCoreInstallDirectory(CoreType coreType) const = 0;
};

class IRuntimeEnvironment
{
public:
    virtual ~IRuntimeEnvironment() = default;

    virtual void cleanupPortProcesses() = 0;
    virtual OperationResult removeStaleTunAdapter() = 0;
    virtual bool skipCoreChecks() const = 0;
    virtual bool isWindowsPlatform() const = 0;
    virtual bool isProcessElevated() const = 0;
};

class IProxyActivationCoordinator
{
public:
    virtual ~IProxyActivationCoordinator() = default;

    virtual void cancelBackgroundTasksForStartup() = 0;
    virtual void refreshExistingCoreTypes() = 0;
    virtual bool isSystemProxyEnabled() const = 0;
    virtual bool updateSystemProxyMode(SystemProxyMode mode) = 0;
};
