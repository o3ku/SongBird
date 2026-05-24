#include "app/ProxySession.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>

#include "app/OutboundLocationProbeService.h"
#include "app/TunSettingsApplyDecision.h"
#include "app/TunRuntimeState.h"
#include "runtime/ClientConfigWriter.h"
#include "runtime/CoreConfigPreflight.h"
#include "runtime/TunCompatCoreRequirement.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/ICoreBackend.h"
#include "services/CoreUpdateService.h"
#include "services/GeoResourceUpdateService.h"

namespace {

constexpr int CoreStartupOverlayMinimumVisibleMs = 650;
constexpr int CoreStartupCompletionOverlayDelayMs = 2000;
constexpr unsigned long BackgroundThreadShutdownWaitMs = 2000;
const QString kSingBoxRuleSetDirectoryName = QStringLiteral("rule-set");

QStringList missingSingBoxRuleSetTagsFor(const QString& configPath)
{
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }

    const QJsonArray ruleSets = document.object()
                                    .value(QStringLiteral("route"))
                                    .toObject()
                                    .value(QStringLiteral("rule_set"))
                                    .toArray();
    QStringList missingTags;
    QSet<QString> seenTags;
    const QString ruleSetDirectory = QDir(QCoreApplication::applicationDirPath()).filePath(kSingBoxRuleSetDirectoryName);
    for (const QJsonValue& value : ruleSets) {
        const QJsonObject ruleSet = value.toObject();
        if (ruleSet.value(QStringLiteral("type")).toString() != QStringLiteral("remote")) {
            continue;
        }
        const QString tag = ruleSet.value(QStringLiteral("tag")).toString().trimmed().toLower();
        if (tag.isEmpty() || seenTags.contains(tag)) {
            continue;
        }
        seenTags.insert(tag);
        const QString filename = QStringLiteral("%1.srs").arg(tag);
        const QFileInfo localRuleSet(QDir(ruleSetDirectory).filePath(filename));
        if (!localRuleSet.exists() || !localRuleSet.isFile() || localRuleSet.size() <= 0) {
            missingTags.append(tag);
        }
    }
    return missingTags;
}

bool waitForLocalTcpPortReady(int port, int timeoutMs)
{
    if (port <= 0 || port > 65535 || timeoutMs <= 0) {
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(port));
        if (socket.waitForConnected(500)) {
            socket.disconnectFromHost();
            return true;
        }
        QThread::msleep(200);
    }
    return false;
}

QString overlayDetailText(QString detail)
{
    detail = detail.trimmed();
    const int urlSeparatorIndex = detail.indexOf(QStringLiteral(": http"), 0, Qt::CaseInsensitive);
    if (urlSeparatorIndex > 0) {
        return detail.left(urlSeparatorIndex).trimmed();
    }
    return detail;
}

QString tunAdminRequiredStartMessage()
{
    return QCoreApplication::translate(
        "ProxySession",
        "TUN mode requires administrator privileges on Windows. Run SongBird as administrator before starting the core.");
}

} // namespace

ProxySession::ProxySession(Dependencies deps, QObject* parent)
    : QObject(parent)
    , deps_(deps)
{
}

ProxySession::~ProxySession()
{
    shuttingDown_ = true;
    lifetimeGuard_.reset();
    cancelPendingCoreRestarts();
    const QList<QPointer<QThread>> threads = backgroundThreads_;
    for (const QPointer<QThread>& thread : threads) {
        if (thread != nullptr && thread->isRunning()) {
            thread->requestInterruption();
            thread->quit();
            while (!thread->wait(BackgroundThreadShutdownWaitMs)) {
                thread->requestInterruption();
            }
        }
    }
    backgroundThreads_.clear();
}

void ProxySession::start(const StartRequest& request)
{
    currentRequest_ = request;
    startInternal(
        request.skipTunCleanup,
        false,
        request.showOverlay);
}

void ProxySession::stop(bool immediate)
{
    stopInternal(immediate, true);
}

void ProxySession::restartIfRunning(const StartRequest& request)
{
    if (!deps_.mainCore.isRunning()) {
        return;
    }

    currentRequest_ = request;
    postStopAction_ = PostStop::Restart{request.showOverlay};
    stopInternal(false, false);
}

void ProxySession::switchServer(const QString& indexId, bool enableTun, bool showOverlay)
{
    if (!deps_.mainCore.isRunning()) {
        return;
    }

    postStopAction_ = PostStop::SwitchDefaultServer{indexId, enableTun, showOverlay};
    stopInternal(false, false);
}

void ProxySession::stopForCoreUpdate()
{
    if (!deps_.mainCore.isRunning()) {
        return;
    }

    postStopAction_ = PostStop::CoreUpdate{};
    stopInternal(false, false);
}

ProxySession::Phase ProxySession::phase() const
{
    return phase_;
}

bool ProxySession::isActive() const
{
    return phase_ == Phase::Proxying;
}

bool ProxySession::isCoreRunning() const
{
    return deps_.mainCore.isRunning();
}

bool ProxySession::isActivationInProgress() const
{
    return phase_ != Phase::Stopped
        && phase_ != Phase::Proxying
        && phase_ != Phase::Stopping;
}

bool ProxySession::isTransitioning() const
{
    return phase_ != Phase::Stopped && phase_ != Phase::Proxying;
}

QString ProxySession::serverLocation() const
{
    return currentServerLocation_;
}

QString ProxySession::serverWarning() const
{
    return currentServerWarning_;
}

bool ProxySession::isManagedProxyActive() const
{
    return managedSystemProxyActive_;
}

void ProxySession::adoptManagedSystemProxy(bool active)
{
    if (managedSystemProxyActive_ == active) {
        return;
    }
    managedSystemProxyActive_ = active;
    emit statusSyncRequested();
}

void ProxySession::requestChecklistOverlay()
{
    if (checklistSteps_.isEmpty()) {
        return;
    }

    checklistOverlayRequested_ = true;
    syncChecklistOverlay();
}

// --- Phase management ---

void ProxySession::setPhase(Phase phase)
{
    if (phase_ == phase) {
        return;
    }
    phase_ = phase;
    emit phaseChanged(phase);
    emit statusSyncRequested();
}

// --- Startup flow ---

