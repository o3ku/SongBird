#pragma once

#include <optional>

#include <QString>
#include <QStringList>

#include "common/OperationResult.h"
#include "common/SystemProxyMode.h"
#include "common/SystemProxySnapshot.h"
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

    // The snapshot of the user's own proxy settings, taken before the first write of this session.
    //
    // Ownership of the system proxy is the one piece of state that decides whether a failed
    // startup or a stop is allowed to touch the registry at all, and the answer cannot be derived
    // from the config (that says what the user wanted) or from isSystemProxyEnabled() (that says
    // the registry is on, not who turned it on). It has to be remembered.
    //
    // Defaulted to "not captured" so a host that does not manage the system proxy -- the auto
    // front end, and most tests -- does not have to implement it; the callers fall back to the
    // pre-snapshot behaviour when `valid` is false.
    virtual SystemProxySnapshot capturedSystemProxySnapshot() const { return {}; }

    // Captures now, and only the first time. The two calls are separate on purpose: the teardown
    // paths must ask "what did we capture?" without ever capturing again, because by then the
    // registry holds our own port and a fresh capture would record it as the user's value.
    virtual SystemProxySnapshot ensureSystemProxySnapshotCaptured()
    {
        return capturedSystemProxySnapshot();
    }

    // Writes `snapshot` back to the system. Defaulted to "nothing to do, it worked" so hosts
    // that do not manage the system proxy stay unaffected; callers only reach this when the
    // snapshot is restorable, which those hosts never report.
    virtual bool restoreSystemProxySnapshot(const SystemProxySnapshot& snapshot)
    {
        Q_UNUSED(snapshot);
        return true;
    }
};
