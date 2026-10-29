#include "auto/AutoTunRuntime.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

#include "appcore/CoreDiscoveryService.h"
#include "appcore/ProxySession.h"
#include "common/AppPlatform.h"
#include "common/AppPaths.h"
#include "runtime/AuxiliaryTunConfig.h"
#include "runtime/ICoreProcessHost.h"

AutoTunRuntime::AutoTunRuntime(Dependencies deps, QObject* parent)
    : QObject(parent)
    , deps_(std::move(deps))
{
}

OperationResult AutoTunRuntime::syncForConfig(
    const Config& config,
    Routing routing,
    ProxySession* session)
{
    if (!config.tun().tunModeItem.enableTun) {
        shutdown(true);
        return OperationResult::ok();
    }
    if (isRunningWithRouting(routing)) {
        return OperationResult::ok();
    }
    if (isWindowsPlatform() && (deps_.isProcessElevated ? !deps_.isProcessElevated() : false)) {
        return OperationResult::fail(QStringLiteral("TUN requires administrator privileges"));
    }
    return start(config, routing);
}

void AutoTunRuntime::shutdown(bool cleanupAdapter)
{
    stop(cleanupAdapter);
}

bool AutoTunRuntime::isRunningWithRouting(Routing routing) const
{
    return deps_.tunCore.isRunning() && activeRouting_ == static_cast<int>(routing);
}

OperationResult AutoTunRuntime::start(const Config& config, Routing routing)
{
    stop(false);
    {
        const OperationResult cleanupResult = deps_.runtimeService.removeStaleAdapterIfPresent();
        if (!cleanupResult.success) {
            // Rebuilt rather than returned as-is: the caller composes this message into the status
            // line, so it must not carry a `cancelled` flag it did not earn.
            return OperationResult::fail(cleanupResult.message);
        }
    }

    const CoreInfo coreInfo = resolveSingBoxCoreInfo();
    if (coreInfo.program.trimmed().isEmpty()) {
        return OperationResult::fail(
            QStringLiteral("TUN requires sing-box, but no sing-box executable was found."));
    }
    const QString configPath = writeRuntimeConfig(config, routing);
    if (configPath.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Failed to write TUN runtime config."));
    }

    activeRouting_ = static_cast<int>(routing);
    const OperationResult startResult = deps_.tunCore.start(
        coreInfo,
        configPath,
        [this](const QString& line) {
            // The core's own output is routed through the coordinator's log so the "tun | " prefix
            // and every other presentation decision stays there; this class only forwards.
            emit tunOutput(line);
        },
        [this](const QString& message) { emit tunStarted(message); },
        // Logged, not published: the caller composes one status from this result, so a status
        // written here would only be overwritten by the caller's version of the same fact.
        [this](const QString& message) { emit tunStartFailed(message); },
        [this, config](int exitCode, QProcess::ExitStatus, bool stopRequested) {
            if (!stopRequested && config.tun().tunModeItem.enableTun) {
                emit tunExitedUnexpectedly(exitCode);
            }
        });
    if (!startResult.success) {
        activeRouting_ = -1;
    }
    return startResult;
}

void AutoTunRuntime::stop(bool cleanupAdapter)
{
    deps_.tunCore.stop(true);
    activeRouting_ = -1;
    if (cleanupAdapter) {
        const OperationResult cleanupResult = deps_.runtimeService.removeStaleAdapterIfPresent();
        emit adapterCleanupResult(cleanupResult);
    }
}

QString AutoTunRuntime::writeRuntimeConfig(const Config& config, Routing routing) const
{
    const QString runtimeDirectory = AppPaths::runtimeDirectory();
    if (!QDir().mkpath(runtimeDirectory)) {
        return {};
    }

    // The TUN device is created by a sing-box process of its own, so its configuration is a core
    // artifact: it is assembled by the core backend, not here. Writing it ourselves is what used to
    // nest the outbound list one level deep and hand sing-box a config it refused to load.
    const QJsonObject root = AuxiliaryTunConfig::buildRoot(
        CoreType::SingBox,
        config,
        routing == Routing::RelayToLocalProxy
            ? AuxiliaryTunRouting::RelayToLocalProxy
            : AuxiliaryTunRouting::DirectOnly);
    if (root.isEmpty()) {
        return {};
    }

    const QString configPath = QDir(runtimeDirectory).filePath(QStringLiteral("auto-tun.generated.json"));
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {};
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return configPath;
}

CoreInfo AutoTunRuntime::resolveSingBoxCoreInfo() const
{
    CoreInfo info;
    info.type = CoreType::SingBox;
    const QString program = deps_.coreDiscovery == nullptr
        ? QString()
        : deps_.coreDiscovery->locateFirstExistingFile(
              deps_.coreDiscovery->resolveCoreCandidates(CoreType::SingBox, deps_.configPath));
    if (program.trimmed().isEmpty()) {
        return {};
    }
    info.program = program;
    info.arguments.clear();
    return info;
}

void AutoTunRuntime::undoSessionProxyAfterAbortedStart(ProxySession* session) const
{
    if (session == nullptr) {
        return;
    }
    // Issue #8's chain, kept whole: a start that dies before ProxySession::start() runs means
    // nothing on that path undoes a proxy an earlier round applied. The ownership decision
    // lives in the session, never here -- only it knows whether the registry holds our values.
    session->undoManagedSystemProxyAfterAbortedStartup();
}