void ProxySession::startInternal(
    bool skipTunCleanup,
    bool environmentAlreadyChecked,
    bool showStartupOverlay)
{
    const QString environmentStep = tr("Environment cleanup");
    const QString downloadCoreStep = tr("Validate core application");
    const QString checkRuntimeResourcesStep = tr("Validate runtime resources");
    const QString validateConfigStep = tr("Validate core config");
    const QString startTunRuntimeStep = tr("Start TUN runtime");
    const QString startCoreStep = tr("Start core process");

    if (!skipTunCleanup && !environmentAlreadyChecked) {
        prepareChecklist(showStartupOverlay);
    } else if (checklistSteps_.isEmpty()) {
        prepareChecklist(showStartupOverlay);
    } else if (showStartupOverlay && !checklistOverlayRequested_) {
        checklistOverlayRequested_ = true;
        syncChecklistOverlay();
    }

    if (deps_.mainCore.isRunning() || phase_ == Phase::Stopping) {
        keepChecklistForUserDismissal(startCoreStep,
            phase_ == Phase::Stopping
                ? QStringLiteral("Core stop is already in progress.")
                : QStringLiteral("Core start is already in progress."));
        return;
    }

    beginActivation();
    if (!environmentAlreadyChecked) {
        coreTunAdapterConflictRetryCount_ = 0;
    }
    cancelPendingCoreRestarts();
    clearServerLocation();
    deps_.resolver.cleanupPortProcesses();

    if (isTunRuntimeBlocked(
            currentRequest_.config,
            deps_.resolver.isWindowsPlatform(),
            deps_.resolver.isProcessElevated())) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, environmentStep,
            QStringLiteral("Administrator permission is required for TUN."));
        failStartup(OperationResult::fail(tunAdminRequiredStartMessage()));
        return;
    }

    // TUN cleanup phase
    if (currentRequest_.config.tun().tunModeItem.enableTun && !environmentAlreadyChecked) {
        if (tunCleanupState_ != TunCleanupState::Idle) {
            keepChecklistForUserDismissal(environmentStep,
                QStringLiteral("TUN adapter cleanup is already in progress."));
            return;
        }

        setCheckpointStatus(CoreStartupCheckpointStatus::Started, environmentStep,
            QStringLiteral("Removing stale TUN adapter if needed."));
        setTunCleanupState(TunCleanupState::CleaningThenResume);
        removeStaleTunAdapterAsync([this, environmentStep](const OperationResult& result) {
            const bool resumeStart = tunCleanupState_ == TunCleanupState::CleaningThenResume;
            setTunCleanupState(TunCleanupState::Idle);
            const bool shouldResume = shouldResumeCoreStartAfterTunCleanup(
                result.success, resumeStart, shuttingDown_.load(), phase_ == Phase::Stopping);
            if (!shouldResume) {
                setCheckpointStatus(
                    result.success ? CoreStartupCheckpointStatus::Passed : CoreStartupCheckpointStatus::Failed,
                    environmentStep, result.message);
                if (!result.success) {
                    failStartup(result);
                } else {
                    keepChecklistForUserDismissal(environmentStep,
                        QStringLiteral("Core startup was canceled before TUN cleanup finished."));
                    cancelActivation();
                    emit statusSyncRequested();
                }
                return;
            }

            setCheckpointStatus(CoreStartupCheckpointStatus::Passed, environmentStep, result.message);
            startInternal(true, true, checklistOverlayRequested_);
        });
        return;
    }

    if (environmentAlreadyChecked
        && checklistStepStatus_.value(environmentStep) == CoreStartupCheckpointStatus::Pending) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Passed, environmentStep,
            QStringLiteral("Environment cleanup completed."));
    } else if (!environmentAlreadyChecked) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Passed, environmentStep,
            skipTunCleanup
                ? QStringLiteral("TUN cleanup completed.")
                : QStringLiteral("No TUN cleanup required."));
    }

    // Validate core application
    setPhase(Phase::ValidateCoreApplication);
    const std::optional<VmessItem> server = deps_.resolver.resolveActiveServer();
    if (!server.has_value()) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, validateConfigStep,
            QStringLiteral("No active server."));
        failStartup(OperationResult::fail(QStringLiteral("No active server is available for runtime config generation.")));
        return;
    }
    currentServer_ = *server;

    const CoreType launchCore = deps_.resolver.resolveLaunchCoreType(*server);
    const CoreInfo coreInfo = deps_.resolver.resolveCoreInfo(*server);
    if (coreInfo.program.isEmpty()) {
        const QStringList candidates = deps_.resolver.resolveCoreCandidates(launchCore);
        const OperationResult missingCoreResult = OperationResult::fail(
            tr("No compatible %1 core executable was found for the active server.")
                .arg(coreTypeDisplayName(launchCore)));
        downloadMissingCoreAndResume(launchCore, missingCoreResult, downloadCoreStep,
            skipTunCleanup, showStartupOverlay);
        return;
    }
    if (checklistStepStatus_.value(downloadCoreStep) == CoreStartupCheckpointStatus::Pending) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Passed, downloadCoreStep,
            QStringLiteral("Required core executable is available."));
    }

    // Validate config
    const QString coreConfigPath = deps_.resolver.resolveRuntimeConfigPath(*server);
    if (coreConfigPath.isEmpty()) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, validateConfigStep,
            QStringLiteral("Runtime config path is empty."));
        failStartup(OperationResult::fail(QStringLiteral("Failed to resolve the runtime config output path.")));
        return;
    }

    setPhase(Phase::ValidateCoreConfig);
    setCheckpointStatus(CoreStartupCheckpointStatus::Started, validateConfigStep,
        QStringLiteral("Generating runtime config."));

    QStringList auxiliaryConfigPaths;
    deps_.resolver.removeStaleSingBoxCache();
    deps_.configWriter.setExistingCoreTypes(currentRequest_.existingCoreTypes);
    const OperationResult writeResult = deps_.configWriter.writeClientConfigs(
        currentRequest_.config, *server, coreConfigPath, &auxiliaryConfigPaths);
    if (!writeResult.success) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, validateConfigStep, writeResult.message);
        failStartup(writeResult);
        return;
    }

    setPhase(Phase::ValidateRuntimeResources);
    if (launchCore == CoreType::SingBox) {
        const QStringList missingRuleSets = missingSingBoxRuleSetTagsFor(coreConfigPath);
        if (!missingRuleSets.isEmpty()) {
            downloadMissingSingBoxRuleSetsAndResume(missingRuleSets, checkRuntimeResourcesStep,
                skipTunCleanup, showStartupOverlay);
            return;
        }
    }

    const OperationResult runtimeResourcesResult = validateCoreGeoFilesBeforeStart(coreInfo);
    if (!runtimeResourcesResult.success) {
        if (!coreUsesLegacyGeoFiles(coreInfo)) {
            setCheckpointStatus(CoreStartupCheckpointStatus::Failed, checkRuntimeResourcesStep,
                runtimeResourcesResult.message);
            failStartup(runtimeResourcesResult);
            return;
        }
        downloadMissingGeoFilesAndResume(coreInfo, runtimeResourcesResult.message,
            checkRuntimeResourcesStep, skipTunCleanup, showStartupOverlay);
        return;
    }
    setCheckpointStatus(CoreStartupCheckpointStatus::Passed, checkRuntimeResourcesStep,
        runtimeResourcesResult.message);

    if (!deps_.resolver.skipCoreChecks()) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Started, validateConfigStep,
            QFileInfo(coreInfo.program).fileName());
        const OperationResult preflightResult = validateCoreConfigBeforeStart(coreInfo, coreConfigPath);
        if (!preflightResult.success) {
            setCheckpointStatus(CoreStartupCheckpointStatus::Failed, validateConfigStep, preflightResult.message);
            failStartup(preflightResult);
            return;
        }
        setCheckpointStatus(CoreStartupCheckpointStatus::Passed, validateConfigStep, preflightResult.message);
    } else {
        setCheckpointStatus(CoreStartupCheckpointStatus::Skipped, validateConfigStep,
            QStringLiteral("Startup checks are disabled."));
    }

    // Start TUN runtime (auxiliary core)
    if (!auxiliaryConfigPaths.isEmpty()) {
        const QList<CoreType> auxiliaryCoreTypes =
            resolveAuxiliaryTunCompatCoreTypes(currentRequest_.config, *server, currentRequest_.existingCoreTypes);
        const CoreType auxiliaryCoreType = auxiliaryCoreTypes.isEmpty()
            ? CoreType::Unknown
            : auxiliaryCoreTypes.constFirst();
        const ICoreBackend* auxiliaryBackend = coreBackend(auxiliaryCoreType);
        setPhase(Phase::StartTunRuntime);
        const QString auxiliaryProgram = deps_.resolver.locateFirstExistingFile(
            deps_.resolver.resolveCoreCandidates(auxiliaryCoreType));
        if (auxiliaryProgram.isEmpty()) {
            const OperationResult missingAuxiliaryResult = OperationResult::fail(
                QStringLiteral("TUN compatibility mode requires %1, but no compatible %1 executable was found.")
                    .arg(coreTypeDisplayName(auxiliaryCoreType)));
            downloadMissingCoreAndResume(auxiliaryCoreType, missingAuxiliaryResult,
                downloadCoreStep, skipTunCleanup, showStartupOverlay);
            return;
        }

        CoreInfo auxiliaryCoreInfo;
        auxiliaryCoreInfo.type = auxiliaryCoreType;
        auxiliaryCoreInfo.program = auxiliaryProgram;
        if (auxiliaryBackend != nullptr) {
            auxiliaryCoreInfo.arguments = auxiliaryBackend->launchArguments(auxiliaryCoreInfo.configPlaceholder);
            auxiliaryCoreInfo.appendConfigArgument = auxiliaryBackend->appendConfigArgument();
        }
        auxiliaryCoreInfo.workingDirectory = QFileInfo(auxiliaryProgram).absolutePath();

        if (!deps_.resolver.skipCoreChecks()) {
            setCheckpointStatus(CoreStartupCheckpointStatus::Started, startTunRuntimeStep,
                QStringLiteral("Validating TUN compatibility relay."));
            const OperationResult auxiliaryPreflightResult = validateCoreConfigBeforeStart(
                auxiliaryCoreInfo, auxiliaryConfigPaths.constFirst());
            if (!auxiliaryPreflightResult.success) {
                setCheckpointStatus(CoreStartupCheckpointStatus::Failed, startTunRuntimeStep,
                    auxiliaryPreflightResult.message);
                failStartup(auxiliaryPreflightResult);
                return;
            }
        }

        stopAuxiliaryCore();
        setCheckpointStatus(CoreStartupCheckpointStatus::Started, startTunRuntimeStep,
            QStringLiteral("Starting TUN compatibility relay core."));
        const OperationResult auxiliaryStartResult = deps_.auxiliaryCore.start(
            auxiliaryCoreInfo, auxiliaryConfigPaths.constFirst(),
            [this](const QString& line) { emit auxiliaryCoreOutput(line); },
            [](const QString&) {},
            [](const QString&) {},
            [this](int code, QProcess::ExitStatus st, bool stopped) {
                handleCoreExited(code, static_cast<int>(st), stopped, true);
            });
        if (!auxiliaryStartResult.success) {
            stopAuxiliaryCore();
            setCheckpointStatus(CoreStartupCheckpointStatus::Failed, startTunRuntimeStep,
                auxiliaryStartResult.message);
            failStartup(auxiliaryStartResult);
            return;
        }
        setCheckpointStatus(CoreStartupCheckpointStatus::Passed, startTunRuntimeStep,
            auxiliaryStartResult.message);
    } else {
        if (currentRequest_.config.tun().tunModeItem.enableTun) {
            setPhase(Phase::StartTunRuntime);
            setCheckpointStatus(CoreStartupCheckpointStatus::Started, startTunRuntimeStep,
                QStringLiteral("TUN runtime will be started by the core process."));
        }
        stopAuxiliaryCore();
    }

    // Start core process
    CoreInfo launchCoreInfo = coreInfo;
    launchCoreInfo.asyncStart = true;

    const QString serverIndexId = server->indexId;
    const bool customServer = (server->configType == ConfigType::Custom);

    setPhase(Phase::StartCoreProcess);
    coreTunEnabledAtStart_ = currentRequest_.config.tun().tunModeItem.enableTun;
    coreTunAdapterConflictDetected_ = false;
    setCheckpointStatus(CoreStartupCheckpointStatus::Started, startCoreStep,
        QFileInfo(launchCoreInfo.program).fileName());

    const OperationResult startResult = deps_.mainCore.start(
        launchCoreInfo, coreConfigPath,
        [this](const QString& line) { emit coreOutput(line); },
        [this, serverIndexId, customServer](const QString& message) {
            coreStartTime_ = QDateTime::currentMSecsSinceEpoch();
            setCheckpointStatus(CoreStartupCheckpointStatus::Started, tr("Start core process"), message);
            if (coreTunEnabledAtStart_
                && checklistStepStatus_.value(tr("Start TUN runtime")) == CoreStartupCheckpointStatus::Started) {
                setCheckpointStatus(CoreStartupCheckpointStatus::Passed, tr("Start TUN runtime"),
                    QStringLiteral("TUN runtime startup is handled by the core process."));
            }
            validateCoreListeningAndHandleStarted(serverIndexId, customServer);
        },
        [this](const QString& msg) { handleCoreStartFailed(msg); },
        [this](int code, QProcess::ExitStatus st, bool stopped) {
            handleCoreExited(code, static_cast<int>(st), stopped, false);
        });

    if (!startResult.success) {
        const bool tunEnabledAtFailedStart = coreTunEnabledAtStart_;
        coreTunEnabledAtStart_ = false;
        stopAuxiliaryCore();
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, startCoreStep, startResult.message);
        if (tunEnabledAtFailedStart
            && checklistStepStatus_.value(startTunRuntimeStep) == CoreStartupCheckpointStatus::Started) {
            setCheckpointStatus(CoreStartupCheckpointStatus::Failed, startTunRuntimeStep, startResult.message);
        }
        failStartup(startResult);
        return;
    }
    setCheckpointStatus(CoreStartupCheckpointStatus::Started, startCoreStep, startResult.message);
    emit statusSyncRequested();
}

void ProxySession::beginActivation()
{
    const bool alreadyActivating = isActivationInProgress();
    if (!alreadyActivating && deps_.resolver.cancelBackgroundTasksForStartup) {
        deps_.resolver.cancelBackgroundTasksForStartup();
    }
    setPhase(Phase::EnvironmentCleanup);
}

void ProxySession::finishActivation()
{
    setPhase(Phase::Proxying);
    emit activated(currentServerLocation_);
}

void ProxySession::cancelActivation()
{
    if (phase_ != Phase::Stopping) {
        setPhase(Phase::Stopped);
    }
}
// --- Checklist ---

void ProxySession::prepareChecklist(bool showOverlay)
{
    checklistSteps_.clear();
    checklistSteps_
        << tr("Environment cleanup")
        << tr("Validate core application")
        << tr("Validate runtime resources")
        << tr("Validate core config");
    if (currentRequest_.config.tun().tunModeItem.enableTun) {
        checklistSteps_.append(tr("Start TUN runtime"));
    }
    checklistSteps_
        << tr("Start core process")
        << tr("Check outbound location")
        << tr("Apply system proxy");
    checklistStepStatus_.clear();
    checklistStepDetails_.clear();
    checklistOverlayRequested_ = showOverlay;
    checklistOverlayShown_ = false;
    ++checklistStableRunGeneration_;
    for (const QString& step : checklistSteps_) {
        checklistStepStatus_.insert(step, CoreStartupCheckpointStatus::Pending);
    }

    if (showOverlay) {
        syncChecklistOverlay();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
}

void ProxySession::setCheckpointStatus(
    CoreStartupCheckpointStatus status,
    const QString& step,
    const QString& detail)
{
    if (!checklistSteps_.contains(step)) {
        checklistSteps_.append(step);
    }
    checklistStepStatus_.insert(step, status);
    checklistStepDetails_.insert(step, detail.trimmed());

    if (!checklistOverlayRequested_) {
        return;
    }

    syncChecklistOverlay();
}

void ProxySession::clearChecklist()
{
    if (checklistOverlayShown_ && checklistOverlayShownAtMs_ > 0) {
        const qint64 elapsedMs = QDateTime::currentMSecsSinceEpoch() - checklistOverlayShownAtMs_;
        if (elapsedMs < CoreStartupOverlayMinimumVisibleMs) {
            const int generation = ++checklistClearGeneration_;
            QTimer::singleShot(
                static_cast<int>(CoreStartupOverlayMinimumVisibleMs - elapsedMs),
                this,
                [this, generation]() {
                    if (generation == checklistClearGeneration_) {
                        clearChecklist();
                    }
                });
            return;
        }
    }

    checklistSteps_.clear();
    checklistStepStatus_.clear();
    checklistStepDetails_.clear();
    const bool shouldClearOverlay = checklistOverlayShown_;
    checklistOverlayRequested_ = false;
    checklistOverlayShown_ = false;
    checklistOverlayShownAtMs_ = 0;
    ++checklistClearGeneration_;
    ++checklistStableRunGeneration_;
    if (shouldClearOverlay) {
        emit checklistCleared();
    }
}

void ProxySession::clearChecklistAfterStableRun(int delayMs)
{
    const int generation = ++checklistStableRunGeneration_;
    QTimer::singleShot(delayMs, this, [this, generation]() {
        if (generation != checklistStableRunGeneration_) {
            return;
        }
        if (phase_ != Phase::Proxying) {
            return;
        }
        clearChecklist();
    });
}

void ProxySession::keepChecklistForUserDismissal(const QString& step, const QString& detail)
{
    checklistKeepOnNextStop_ = true;
    setCheckpointStatus(CoreStartupCheckpointStatus::Failed, step, detail);
}

void ProxySession::syncChecklistOverlay()
{
    if (!checklistOverlayRequested_) {
        return;
    }

    QStringList items;
    items.reserve(checklistSteps_.size());
    for (const QString& itemStep : checklistSteps_) {
        const CoreStartupCheckpointStatus itemStatus = checklistStepStatus_.value(
            itemStep,
            CoreStartupCheckpointStatus::Pending);
        items.append(coreStartupChecklistItem(
            itemStatus,
            itemStep,
            overlayDetailText(checklistStepDetails_.value(itemStep))));
    }
    emit checklistUpdated(items);
    checklistOverlayShown_ = true;
    if (checklistOverlayShownAtMs_ <= 0) {
        checklistOverlayShownAtMs_ = QDateTime::currentMSecsSinceEpoch();
    }
}

// --- Startup failure ---

void ProxySession::failStartup(const OperationResult& result)
{
    cleanupAfterFailedStartup();
    if (currentRequest_.warnOnFailure) {
        setServerWarning(tr("Please verify manually"));
    }
    emit failed(result.message);
}

void ProxySession::cleanupAfterFailedStartup()
{
    cancelPendingCoreRestarts();
    currentServer_ = {};

    const bool proxyConfigured =
        normalizeSystemProxyMode(currentRequest_.config.sysProxyType) != SystemProxyMode::ForcedClear
        || currentRequest_.config.ui().mainProxyEnabled;
    const bool proxyApplied = deps_.resolver.isSystemProxyEnabled
        ? deps_.resolver.isSystemProxyEnabled()
        : false;
    if (proxyConfigured || proxyApplied || managedSystemProxyActive_) {
        const bool proxyCleared = updateSystemProxyMode(SystemProxyMode::ForcedClear);
        emit logMessage(proxyCleared
            ? QStringLiteral("System proxy disabled because core startup failed.")
            : QStringLiteral("Failed to disable system proxy after core startup failed."));
        if (proxyCleared) {
            managedSystemProxyActive_ = false;
        }
    }
    clearProxyStateAfterStopped();

    if (deps_.mainCore.isRunning()) {
        checklistKeepOnNextStop_ = true;
        setPhase(Phase::Stopping);
        const OperationResult stopResult = deps_.mainCore.stop(false);
        emit logMessage(stopResult.message);
    } else {
        setPhase(Phase::Stopped);
    }
    stopAuxiliaryCore(false);

    cancelActivation();
    coreTunEnabledAtStart_ = false;
    emit statusSyncRequested();
}

// --- Download and resume ---

void ProxySession::downloadMissingGeoFilesAndResume(
    const CoreInfo& coreInfo,
    const QString& geoValidationMessage,
    const QString& checkGeoStep,
    bool skipTunCleanup,
    bool showStartupOverlay)
{
    const QString targetDirectory = coreInfo.workingDirectory.trimmed().isEmpty()
        ? QFileInfo(coreInfo.program).absolutePath()
        : coreInfo.workingDirectory;
    if (targetDirectory.trimmed().isEmpty()) {
        failStartup(OperationResult::fail(QStringLiteral("Core working directory is empty.")));
        return;
    }

    const QString startMessage = tr("Missing legacy geo files detected. Downloading to %1.")
        .arg(QDir::toNativeSeparators(targetDirectory));
    setCheckpointStatus(CoreStartupCheckpointStatus::Started, checkGeoStep, startMessage);
    emit logMessage(geoValidationMessage);

    setPhase(Phase::ValidateRuntimeResources);

    const std::weak_ptr<char> guard = lifetimeGuard_;
    QThread* thread = QThread::create([this, targetDirectory, checkGeoStep, skipTunCleanup,
                                       showStartupOverlay, guard]() {
        const auto reportProgress = [this, checkGeoStep, guard](const QString& message) {
            if (message.trimmed().isEmpty()) {
                return;
            }
            QMetaObject::invokeMethod(this, [this, checkGeoStep, message, guard]() {
                if (guard.expired() || !isActivationInProgress()) {
                    return;
                }
                setCheckpointStatus(CoreStartupCheckpointStatus::Started, checkGeoStep, message);
            }, Qt::QueuedConnection);
        };

        GeoResourceUpdateService geoUpdateService(targetDirectory, {}, reportProgress);
        const QList<OperationResult> results{
            geoUpdateService.update(QStringLiteral("geosite")),
            geoUpdateService.update(QStringLiteral("geoip"))};

        bool success = true;
        QStringList messages;
        for (const OperationResult& r : results) {
            success = success && r.success;
            if (!r.message.trimmed().isEmpty()) {
                messages.append(r.message.trimmed());
            }
        }
        const OperationResult result = success
            ? OperationResult::ok(messages.join(QChar('\n')))
            : OperationResult::fail(messages.join(QChar('\n')));

        QMetaObject::invokeMethod(this, [this, result, checkGeoStep, skipTunCleanup,
                                         showStartupOverlay, guard]() {
            if (guard.expired() || shuttingDown_.load()) {
                return;
            }
            const bool startupCanceled = !isActivationInProgress();
            if (startupCanceled) {
                keepChecklistForUserDismissal(checkGeoStep,
                    tr("Proxy startup was canceled while Geo files were updating."));
                cancelActivation();
                emit statusSyncRequested();
                return;
            }
            if (!result.success) {
                setCheckpointStatus(CoreStartupCheckpointStatus::Failed, checkGeoStep, result.message);
                failStartup(result);
                return;
            }
            if (normalizeSystemProxyMode(currentRequest_.config.sysProxyType) != SystemProxyMode::ForcedChange) {
                keepChecklistForUserDismissal(checkGeoStep,
                    tr("Geo files were updated, but proxy startup was canceled."));
                cancelActivation();
                emit statusSyncRequested();
                return;
            }
            setCheckpointStatus(CoreStartupCheckpointStatus::Passed, checkGeoStep, result.message);
            startInternal(skipTunCleanup, true, showStartupOverlay);
        }, Qt::QueuedConnection);
    });
    trackBackgroundThread(thread);
    thread->start();
}

void ProxySession::downloadMissingSingBoxRuleSetsAndResume(
    const QStringList& missingRuleSetTags,
    const QString& checkGeoStep,
    bool skipTunCleanup,
    bool showStartupOverlay)
{
    QStringList tags;
    QSet<QString> seenTags;
    for (const QString& tag : missingRuleSetTags) {
        const QString normalizedTag = tag.trimmed().toLower();
        if (normalizedTag.isEmpty() || seenTags.contains(normalizedTag)) {
            continue;
        }
        seenTags.insert(normalizedTag);
        tags.append(normalizedTag);
    }

    if (tags.isEmpty()) {
        startInternal(skipTunCleanup, true, showStartupOverlay);
        return;
    }

    const QString targetDirectory = QCoreApplication::applicationDirPath();
    const QString startMessage = tr("Missing sing-box rule sets detected. Downloading %1 file(s) to %2.")
        .arg(tags.size())
        .arg(QDir::toNativeSeparators(QDir(targetDirectory).filePath(kSingBoxRuleSetDirectoryName)));
    setCheckpointStatus(CoreStartupCheckpointStatus::Started, checkGeoStep, startMessage);

    setPhase(Phase::ValidateRuntimeResources);

    const std::weak_ptr<char> guard = lifetimeGuard_;
    QThread* thread = QThread::create([this, targetDirectory, tags, checkGeoStep, skipTunCleanup,
                                       showStartupOverlay, guard]() {
        const auto reportProgress = [this, checkGeoStep, guard](const QString& message) {
            if (message.trimmed().isEmpty()) {
                return;
            }
            QMetaObject::invokeMethod(this, [this, checkGeoStep, message, guard]() {
                if (guard.expired() || !isActivationInProgress()) {
                    return;
                }
                setCheckpointStatus(CoreStartupCheckpointStatus::Started, checkGeoStep, message);
            }, Qt::QueuedConnection);
        };

        GeoResourceUpdateService geoUpdateService(targetDirectory, {}, reportProgress);
        QList<OperationResult> results;
        for (const QString& tag : tags) {
            results.append(geoUpdateService.updateSingBoxRuleSet(tag));
        }

        bool success = true;
        QStringList messages;
        for (const OperationResult& r : results) {
            success = success && r.success;
            if (!r.message.trimmed().isEmpty()) {
                messages.append(r.message.trimmed());
            }
        }
        const OperationResult result = success
            ? OperationResult::ok(messages.join(QChar('\n')))
            : OperationResult::fail(messages.join(QChar('\n')));

        QMetaObject::invokeMethod(this, [this, result, checkGeoStep, skipTunCleanup,
                                         showStartupOverlay, guard]() {
            if (guard.expired() || shuttingDown_.load()) {
                return;
            }
            const bool startupCanceled = !isActivationInProgress();
            if (startupCanceled) {
                keepChecklistForUserDismissal(checkGeoStep,
                    tr("Proxy startup was canceled while sing-box rule sets were updating."));
                cancelActivation();
                emit statusSyncRequested();
                return;
            }
            if (!result.success) {
                setCheckpointStatus(CoreStartupCheckpointStatus::Failed, checkGeoStep, result.message);
                failStartup(result);
                return;
            }
            if (normalizeSystemProxyMode(currentRequest_.config.sysProxyType) != SystemProxyMode::ForcedChange) {
                keepChecklistForUserDismissal(checkGeoStep,
                    tr("sing-box rule sets were updated, but proxy startup was canceled."));
                cancelActivation();
                emit statusSyncRequested();
                return;
            }
            setCheckpointStatus(CoreStartupCheckpointStatus::Passed, checkGeoStep, result.message);
            startInternal(skipTunCleanup, true, showStartupOverlay);
        }, Qt::QueuedConnection);
    });
    trackBackgroundThread(thread);
    thread->start();
}

void ProxySession::downloadMissingCoreAndResume(
    CoreType coreType,
    const OperationResult& missingCoreResult,
    const QString& downloadCoreStep,
    bool skipTunCleanup,
    bool showStartupOverlay)
{
    const CoreType runtimeCore = deps_.resolver.resolveRuntimeCoreType(coreType);
    const QString installDirectory = deps_.resolver.resolveCoreInstallDirectory(runtimeCore);
    if (runtimeCore == CoreType::Unknown || installDirectory.trimmed().isEmpty()) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, downloadCoreStep, missingCoreResult.message);
        failStartup(missingCoreResult);
        return;
    }

    const QString startMessage = tr("Missing %1 core detected. Downloading to %2.")
        .arg(coreTypeDisplayName(runtimeCore))
        .arg(QDir::toNativeSeparators(installDirectory));
    setCheckpointStatus(CoreStartupCheckpointStatus::Started, downloadCoreStep, startMessage);
    emit logMessage(missingCoreResult.message);

    const BackgroundTaskCoordinator::Token token =
        deps_.backgroundTasks.tryBeginStartupTask(BackgroundTaskCoordinator::Kind::CoreUpdate);
    if (!token.isValid()) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, downloadCoreStep,
            tr("Another background task is already running."));
        failStartup(OperationResult::fail(
            tr("Cannot download %1 while another background task is running.")
                .arg(coreTypeDisplayName(runtimeCore))));
        return;
    }

    setPhase(Phase::ValidateCoreApplication);

    const CoreUpdateConfig workerConfig{
        currentRequest_.config.checkPreReleaseUpdate,
        currentRequest_.config.ignoreGeoUpdateCore};
    const std::weak_ptr<char> guard = lifetimeGuard_;

    QThread* thread = QThread::create([this, runtimeCore, workerConfig, installDirectory,
                                       downloadCoreStep, skipTunCleanup,
                                       showStartupOverlay, token, guard]() {
        CoreUpdateService coreUpdateService;
        CoreUpdateService::UpdateOptions options;
        options.progressHandler = [this, downloadCoreStep, token, guard](const QString& message) {
            if (message.trimmed().isEmpty()) {
                return;
            }
            QMetaObject::invokeMethod(this, [this, downloadCoreStep, message, token, guard]() {
                if (guard.expired()) {
                    return;
                }
                if (!deps_.backgroundTasks.isCurrent(token)) {
                    return;
                }
                if (!isActivationInProgress()) {
                    return;
                }
                setCheckpointStatus(CoreStartupCheckpointStatus::Started, downloadCoreStep, message);
            }, Qt::QueuedConnection);
        };
        options.cancelCheck = [this]() {
            QThread* currentThread = QThread::currentThread();
            return shuttingDown_.load()
                || (currentThread != nullptr && currentThread->isInterruptionRequested());
        };
        options.skipLocalVersionCheck = true;

        const OperationResult result = coreUpdateService.update(
            runtimeCore, workerConfig, installDirectory, options);

        QMetaObject::invokeMethod(this, [this, runtimeCore, downloadCoreStep, skipTunCleanup,
                                         showStartupOverlay, token, result, guard]() {
            if (guard.expired()) {
                return;
            }
            if (!deps_.backgroundTasks.isCurrent(token)) {
                return;
            }
            deps_.backgroundTasks.finish(token);
            if (shuttingDown_.load()) {
                return;
            }
            const bool startupCanceled = !isActivationInProgress();
            if (startupCanceled) {
                keepChecklistForUserDismissal(downloadCoreStep,
                    tr("Proxy startup was canceled while the core was downloading."));
                cancelActivation();
                emit statusSyncRequested();
                return;
            }
            if (!result.success) {
                setCheckpointStatus(CoreStartupCheckpointStatus::Failed, downloadCoreStep, result.message);
                failStartup(result);
                return;
            }
            if (normalizeSystemProxyMode(currentRequest_.config.sysProxyType) != SystemProxyMode::ForcedChange) {
                keepChecklistForUserDismissal(downloadCoreStep,
                    tr("The core was downloaded, but proxy startup was canceled."));
                cancelActivation();
                emit statusSyncRequested();
                return;
            }
            deps_.resolver.refreshExistingCoreTypes();
            setCheckpointStatus(CoreStartupCheckpointStatus::Passed, downloadCoreStep, result.message);
            startInternal(skipTunCleanup, true, showStartupOverlay);
        }, Qt::QueuedConnection);
    });
    trackBackgroundThread(thread);
    thread->start();
}
// --- Core started/failed ---

void ProxySession::handleCoreStarted(const QString& serverIndexId)
{
    setServerWarning({});
    if (phase_ == Phase::Stopping) {
        return;
    }
    setPhase(Phase::CheckOutboundLocation);
    queryServerLocation(serverIndexId);
}

void ProxySession::validateCoreListeningAndHandleStarted(
    const QString& serverIndexId,
    bool customServer)
{
    const QString startCoreStep = tr("Start core process");
    const bool usesDedicatedProbe = !customServer;
    const int listenPort = OutboundLocationProbeService::resolveHttpPort(currentRequest_.config, usesDedicatedProbe);
    if (listenPort <= 0 || listenPort > 65535) {
        const QString message = QStringLiteral("Local proxy listen port is unavailable.");
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, startCoreStep, message);
        failStartup(OperationResult::fail(message));
        return;
    }

    const std::weak_ptr<char> guard = lifetimeGuard_;
    const QString activeServerId = serverIndexId.trimmed();
    const qint64 startTime = coreStartTime_;
    const bool tunEnabledAtStart = coreTunEnabledAtStart_;
    constexpr int kCoreProbeTotalTimeoutMs = 8000;
    constexpr int kTunCoreProbeTotalTimeoutMs = 20000;
    const int listenProbeTimeoutMs = tunEnabledAtStart ? kTunCoreProbeTotalTimeoutMs : kCoreProbeTotalTimeoutMs;

    QThread* thread = QThread::create([this, guard, activeServerId,
                                       startCoreStep, startTime,
                                       listenPort, listenProbeTimeoutMs]() {
        const bool portReady = waitForLocalTcpPortReady(listenPort, listenProbeTimeoutMs);

        QMetaObject::invokeMethod(this, [this, guard, activeServerId,
                                         startCoreStep, startTime,
                                         listenPort, listenProbeTimeoutMs, portReady]() {
            if (guard.expired()) {
                return;
            }
            if (coreStartTime_ != startTime || deps_.resolver.currentIndexId() != activeServerId) {
                setCheckpointStatus(CoreStartupCheckpointStatus::Skipped, startCoreStep,
                    QStringLiteral("Core listening validation result is stale."));
                if (coreStartTime_ == startTime && deps_.mainCore.isRunning() && phase_ != Phase::Stopping) {
                    stop(false);
                }
                return;
            }

            if (!portReady) {
                const QString message = QStringLiteral(
                    "Core did not open the local proxy port %1 within %2 seconds.")
                    .arg(listenPort)
                    .arg((listenProbeTimeoutMs + 999) / 1000);
                setCheckpointStatus(CoreStartupCheckpointStatus::Failed, startCoreStep, message);
                failStartup(OperationResult::fail(message));
                return;
            }

            setCheckpointStatus(CoreStartupCheckpointStatus::Passed, startCoreStep,
                QStringLiteral("Core local proxy is listening on 127.0.0.1:%1.").arg(listenPort));
            handleCoreStarted(activeServerId);
        }, Qt::QueuedConnection);
    });
    trackBackgroundThread(thread);
    thread->start();
}

void ProxySession::handleCoreStartFailed(const QString& message)
{
    const bool tunEnabledAtFailedStart = coreTunEnabledAtStart_;
    setCheckpointStatus(CoreStartupCheckpointStatus::Failed, tr("Start core process"), message);
    if (tunEnabledAtFailedStart
        && checklistStepStatus_.value(tr("Start TUN runtime")) == CoreStartupCheckpointStatus::Started) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, tr("Start TUN runtime"), message);
    }
    coreTunEnabledAtStart_ = false;
    failStartup(OperationResult::fail(message));
    stopAuxiliaryCore();
    clearServerLocation();
}

// --- Location probe ---

void ProxySession::queryServerLocation(const QString& serverIndexId)
{
    setPhase(Phase::CheckOutboundLocation);
    const QString locationStep = tr("Check outbound location");
    if (!deps_.mainCore.isRunning() || serverIndexId.trimmed().isEmpty()) {
        const QString message = QStringLiteral("Core is not ready for outbound location detection.");
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, locationStep, message);
        failStartup(OperationResult::fail(message));
        return;
    }

    clearServerLocation();
    setCheckpointStatus(CoreStartupCheckpointStatus::Started, locationStep,
        QStringLiteral("Detecting outbound IP location."));

    const bool usesDedicatedProbe = currentServer_.configType != ConfigType::Custom;
    const int httpPort = OutboundLocationProbeService::resolveHttpPort(currentRequest_.config, usesDedicatedProbe);
    if (httpPort <= 0 || httpPort > 65535) {
        const QString message = QStringLiteral("Location probe port is unavailable.");
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, locationStep, message);
        failStartup(OperationResult::fail(message));
        return;
    }

    const std::weak_ptr<char> guard = lifetimeGuard_;
    const QString activeServerId = serverIndexId.trimmed();
    const qint64 startTime = coreStartTime_;
    QThread* thread = QThread::create([this, guard, activeServerId, httpPort, locationStep, startTime]() {
        const OutboundLocationProbeResult probeResult = deps_.locationProbe.probe(httpPort);
        const QString location = probeResult.location;
        const QString lastError = probeResult.error;

        QMetaObject::invokeMethod(this, [this, guard, activeServerId, location, lastError, locationStep, startTime]() {
            if (guard.expired()) {
                return;
            }
            if (!deps_.mainCore.isRunning()
                || coreStartTime_ != startTime
                || deps_.resolver.currentIndexId() != activeServerId) {
                setCheckpointStatus(CoreStartupCheckpointStatus::Failed, locationStep,
                    QStringLiteral("Outbound location detection result is stale."));
                cancelActivation();
                return;
            }
            setServerLocation(location);
            if (currentServerLocation_.isEmpty()) {
                const QString message = lastError.trimmed().isEmpty()
                    ? QStringLiteral("Outbound location unavailable.")
                    : lastError;
                setCheckpointStatus(CoreStartupCheckpointStatus::Failed, locationStep, message);
                failStartup(OperationResult::fail(message));
                return;
            }
            setCheckpointStatus(CoreStartupCheckpointStatus::Passed, locationStep, currentServerLocation_);
            if (applySystemProxyAfterLocation()) {
                clearChecklistAfterStableRun(CoreStartupCompletionOverlayDelayMs);
            }
        }, Qt::QueuedConnection);
    });
    trackBackgroundThread(thread);
    thread->start();
}

bool ProxySession::applySystemProxyAfterLocation()
{
    const QString proxyStep = tr("Apply system proxy");
    const SystemProxyMode configuredMode = normalizeSystemProxyMode(currentRequest_.config.sysProxyType);

    if (configuredMode != SystemProxyMode::ForcedChange) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, proxyStep,
            QStringLiteral("Global system proxy mode is not enabled."));
        failStartup(OperationResult::fail(QStringLiteral("Global system proxy mode is not enabled.")));
        return false;
    }

    setPhase(Phase::ApplySystemProxy);
    setCheckpointStatus(CoreStartupCheckpointStatus::Started, proxyStep,
        QStringLiteral("Applying Global system proxy."));
    const bool proxyUpdated = updateSystemProxyMode(SystemProxyMode::ForcedChange);
    if (!proxyUpdated) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, proxyStep,
            QStringLiteral("Failed to apply the configured Global system proxy."));
        failStartup(OperationResult::fail(
            QStringLiteral("Failed to apply the configured Global system proxy after starting the core.")));
        emit statusSyncRequested();
        return false;
    }
    setCheckpointStatus(CoreStartupCheckpointStatus::Passed, proxyStep,
        QStringLiteral("Global system proxy is active."));
    managedSystemProxyActive_ = true;

    finishActivation();
    emit statusSyncRequested();
    return true;
}

// --- Stop flow ---

void ProxySession::stopInternal(bool immediate, bool clearPostStopAction)
{
    const bool keepChecklist = checklistKeepOnNextStop_;
    checklistKeepOnNextStop_ = false;
    const bool checklistVisible = checklistOverlayShown_ || checklistOverlayRequested_;
    const bool coreStopPending = !immediate && deps_.mainCore.isRunning();
    setPhase(coreStopPending ? Phase::Stopping : Phase::Stopped);

    const bool hasRestartOrSwitchPending = !clearPostStopAction
        && (std::holds_alternative<PostStop::Restart>(postStopAction_)
            || std::holds_alternative<PostStop::SwitchDefaultServer>(postStopAction_));
    if (!checklistVisible && !hasRestartOrSwitchPending && !keepChecklist) {
        clearChecklist();
    }
    clearServerLocation();
    cancelPendingCoreRestarts();

    if (tunCleanupState_ == TunCleanupState::CleaningThenResume) {
        setTunCleanupState(TunCleanupState::Cleaning);
    }
    if (clearPostStopAction) {
        postStopAction_ = std::monostate{};
    }
    clearAppliedProxyAfterStopped();
    if (clearPostStopAction) {
        clearProxyStateAfterStopped();
    }

    skipTunCleanupOnNextCoreExit_ = immediate
        && shouldCleanupTunAfterCoreStop(deps_.resolver.isWindowsPlatform(), coreTunEnabledAtStart_);
    const bool cleanupTunSynchronously = skipTunCleanupOnNextCoreExit_;

    deps_.mainCore.stop(immediate);
    stopAuxiliaryCore(true);

    if (cleanupTunSynchronously) {
        setTunCleanupState(TunCleanupState::Idle);
        removeStaleTunAdapter();
    }
    cancelActivation();
    emit statusSyncRequested();
}

void ProxySession::handleCoreExited(int exitCode, int status, bool stopRequested, bool auxiliary)
{
    if (!auxiliary) {
        setPhase(Phase::Stopped);
        if (std::holds_alternative<std::monostate>(postStopAction_)) {
            cancelActivation();
        }
        if (stopRequested && !checklistOverlayShown_ && !checklistOverlayRequested_) {
            clearChecklist();
        }
    }

    const bool cleanupTunAfterStop = !auxiliary && shouldCleanupTunAfterCoreStop(
        deps_.resolver.isWindowsPlatform(),
        coreTunEnabledAtStart_);
    const bool tunEnabledAtCoreExit = !auxiliary && coreTunEnabledAtStart_;
    const bool tunAdapterConflictDetected = !auxiliary && coreTunAdapterConflictDetected_;
    const bool cleanupAlreadyHandledOnImmediateStop = !auxiliary && skipTunCleanupOnNextCoreExit_;
    if (!auxiliary) {
        coreTunEnabledAtStart_ = false;
        coreTunAdapterConflictDetected_ = false;
        skipTunCleanupOnNextCoreExit_ = false;
    }

    const auto* pendingRestart = std::get_if<PostStop::Restart>(&postStopAction_);
    const auto* pendingSwitch = std::get_if<PostStop::SwitchDefaultServer>(&postStopAction_);
    const bool switchAfterStop = !auxiliary
        && stopRequested
        && pendingSwitch != nullptr;
    const bool restartAfterStop = !auxiliary && stopRequested && pendingRestart != nullptr && !switchAfterStop;
    const bool coreUpdateAfterStop = !auxiliary
        && stopRequested
        && std::holds_alternative<PostStop::CoreUpdate>(postStopAction_);
    const bool restartShowOverlay = pendingRestart != nullptr ? pendingRestart->showStartupOverlay : false;
    const QString switchIndexId = pendingSwitch != nullptr ? pendingSwitch->indexId : QString{};
    const bool switchEnableTun = pendingSwitch != nullptr && pendingSwitch->enableTun;
    const bool switchShowOverlay = pendingSwitch != nullptr && pendingSwitch->showStartupOverlay;
    if (restartAfterStop || switchAfterStop || coreUpdateAfterStop) {
        postStopAction_ = std::monostate{};
    }
    if (!auxiliary) {
        clearAppliedProxyAfterStopped();
    }

    emit statusSyncRequested();
    if (cleanupAlreadyHandledOnImmediateStop) {
        emit stopped();
        return;
    }

    if (cleanupTunAfterStop && stopRequested) {
        if (tunCleanupState_ != TunCleanupState::Idle) {
            emit stopped();
            return;
        }

        setTunCleanupState(TunCleanupState::Cleaning);
        removeStaleTunAdapterAsync([this,
                                    coreUpdateAfterStop,
                                    switchAfterStop,
                                    switchIndexId,
                                    switchEnableTun,
                                    switchShowOverlay,
                                    restartAfterStop,
                                    restartShowOverlay](const OperationResult& result) {
            setTunCleanupState(TunCleanupState::Idle);
            emit logMessage(result.message);
            if (shouldRunPostStopActionAfterTunCleanup(
                    result.success,
                    coreUpdateAfterStop,
                    shuttingDown_.load())) {
                emit coreUpdateResumeRequested();
                return;
            }
            if (shouldRunPostStopActionAfterTunCleanup(result.success, switchAfterStop, shuttingDown_.load())) {
                emit serverSwitchResumeRequested(switchIndexId, switchEnableTun, switchShowOverlay);
                return;
            }
            if (shouldRunPostStopActionAfterTunCleanup(result.success, restartAfterStop, shuttingDown_.load())) {
                scheduleCoreStartAfterStop(restartShowOverlay);
            }
        });
        if (shuttingDown_.load() || stopRequested) {
            emit stopped();
            return;
        }
    } else if (coreUpdateAfterStop && !shuttingDown_.load()) {
        emit coreUpdateResumeRequested();
    } else if (switchAfterStop && !shuttingDown_.load()) {
        emit serverSwitchResumeRequested(switchIndexId, switchEnableTun, switchShowOverlay);
    } else if (restartAfterStop && !shuttingDown_.load()) {
        scheduleCoreStartAfterStop(restartShowOverlay);
    }

    if (shuttingDown_.load() || stopRequested) {
        emit stopped();
        return;
    }

    // Crash restart logic
    const QProcess::ExitStatus exitStatus = static_cast<QProcess::ExitStatus>(status);
    constexpr int kMaxTunAdapterConflictRestarts = 1;
    if (!auxiliary
        && shouldRetryAfterTunAdapterConflict(
            deps_.resolver.isWindowsPlatform(),
            tunEnabledAtCoreExit,
            tunAdapterConflictDetected,
            coreTunAdapterConflictRetryCount_,
            kMaxTunAdapterConflictRestarts)) {
        ++coreTunAdapterConflictRetryCount_;
        scheduleCoreRestart(
            QStringLiteral("TUN adapter conflict detected (code=%1). Cleaning 'singbox_tun' before retry...")
                .arg(exitCode),
            false,
            1000);
        return;
    }
    if (!auxiliary && tunAdapterConflictDetected) {
        emit logMessage(QStringLiteral("TUN adapter conflict persisted after cleanup retry. Auto-restart disabled."));
        clearProxyStateAfterStopped();
        emit statusSyncRequested();
        emit stopped();
        return;
    }

    int& crashCount = auxiliary ? auxiliaryCrashRestartCount_ : coreCrashRestartCount_;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 runDuration = coreStartTime_ > 0 ? (now - coreStartTime_) : 0;

    constexpr qint64 kStableRunThresholdMs = 60000;
    if (runDuration > kStableRunThresholdMs) {
        crashCount = 0;
    }
    ++crashCount;

    constexpr int kMaxCrashRestarts = 5;
    if (crashCount > kMaxCrashRestarts) {
        const QString core = auxiliary ? QStringLiteral("Auxiliary core") : QStringLiteral("Core");
        const QString kind = exitStatus == QProcess::CrashExit ? QStringLiteral("crash") : QStringLiteral("exit");
        emit logMessage(QStringLiteral("%1 %2 detected (code=%3). Auto-restart disabled after %4 consecutive failures.")
            .arg(core, kind).arg(exitCode).arg(kMaxCrashRestarts));
        if (!auxiliary) {
            emit transientStatus(
                tr("Core crashed %1 times. Auto-restart disabled. Check your configuration.").arg(kMaxCrashRestarts), 0);
            clearProxyStateAfterStopped();
            emit statusSyncRequested();
        }
        emit stopped();
        return;
    }

    const QString core = auxiliary ? QStringLiteral("Auxiliary core") : QStringLiteral("Core");
    const QString kind = exitStatus == QProcess::CrashExit ? QStringLiteral("crash") : QStringLiteral("exit");
    const int delayMs = std::min(3000 * crashCount, 30000);
    scheduleCoreRestart(
        QStringLiteral("%1 %2 detected (code=%3). Restarting in %4s... (attempt %5/%6)")
            .arg(core, kind).arg(exitCode).arg(delayMs / 1000).arg(crashCount).arg(kMaxCrashRestarts),
        auxiliary,
        delayMs);
}

void ProxySession::scheduleCoreStartAfterStop(bool showStartupOverlay)
{
    QTimer::singleShot(50, this, [this, showStartupOverlay]() {
        if (shuttingDown_.load()) {
            return;
        }
        startInternal(false, false, showStartupOverlay);
    });
}

void ProxySession::clearAppliedProxyAfterStopped()
{
    const bool systemProxyEnabled = deps_.resolver.isSystemProxyEnabled
        ? deps_.resolver.isSystemProxyEnabled()
        : false;
    if (!shouldClearManagedSystemProxy(managedSystemProxyActive_, systemProxyEnabled)) {
        return;
    }

    const bool proxyCleared = updateSystemProxyMode(SystemProxyMode::ForcedClear);
    emit logMessage(proxyCleared
        ? QStringLiteral("System proxy disabled because the core stopped.")
        : QStringLiteral("Failed to disable system proxy after the core stopped."));
    if (proxyCleared) {
        managedSystemProxyActive_ = false;
    }
}

void ProxySession::clearProxyStateAfterStopped()
{
    managedSystemProxyActive_ = false;
    currentServer_ = {};
    currentServerLocation_.clear();
    currentServerWarning_.clear();
}
// --- TUN cleanup ---

void ProxySession::setTunCleanupState(TunCleanupState state)
{
    tunCleanupState_ = state;
}

void ProxySession::removeStaleTunAdapterAsync(const std::function<void(const OperationResult&)>& completion)
{
    const std::weak_ptr<char> guard = lifetimeGuard_;
    QThread* thread = QThread::create([this, completion, guard]() {
        const OperationResult result = deps_.resolver.removeStaleTunAdapter
            ? deps_.resolver.removeStaleTunAdapter()
            : OperationResult::fail(QStringLiteral("TUN runtime service is unavailable."));
        QMetaObject::invokeMethod(this, [completion, result, guard]() {
            if (guard.expired()) {
                return;
            }
            if (completion) {
                completion(result);
            }
        }, Qt::QueuedConnection);
    });
    trackBackgroundThread(thread);
    thread->start();
}

void ProxySession::removeStaleTunAdapter()
{
    const OperationResult result = deps_.resolver.removeStaleTunAdapter
        ? deps_.resolver.removeStaleTunAdapter()
        : OperationResult::fail(QStringLiteral("TUN runtime service is unavailable."));
    emit logMessage(result.message);
}

void ProxySession::stopAuxiliaryCore(bool immediate)
{
    deps_.auxiliaryCore.stop(immediate);
}

// --- Crash restart ---

void ProxySession::scheduleCoreRestart(const QString& reason, bool auxiliary, int delayMs)
{
    if (shuttingDown_.load()) {
        return;
    }
    if (!reason.isEmpty()) {
        emit logMessage(reason);
    }
    QTimer*& slot = auxiliary ? auxiliaryRestartTimer_ : coreRestartTimer_;
    if (slot == nullptr) {
        slot = new QTimer(this);
        slot->setSingleShot(true);
        QObject::connect(slot, &QTimer::timeout, this, [this, auxiliary]() {
            if (shuttingDown_.load()) {
                return;
            }

            if (!auxiliary && currentRequest_.config.tun().tunModeItem.enableTun) {
                if (tunCleanupState_ != TunCleanupState::Idle) {
                    return;
                }

                setTunCleanupState(TunCleanupState::Cleaning);
                removeStaleTunAdapterAsync([this](const OperationResult& result) {
                    setTunCleanupState(TunCleanupState::Idle);
                    emit logMessage(result.message);
                    if (shouldResumeCoreStartAfterTunCleanup(
                            result.success,
                            true,
                            shuttingDown_.load(),
                            phase_ == Phase::Stopping)) {
                        if (!checklistSteps_.isEmpty()) {
                            setCheckpointStatus(
                                CoreStartupCheckpointStatus::Passed,
                                tr("Environment cleanup"),
                                result.message);
                        }
                        startInternal(true, true, false);
                    }
                });
                return;
            }

            startInternal(false, false, false);
        });
    }
    slot->start(delayMs);
}

void ProxySession::cancelPendingCoreRestarts()
{
    if (coreRestartTimer_ != nullptr) {
        coreRestartTimer_->stop();
    }
    if (auxiliaryRestartTimer_ != nullptr) {
        auxiliaryRestartTimer_->stop();
    }
}

// --- Server location ---

void ProxySession::setServerLocation(QString location)
{
    location = location.trimmed();
    if (currentServerLocation_ == location) {
        return;
    }
    currentServerLocation_ = std::move(location);
    emit statusSyncRequested();
}

void ProxySession::setServerWarning(QString warning)
{
    warning = warning.trimmed();
    if (currentServerWarning_ == warning) {
        return;
    }
    currentServerWarning_ = std::move(warning);
    emit statusSyncRequested();
}

void ProxySession::clearServerLocation()
{
    if (currentServerLocation_.isEmpty() && currentServerWarning_.isEmpty()) {
        return;
    }
    currentServerLocation_.clear();
    currentServerWarning_.clear();
    emit statusSyncRequested();
}

// --- Utilities ---

bool ProxySession::updateSystemProxyMode(SystemProxyMode mode) const
{
    if (deps_.resolver.updateSystemProxyMode) {
        return deps_.resolver.updateSystemProxyMode(mode);
    }
    return false;
}

void ProxySession::trackBackgroundThread(QThread* thread)
{
    if (thread == nullptr) {
        return;
    }

    for (int index = backgroundThreads_.size() - 1; index >= 0; --index) {
        const QPointer<QThread>& trackedThread = backgroundThreads_.at(index);
        if (trackedThread.isNull() || !trackedThread->isRunning()) {
            backgroundThreads_.removeAt(index);
        }
    }

    QPointer<QThread> guard(thread);
    backgroundThreads_.append(guard);
    QObject::connect(thread, &QThread::finished, this, [this, guard]() {
        backgroundThreads_.removeAll(guard);
    });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
}
