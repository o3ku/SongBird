#include "appcore/ProxySession.h"

#include <utility>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRandomGenerator>
#include <QSet>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>

#include "appcore/OutboundLivenessProbeService.h"
#include "appcore/OutboundLocationProbeService.h"
#include "common/ServerDisplayName.h"
#include "appcore/TunSettingsApplyDecision.h"
#include "appcore/TunRuntimeState.h"
#include "common/AppPaths.h"
#include "common/BackgroundThreadLaunch.h"
#include "common/SingBoxPaths.h"
#include "common/ThreadShutdown.h"
#include "runtime/ClientConfigWriter.h"
#include "runtime/CoreConfigPreflight.h"
#include "runtime/TunCompatCoreRequirement.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/ICoreBackend.h"
#include "services/CoreUpdateService.h"
#include "services/GeoResourceUpdateService.h"

namespace {

constexpr int CoreStartupCompletionOverlayDelayMs = 2000;

// The correlation handle as a log field. "-" rather than an empty value so the field is always
// present and a grep for "id=" does not silently skip the lines written before the first activation.
QString activationTag(const QString& activationId)
{
    return QStringLiteral("id=%1").arg(activationId.isEmpty() ? QStringLiteral("-") : activationId);
}

// The machine half of a gate log line: key=value, so the line stays greppable and its fields can be
// read without a parser. Assembled from parts rather than written as one template, because a single
// template reading "id=%1 port=%2 ..." is a prose literal that the localization scan would report.
QString outboundGateFacts(const QString& activationId, int port, int hosts, qint64 elapsedMs)
{
    QStringList parts;
    parts << activationTag(activationId);
    // A field the caller has no value for is left out rather than written as a zero, so a skipped
    // check does not read as one that tried nothing in no time.
    if (port > 0) {
        parts << QStringLiteral("port=%1").arg(port);
    }
    if (hosts > 0) {
        parts << QStringLiteral("hosts=%1").arg(hosts);
    }
    if (elapsedMs >= 0) {
        parts << QStringLiteral("elapsed=%1ms").arg(elapsedMs);
    }
    return parts.join(QStringLiteral(" "));
}

// The DNS the core was actually started with. A "the gate failed because of DNS" report cannot be
// argued either way without this line: the config path the Activating line already carries says
// nothing about which resolvers ended up in effect, and the core's own resolver errors are the only
// other place they appear -- interleaved with everything else it logs.
QString describeEffectiveDns(const DnsConfigState& dns)
{
    QStringList parts;
    parts << QStringLiteral("directDns=%1").arg(dns.directDns);
    parts << QStringLiteral("remoteDns=%1").arg(dns.remoteDns);
    parts << QStringLiteral("proxyServerDns=%1").arg(
        dns.proxyServerDns.trimmed().isEmpty() ? QStringLiteral("(unset)") : dns.proxyServerDns);
    parts << QStringLiteral("fakeIp=%1").arg(dns.fakeIp ? QStringLiteral("on") : QStringLiteral("off"));
    return parts.join(QStringLiteral(" "));
}

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
    const QString ruleSetDirectory = QDir(AppPaths::applicationDirectory()).filePath(SingBoxPaths::ruleSetDirectoryName());
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

QStringList normalizedUniqueTags(const QStringList& tags)
{
    QStringList normalizedTags;
    QSet<QString> seenTags;
    for (const QString& tag : tags) {
        const QString normalizedTag = tag.trimmed().toLower();
        if (normalizedTag.isEmpty() || seenTags.contains(normalizedTag)) {
            continue;
        }
        seenTags.insert(normalizedTag);
        normalizedTags.append(normalizedTag);
    }
    return normalizedTags;
}

OperationResult combineOperationResults(const QList<OperationResult>& results)
{
    bool success = true;
    QStringList messages;
    for (const OperationResult& result : results) {
        success = success && result.success;
        if (!result.message.trimmed().isEmpty()) {
            messages.append(result.message.trimmed());
        }
    }

    const QString message = messages.join(QChar('\n'));
    return success ? OperationResult::ok(message) : OperationResult::fail(message);
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
    , checklist_(
          this,
          [this](const QStringList& items) { emit checklistUpdated(items); },
          [this]() { emit checklistCleared(); })
{
}

ProxySession::~ProxySession()
{
    shuttingDown_ = true;
    lifetimeGuard_.reset();
    cancelPendingCoreRestarts();
    if (!backgroundThreads_.waitForAll()) {
        // A worker outlived the whole shutdown budget, so it can still reach this object: the
        // workers call QMetaObject::invokeMethod(this, ...) from their own thread, which
        // dereferences `this`. This destructor may therefore neither complete (that frees what the
        // worker is about to touch) nor keep waiting (that is the unbounded hang being fixed).
        // Every production ProxySession lives for the whole process -- AppBootstrapObjects and
        // SongBirdAutoCoordinator each own exactly one, created once and never reset -- so the
        // only honest remaining action is to end the process without unwinding.
        abandonProcessAfterStuckThread(
            QStringLiteral("ProxySession was destroyed with a background thread still running"));
    }
}

void ProxySession::setCoreSwitchConfirmation(CoreSwitchConfirmation confirmation)
{
    coreSwitchConfirmation_ = std::move(confirmation);
}

void ProxySession::setAvailabilityCheck(AvailabilityCheck check, int intervalMs)
{
    availabilityCheck_ = std::move(check);
    healthWatchIntervalMs_ = intervalMs > 0 ? intervalMs : kDefaultHealthWatchIntervalMs;
}

void ProxySession::setDirectConnectivityCheck(DirectConnectivityCheck check)
{
    directConnectivityCheck_ = std::move(check);
}

bool ProxySession::directConnectivityReachable() const
{
    const int timeoutMs = OutboundLivenessProbeService::LivenessProbeDirectTimeoutMs;
    if (directConnectivityCheck_) {
        return directConnectivityCheck_(timeoutMs);
    }
    return deps_.livenessProbe.probeDirect(timeoutMs).reachable;
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

QString ProxySession::activationId() const
{
    return activationId_;
}

bool ProxySession::outboundReachable() const
{
    return outboundReachable_;
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
    checklist_.requestOverlay();
}

// --- Phase management ---

ProxySession::StartupSteps ProxySession::startupSteps()
{
    return {
        tr("Environment cleanup"),
        tr("Validate core application"),
        tr("Validate core config"),
        tr("Validate runtime resources"),
        tr("Start TUN runtime"),
        tr("Start core process")};
}

void ProxySession::setPhase(Phase phase)
{
    if (phase_ == phase) {
        return;
    }
    phase_ = phase;

    // The health watch only means something while the proxy is actually up. Before Proxying the
    // core has not been confirmed listening yet (validateCoreListeningAndHandleStarted), so a
    // failed probe would say nothing about the node; once the session leaves Proxying the node is
    // no longer what the user is talking to, and the core-exit path owns the reporting from there.
    if (phase == Phase::Proxying) {
        // A fresh start supersedes whatever the previous attempt concluded: any warning that is up
        // describes a core that is no longer the one running. The policy is reset with it, so the
        // new watch has to earn its warning again from zero rather than inheriting a count.
        healthWatchPolicy_.reset();
        setServerWarning(QString());
        // A fresh start owns the status bar again: whatever the previous session concluded about
        // the local network is replaced by this session's own gate.
        waivedOutboundWarningShown_ = false;
        directRecheckInProgress_ = false;
        if (directRecheckTimer_ != nullptr) {
            directRecheckTimer_->stop();
        }
        scheduleHealthWatch();
    } else {
        cancelHealthWatch();
    }

    emit phaseChanged(phase);
    emit statusSyncRequested();
}

// --- Startup flow ---

void ProxySession::startInternal(
    bool skipTunCleanup,
    bool environmentAlreadyChecked,
    bool showStartupOverlay)
{
    const StartupContext context{startupSteps(), skipTunCleanup, environmentAlreadyChecked, showStartupOverlay};
    const QString& downloadCoreStep = context.steps.downloadCore;
    const QString& checkRuntimeResourcesStep = context.steps.runtimeResources;
    const QString& validateConfigStep = context.steps.validateConfig;
    const QString& startTunRuntimeStep = context.steps.tunRuntime;
    const QString& startCoreStep = context.steps.startCore;

    ensureChecklistPrepared(context);
    if (rejectOverlappingStart(context)) {
        return;
    }

    beginStartupAttempt(context);
    if (!ensureTunCleanupCompleted(context)) {
        return;
    }

    const std::optional<StartupProfile> profile = resolveStartupProfile(context);
    if (!profile.has_value()) {
        return;
    }

    const VmessItem& server = profile->server;
    const CoreType launchCore = profile->launchCore;
    const CoreInfo& coreInfo = profile->coreInfo;
    const QString& coreConfigPath = profile->coreConfigPath;

    setPhase(Phase::ValidateCoreConfig);
    setCheckpointStatus(CoreStartupCheckpointStatus::Started, validateConfigStep,
        QCoreApplication::translate("ProxySession", "Generating runtime config."));

    QStringList auxiliaryConfigPaths;
    deps_.configWriter.setExistingCoreTypes(currentRequest_.existingCoreTypes);
    deps_.configWriter.setRuntimeCoreInfo(coreInfo);
    const OperationResult writeResult = deps_.configWriter.writeClientConfigs(
        currentRequest_.config, server, coreConfigPath, &auxiliaryConfigPaths);
    if (!writeResult.success) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, validateConfigStep, writeResult.message);
        failStartup(writeResult);
        return;
    }

    const QStringList activationContext{
        QStringLiteral("id=%1").arg(activationId_),
        QStringLiteral("core=%1 (%2)").arg(coreTypeDisplayName(launchCore), QDir::toNativeSeparators(coreInfo.program)),
        QStringLiteral("config=%1").arg(QDir::toNativeSeparators(coreConfigPath)),
        QStringLiteral("tun=%1").arg(currentRequest_.config.tun().tunModeItem.enableTun
            ? QStringLiteral("on") : QStringLiteral("off")),
        QStringLiteral("routing=%1").arg(currentRequest_.config.collection().routingModeId)};
    emit logMessage(tr("Activating %1 | %2")
        .arg(serverDisplayName(server), activationContext.join(QStringLiteral(" | "))));

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
        if (!coreNeedsGeoFiles(coreInfo)) {
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

    if (!deps_.environment.skipCoreChecks()) {
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
            QCoreApplication::translate("ProxySession", "Startup checks are disabled."));
    }

    // Start TUN runtime (auxiliary core)
    if (!auxiliaryConfigPaths.isEmpty()) {
        const QList<CoreType> auxiliaryCoreTypes =
            resolveAuxiliaryTunCompatCoreTypes(currentRequest_.config, server, currentRequest_.existingCoreTypes);
        const CoreType auxiliaryCoreType = auxiliaryCoreTypes.isEmpty()
            ? CoreType::Unknown
            : auxiliaryCoreTypes.constFirst();
        const ICoreBackend* auxiliaryBackend = coreBackend(auxiliaryCoreType);
        setPhase(Phase::StartTunRuntime);
        const QString auxiliaryProgram = deps_.profileResolver.locateFirstExistingFile(
            deps_.profileResolver.resolveCoreCandidates(auxiliaryCoreType));
        if (auxiliaryProgram.isEmpty()) {
            const OperationResult missingAuxiliaryResult = OperationResult::fail(
                QCoreApplication::translate("ProxySession", "TUN compatibility mode requires %1, but no compatible %1 executable was found.")
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

        if (!deps_.environment.skipCoreChecks()) {
            setCheckpointStatus(CoreStartupCheckpointStatus::Started, startTunRuntimeStep,
                QCoreApplication::translate("ProxySession", "Validating TUN compatibility relay."));
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
            QCoreApplication::translate("ProxySession", "Starting TUN compatibility relay core."));
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
                QCoreApplication::translate("ProxySession", "TUN runtime will be started by the core process."));
        }
        stopAuxiliaryCore();
    }

    // Start core process
    CoreInfo launchCoreInfo = coreInfo;
    launchCoreInfo.asyncStart = true;

    const QString serverIndexId = server.indexId;
    const bool customServer = (server.configType == ConfigType::Custom);

    setPhase(Phase::StartCoreProcess);
    coreTunEnabledAtStart_ = currentRequest_.config.tun().tunModeItem.enableTun;
    coreTunAdapterConflictDetected_ = false;
    setCheckpointStatus(CoreStartupCheckpointStatus::Started, startCoreStep,
        QFileInfo(launchCoreInfo.program).fileName());

    const OperationResult startResult = deps_.mainCore.start(
        launchCoreInfo, coreConfigPath,
        [this](const QString& line) { handleMainCoreOutput(line); },
        [this, serverIndexId, customServer](const QString& message) {
            coreStartTime_ = QDateTime::currentMSecsSinceEpoch();
            setCheckpointStatus(CoreStartupCheckpointStatus::Started, tr("Start core process"), message);
            if (coreTunEnabledAtStart_
                && checklist_.status(tr("Start TUN runtime")) == CoreStartupCheckpointStatus::Started) {
                setCheckpointStatus(CoreStartupCheckpointStatus::Passed, tr("Start TUN runtime"),
                    QCoreApplication::translate("ProxySession", "TUN runtime startup is handled by the core process."));
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
            && checklist_.status(startTunRuntimeStep) == CoreStartupCheckpointStatus::Started) {
            setCheckpointStatus(CoreStartupCheckpointStatus::Failed, startTunRuntimeStep, startResult.message);
        }
        failStartup(startResult);
        return;
    }
    setCheckpointStatus(CoreStartupCheckpointStatus::Started, startCoreStep, startResult.message);
    emit statusSyncRequested();
}

void ProxySession::ensureChecklistPrepared(const StartupContext& context)
{
    if (!context.skipTunCleanup && !context.environmentAlreadyChecked) {
        prepareChecklist(context.showStartupOverlay);
    } else if (checklist_.isEmpty()) {
        prepareChecklist(context.showStartupOverlay);
    } else if (context.showStartupOverlay && !checklist_.overlayRequested()) {
        checklist_.requestOverlay();
    }
}

bool ProxySession::rejectOverlappingStart(const StartupContext& context)
{
    if (!deps_.mainCore.isRunning() && phase_ != Phase::Stopping) {
        return false;
    }

    keepChecklistForUserDismissal(context.steps.startCore,
        phase_ == Phase::Stopping
            ? QCoreApplication::translate("ProxySession", "Core stop is already in progress.")
            : QCoreApplication::translate("ProxySession", "Core start is already in progress."));
    return true;
}

void ProxySession::beginStartupAttempt(const StartupContext& context)
{
    beginActivation();
    if (!context.environmentAlreadyChecked) {
        crashRestartPolicy_.resetTunConflictRetries();
    }
    cancelPendingCoreRestarts();
    outboundReachable_ = false;
    clearServerLocation();
    deps_.environment.cleanupPortProcesses();
}

bool ProxySession::ensureTunCleanupCompleted(const StartupContext& context)
{
    if (isTunRuntimeBlocked(
            currentRequest_.config,
            deps_.environment.isWindowsPlatform(),
            deps_.environment.isProcessElevated())) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, context.steps.environment,
            QCoreApplication::translate("ProxySession", "Administrator permission is required for TUN."));
        failStartup(OperationResult::fail(tunAdminRequiredStartMessage()));
        return false;
    }

    if (currentRequest_.config.tun().tunModeItem.enableTun && !context.environmentAlreadyChecked) {
        if (tunCleanupState_ != TunCleanupState::Idle) {
            keepChecklistForUserDismissal(context.steps.environment,
                QCoreApplication::translate("ProxySession", "TUN adapter cleanup is already in progress."));
            return false;
        }

        setCheckpointStatus(CoreStartupCheckpointStatus::Started, context.steps.environment,
            QCoreApplication::translate("ProxySession", "Removing stale TUN adapter if needed."));
        setTunCleanupState(TunCleanupState::CleaningThenResume);
        removeStaleTunAdapterAsync([this, context](const OperationResult& result) {
            const bool resumeStart = tunCleanupState_ == TunCleanupState::CleaningThenResume;
            setTunCleanupState(TunCleanupState::Idle);
            const bool shouldResume = shouldResumeCoreStartAfterTunCleanup(
                result.success, resumeStart, shuttingDown_.load(), phase_ == Phase::Stopping);
            if (!shouldResume) {
                setCheckpointStatus(
                    result.success ? CoreStartupCheckpointStatus::Passed : CoreStartupCheckpointStatus::Failed,
                    context.steps.environment, result.message);
                if (!result.success) {
                    failStartup(result);
                } else {
                    keepChecklistForUserDismissal(context.steps.environment,
                        QCoreApplication::translate("ProxySession", "Core startup was canceled before TUN cleanup finished."));
                    cancelActivation();
                    emit statusSyncRequested();
                }
                return;
            }

            setCheckpointStatus(CoreStartupCheckpointStatus::Passed, context.steps.environment, result.message);
            startInternal(true, true, checklist_.overlayRequested());
        });
        return false;
    }

    if (context.environmentAlreadyChecked
        && checklist_.status(context.steps.environment) == CoreStartupCheckpointStatus::Pending) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Passed, context.steps.environment,
            QCoreApplication::translate("ProxySession", "Environment cleanup completed."));
    } else if (!context.environmentAlreadyChecked) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Passed, context.steps.environment,
            context.skipTunCleanup
                ? QCoreApplication::translate("ProxySession", "TUN cleanup completed.")
                : QCoreApplication::translate("ProxySession", "No TUN cleanup required."));
    }

    return true;
}

std::optional<ProxySession::StartupProfile> ProxySession::resolveStartupProfile(const StartupContext& context)
{
    setPhase(Phase::ValidateCoreApplication);
    const std::optional<VmessItem> server = deps_.profileResolver.resolveActiveServer();
    if (!server.has_value()) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, context.steps.validateConfig,
            QCoreApplication::translate("ProxySession", "No active server."));
        failStartup(OperationResult::fail(QCoreApplication::translate("ProxySession", "No active server is available for runtime config generation.")));
        return std::nullopt;
    }
    currentServer_ = *server;

    const CoreLaunchCompatDecision compat = evaluateCoreLaunchCompat(
        currentRequest_.config, *server, currentRequest_.existingCoreTypes);
    if (compat.outcome == CoreLaunchCompatOutcome::NoCompatibleCore) {
        const QString message = coreLaunchCompatFailureMessage(compat);
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, context.steps.validateConfig, message);
        failStartup(OperationResult::fail(message));
        return std::nullopt;
    }
    if (compat.requiresUserDecision()) {
        if (coreSwitchConfirmation_ && !coreSwitchConfirmation_(compat)) {
            const QString message = tr("Startup canceled: %1 cannot run %2 servers.")
                .arg(coreTypeDisplayName(compat.storedCore))
                .arg(configTypeDisplayName(compat.configType));
            setCheckpointStatus(CoreStartupCheckpointStatus::Failed, context.steps.validateConfig, message);
            failStartup(OperationResult::fail(message));
            return std::nullopt;
        }

        emit logMessage(tr("%1 cannot run %2 servers. Using %3 instead.")
            .arg(coreTypeDisplayName(compat.storedCore))
            .arg(configTypeDisplayName(compat.configType))
            .arg(coreTypeDisplayName(compat.resolvedCore)));
    }

    const CoreType launchCore = deps_.profileResolver.resolveLaunchCoreType(*server);
    const CoreInfo coreInfo = deps_.profileResolver.resolveCoreInfo(*server);
    if (coreInfo.program.isEmpty()) {
        const OperationResult missingCoreResult = OperationResult::fail(
            tr("No compatible %1 core executable was found for the active server.")
                .arg(coreTypeDisplayName(launchCore)));
        downloadMissingCoreAndResume(launchCore, missingCoreResult, context.steps.downloadCore,
            context.skipTunCleanup, context.showStartupOverlay);
        return std::nullopt;
    }
    if (checklist_.status(context.steps.downloadCore) == CoreStartupCheckpointStatus::Pending) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Passed, context.steps.downloadCore,
            QCoreApplication::translate("ProxySession", "Required core executable is available."));
    }

    const QString coreConfigPath = deps_.profileResolver.resolveRuntimeConfigPath(*server);
    if (coreConfigPath.isEmpty()) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, context.steps.validateConfig,
            QCoreApplication::translate("ProxySession", "Runtime config path is empty."));
        failStartup(OperationResult::fail(QCoreApplication::translate("ProxySession", "Failed to resolve the runtime config output path.")));
        return std::nullopt;
    }

    return StartupProfile{*server, launchCore, coreInfo, coreConfigPath};
}

void ProxySession::beginActivation()
{
    const bool alreadyActivating = isActivationInProgress();
    if (!alreadyActivating) {
        deps_.activationCoordinator.cancelBackgroundTasksForStartup();
        // A fresh activation, so a fresh handle. The resume paths re-enter startInternal with the
        // phase still inside the startup range, which is exactly the case that must keep the id its
        // earlier lines were already written under.
        activationId_ = QStringLiteral("%1").arg(
            QRandomGenerator::global()->generate(), 8, 16, QLatin1Char('0'));
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
    checklist_.prepare(currentRequest_.config.tun().tunModeItem.enableTun, showOverlay);
}

void ProxySession::setCheckpointStatus(
    CoreStartupCheckpointStatus status,
    const QString& step,
    const QString& detail)
{
    checklist_.setStatus(status, step, detail);
}

void ProxySession::clearChecklist()
{
    checklist_.clear();
}

void ProxySession::clearChecklistAfterStableRun(int delayMs)
{
    checklist_.clearAfterStableRun(delayMs, [this]() {
        return phase_ == Phase::Proxying;
    });
}

void ProxySession::keepChecklistForUserDismissal(const QString& step, const QString& detail)
{
    checklist_.keepForUserDismissal(step, detail);
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

void ProxySession::undoManagedSystemProxyAfterAbortedStartup()
{
    // The proxy half of cleanupAfterFailedStartup(), with one deliberate difference: the flag
    // is dropped only when the undo write succeeded, like stopForCoreUpdate and unlike a failed
    // startup (which calls clearProxyStateAfterStopped() and forgets ownership wholesale --
    // nothing there is ever coming back to retry). Here the session stays alive, and a later
    // stop() must still be able to redo this undo, so a failed write must not erase the claim.
    // Auto wires no snapshot capture, so a restorable snapshot is always false here and the
    // decision lands on Clear -- the documented fallback for "ours, and the port is dead".
    const bool snapshotRestorable =
        isRestorableSystemProxySnapshot(deps_.activationCoordinator.capturedSystemProxySnapshot());
    switch (decideFailedStartupProxyAction(managedSystemProxyActive_, snapshotRestorable)) {
    case FailedStartupProxyAction::LeaveAlone:
        break;
    case FailedStartupProxyAction::Restore:
    case FailedStartupProxyAction::Clear:
        if (undoSystemProxyChange(SystemProxyUndoTrigger::StartupFailed)) {
            managedSystemProxyActive_ = false;
        }
        break;
    }
    // The registry may have changed, so the state sync the two sibling undo paths end with
    // (:1517, cleanupAfterFailedStartup) belongs here too. Auto does not connect this signal
    // today -- its UI is session-driven, not registry-driven -- but symmetry is the contract:
    // an undo path that changes the registry and stays silent is how #8 was found.
    emit statusSyncRequested();
}

void ProxySession::cleanupAfterFailedStartup()
{
    cancelPendingCoreRestarts();
    currentServer_ = {};

    // Ownership, not configuration. This used to clear the proxy whenever the config asked for it
    // OR the registry happened to be enabled -- and both are true for a user who was running
    // their own proxy before starting this app, so a startup that failed before ever touching the
    // registry still deleted their settings. The decision lives in a named function now because
    // it is a one-line rule that is easy to get wrong inline.
    const bool snapshotRestorable =
        isRestorableSystemProxySnapshot(deps_.activationCoordinator.capturedSystemProxySnapshot());
    switch (decideFailedStartupProxyAction(managedSystemProxyActive_, snapshotRestorable)) {
    case FailedStartupProxyAction::LeaveAlone:
        break;
    case FailedStartupProxyAction::Restore:
    case FailedStartupProxyAction::Clear:
        if (undoSystemProxyChange(SystemProxyUndoTrigger::StartupFailed)) {
            managedSystemProxyActive_ = false;
        }
        break;
    }
    clearProxyStateAfterStopped();

    if (deps_.mainCore.isRunning()) {
        checklist_.markKeepOnNextStop();
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
    const QString targetDirectory = coreGeoDirectory(coreInfo);
    if (targetDirectory.trimmed().isEmpty()) {
        failStartup(OperationResult::fail(QCoreApplication::translate("ProxySession", "Core data directory is empty.")));
        return;
    }

    const QList<CoreGeoFileRequirement> requirements = coreGeoFileRequirementsFor(coreInfo);
    if (requirements.isEmpty()) {
        // Nothing to fetch after all; go straight on rather than stalling on an empty download.
        startInternal(skipTunCleanup, true, showStartupOverlay);
        return;
    }

    const QString startMessage = tr("Missing geo database files detected. Downloading to %1.")
        .arg(QDir::toNativeSeparators(targetDirectory));
    setCheckpointStatus(CoreStartupCheckpointStatus::Started, checkGeoStep, startMessage);
    emit logMessage(geoValidationMessage);

    setPhase(Phase::ValidateRuntimeResources);

    const std::weak_ptr<char> guard = lifetimeGuard_;
    QThread* thread = launchBackgroundThread([this, targetDirectory, requirements, checkGeoStep,
                                       skipTunCleanup, showStartupOverlay, guard]() {
        const auto reportProgress = [this, checkGeoStep, guard](const QString& message) {
            postStartupDownloadProgress(checkGeoStep, message, guard);
        };

        GeoResourceUpdateService geoUpdateService(targetDirectory, {}, reportProgress);
        QList<OperationResult> results;
        for (const CoreGeoFileRequirement& requirement : requirements) {
            // A file already on disk is not fetched again, so a retry after a canceled or failed
            // download resumes instead of re-pulling everything the core needs.
            if (coreGeoFileIsPresent(targetDirectory, requirement.fileName)) {
                continue;
            }
            results.append(geoUpdateService.updateCoreGeoFile(requirement));
        }
        const OperationResult result = combineOperationResults(results);

        QMetaObject::invokeMethod(this, [this, result, checkGeoStep, skipTunCleanup,
                                         showStartupOverlay, guard]() {
            if (guard.expired() || shuttingDown_.load()) {
                return;
            }
            handleStartupDownloadFinished(
                checkGeoStep,
                result,
                skipTunCleanup,
                showStartupOverlay,
                tr("Proxy startup was canceled while Geo files were updating."),
                tr("Geo files were updated, but proxy startup was canceled."));
        }, Qt::QueuedConnection);
    }, [this](QThread* worker) { trackBackgroundThread(worker); });
    thread->start();
}

void ProxySession::downloadMissingSingBoxRuleSetsAndResume(
    const QStringList& missingRuleSetTags,
    const QString& checkGeoStep,
    bool skipTunCleanup,
    bool showStartupOverlay)
{
    const QStringList tags = normalizedUniqueTags(missingRuleSetTags);
    if (tags.isEmpty()) {
        startInternal(skipTunCleanup, true, showStartupOverlay);
        return;
    }

    const QString targetDirectory = AppPaths::applicationDirectory();
    const QString startMessage = tr("Missing sing-box rule sets detected. Downloading %1 file(s) to %2.")
        .arg(tags.size())
        .arg(QDir::toNativeSeparators(QDir(targetDirectory).filePath(SingBoxPaths::ruleSetDirectoryName())));
    setCheckpointStatus(CoreStartupCheckpointStatus::Started, checkGeoStep, startMessage);

    setPhase(Phase::ValidateRuntimeResources);

    const std::weak_ptr<char> guard = lifetimeGuard_;
    QThread* thread = launchBackgroundThread([this, targetDirectory, tags, checkGeoStep, skipTunCleanup,
                                       showStartupOverlay, guard]() {
        const auto reportProgress = [this, checkGeoStep, guard](const QString& message) {
            postStartupDownloadProgress(checkGeoStep, message, guard);
        };

        GeoResourceUpdateService geoUpdateService(targetDirectory, {}, reportProgress);
        QList<OperationResult> results;
        for (const QString& tag : tags) {
            results.append(geoUpdateService.updateSingBoxRuleSet(tag));
        }
        const OperationResult result = combineOperationResults(results);

        QMetaObject::invokeMethod(this, [this, result, checkGeoStep, skipTunCleanup,
                                         showStartupOverlay, guard]() {
            if (guard.expired() || shuttingDown_.load()) {
                return;
            }
            handleStartupDownloadFinished(
                checkGeoStep,
                result,
                skipTunCleanup,
                showStartupOverlay,
                tr("Proxy startup was canceled while sing-box rule sets were updating."),
                tr("sing-box rule sets were updated, but proxy startup was canceled."));
        }, Qt::QueuedConnection);
    }, [this](QThread* worker) { trackBackgroundThread(worker); });
    thread->start();
}

void ProxySession::downloadMissingCoreAndResume(
    CoreType coreType,
    const OperationResult& missingCoreResult,
    const QString& downloadCoreStep,
    bool skipTunCleanup,
    bool showStartupOverlay)
{
    const CoreType runtimeCore = deps_.profileResolver.resolveRuntimeCoreType(coreType);
    const QString installDirectory = deps_.profileResolver.resolveCoreInstallDirectory(runtimeCore);
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

    QThread* thread = launchBackgroundThread([this, runtimeCore, workerConfig, installDirectory,
                                       downloadCoreStep, skipTunCleanup,
                                       showStartupOverlay, token, guard]() {
        CoreUpdateService coreUpdateService;
        CoreUpdateService::UpdateOptions options;
        options.progressHandler = [this, downloadCoreStep, token, guard](const QString& message) {
            postStartupDownloadProgress(downloadCoreStep, message, token, guard);
        };
        options.cancelCheck = [this]() {
            QThread* currentThread = QThread::currentThread();
            return shuttingDown_.load()
                || (currentThread != nullptr && currentThread->isInterruptionRequested());
        };
        options.skipLocalVersionCheck = true;

        const OperationResult result = coreUpdateService.update(
            runtimeCore, workerConfig, installDirectory, options);

        QMetaObject::invokeMethod(this, [this, downloadCoreStep, skipTunCleanup,
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
            handleStartupDownloadFinished(
                downloadCoreStep,
                result,
                skipTunCleanup,
                showStartupOverlay,
                tr("Proxy startup was canceled while the core was downloading."),
                tr("The core was downloaded, but proxy startup was canceled."),
                [this]() { deps_.activationCoordinator.refreshExistingCoreTypes(); });
        }, Qt::QueuedConnection);
    }, [this](QThread* worker) { trackBackgroundThread(worker); });
    thread->start();
}

void ProxySession::postStartupDownloadProgress(
    const QString& step,
    const QString& message,
    const std::weak_ptr<char>& guard)
{
    if (message.trimmed().isEmpty()) {
        return;
    }

    QMetaObject::invokeMethod(this, [this, step, message, guard]() {
        if (guard.expired() || !isActivationInProgress()) {
            return;
        }
        setCheckpointStatus(CoreStartupCheckpointStatus::Started, step, message);
    }, Qt::QueuedConnection);
}

void ProxySession::postStartupDownloadProgress(
    const QString& step,
    const QString& message,
    const BackgroundTaskCoordinator::Token& token,
    const std::weak_ptr<char>& guard)
{
    if (message.trimmed().isEmpty()) {
        return;
    }

    QMetaObject::invokeMethod(this, [this, step, message, token, guard]() {
        if (guard.expired()) {
            return;
        }
        if (!deps_.backgroundTasks.isCurrent(token)) {
            return;
        }
        if (!isActivationInProgress()) {
            return;
        }
        setCheckpointStatus(CoreStartupCheckpointStatus::Started, step, message);
    }, Qt::QueuedConnection);
}

void ProxySession::handleStartupDownloadFinished(
    const QString& step,
    const OperationResult& result,
    bool skipTunCleanup,
    bool showStartupOverlay,
    const QString& canceledMessage,
    const QString& completedButCanceledMessage,
    const std::function<void()>& successAction)
{
    if (!isActivationInProgress()) {
        keepChecklistForUserDismissal(step, canceledMessage);
        cancelActivation();
        emit statusSyncRequested();
        return;
    }
    if (!result.success) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, step, result.message);
        failStartup(result);
        return;
    }
    if (normalizeSystemProxyMode(currentRequest_.config.sysProxyType) != SystemProxyMode::ForcedChange) {
        keepChecklistForUserDismissal(step, completedButCanceledMessage);
        cancelActivation();
        emit statusSyncRequested();
        return;
    }

    if (successAction) {
        successAction();
    }
    setCheckpointStatus(CoreStartupCheckpointStatus::Passed, step, result.message);
    startInternal(skipTunCleanup, true, showStartupOverlay);
}
// --- Core started/failed ---

void ProxySession::handleCoreStarted(const QString& serverIndexId)
{
    if (phase_ != Phase::StartCoreProcess || !deps_.mainCore.isRunning()) {
        return;
    }
    setServerWarning({});
    queryOutboundConnection(serverIndexId);
}

void ProxySession::validateCoreListeningAndHandleStarted(
    const QString& serverIndexId,
    bool customServer)
{
    const QString startCoreStep = tr("Start core process");
    const bool usesDedicatedProbe = !customServer;
    const int listenPort = OutboundLocationProbeService::resolveHttpPort(currentRequest_.config, usesDedicatedProbe);
    if (listenPort <= 0 || listenPort > 65535) {
        const QString message = QCoreApplication::translate("ProxySession", "Local proxy listen port is unavailable.");
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

    QThread* thread = launchBackgroundThread([this, guard, activeServerId,
                                       startCoreStep, startTime,
                                       listenPort, listenProbeTimeoutMs]() {
        const bool portReady = waitForLocalTcpPortReady(listenPort, listenProbeTimeoutMs);

        QMetaObject::invokeMethod(this, [this, guard, activeServerId,
                                         startCoreStep, startTime,
                                         listenPort, listenProbeTimeoutMs, portReady]() {
            if (guard.expired()) {
                return;
            }
            if (coreStartTime_ != startTime || deps_.profileResolver.currentIndexId() != activeServerId) {
                setCheckpointStatus(CoreStartupCheckpointStatus::Skipped, startCoreStep,
                    QCoreApplication::translate("ProxySession", "Core listening validation result is stale."));
                if (coreStartTime_ == startTime && deps_.mainCore.isRunning() && phase_ != Phase::Stopping) {
                    stop(false);
                }
                return;
            }

            if (!portReady) {
                const QString message = QCoreApplication::translate(
                    "ProxySession",
                    "Core did not open the local proxy port %1 within %2 seconds.")
                    .arg(listenPort)
                    .arg((listenProbeTimeoutMs + 999) / 1000);
                setCheckpointStatus(CoreStartupCheckpointStatus::Failed, startCoreStep, message);
                failStartup(OperationResult::fail(message));
                return;
            }

            setCheckpointStatus(CoreStartupCheckpointStatus::Passed, startCoreStep,
                QCoreApplication::translate("ProxySession", "Core local proxy is listening on 127.0.0.1:%1.").arg(listenPort));
            handleCoreStarted(activeServerId);
        }, Qt::QueuedConnection);
    }, [this](QThread* worker) { trackBackgroundThread(worker); });
    thread->start();
}

void ProxySession::handleMainCoreOutput(const QString& line)
{
    if (coreTunEnabledAtStart_ && isTunAdapterConflictOutput(line)) {
        coreTunAdapterConflictDetected_ = true;
    }
    emit coreOutput(line);
}

void ProxySession::handleCoreStartFailed(const QString& message)
{
    if (isTunAdapterConflictOutput(message)) {
        coreTunAdapterConflictDetected_ = true;
    }

    const bool tunEnabledAtFailedStart = coreTunEnabledAtStart_;
    setCheckpointStatus(CoreStartupCheckpointStatus::Failed, tr("Start core process"), message);
    if (tunEnabledAtFailedStart
        && checklist_.status(tr("Start TUN runtime")) == CoreStartupCheckpointStatus::Started) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, tr("Start TUN runtime"), message);
    }
    coreTunEnabledAtStart_ = false;
    failStartup(OperationResult::fail(message));
    stopAuxiliaryCore();
    clearServerLocation();
}

// --- Tier 1: outbound startup gate ---

void ProxySession::queryOutboundConnection(const QString& serverIndexId)
{
    setPhase(Phase::CheckOutboundConnection);
    const QString connectionStep = tr("Check outbound connection");
    if (!deps_.mainCore.isRunning() || serverIndexId.trimmed().isEmpty()) {
        const QString message = tr("Core is not ready for the outbound connection check.");
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, connectionStep, message);
        failStartup(OperationResult::fail(message));
        return;
    }

    // The explicit advanced escape hatch. Off by default: the gate is what keeps a node that
    // answers nothing from being reported as active, so skipping it is a trade the user opts into.
    // Everything after the gate is unchanged -- the system proxy is applied and the display-only
    // location probe still runs.
    if (currentRequest_.config.skipOutboundCheck) {
        const QString activeServerId = serverIndexId.trimmed();
        setCheckpointStatus(CoreStartupCheckpointStatus::Skipped, connectionStep,
            tr("Outbound connection check was skipped."));
        // Logged as loudly as a verdict would be. "The user turned the check off" is the first
        // thing a report has to be read against, and without this line the gate looks like it
        // simply never ran.
        emit logMessage(tr("Outbound check skipped") + QStringLiteral(" | ")
            + outboundGateFacts(activationId_, 0, 0, -1));
        // Waived, not confirmed: the UI state asks whether the session may be reported active, and
        // the user has just said the check is not wanted.
        outboundReachable_ = true;
        if (applySystemProxyAfterOutboundCheck()) {
            clearChecklistAfterStableRun(CoreStartupCompletionOverlayDelayMs);
            queryServerLocation(activeServerId);
        }
        return;
    }

    const bool usesDedicatedProbe = currentServer_.configType != ConfigType::Custom;
    const int httpPort = OutboundLocationProbeService::resolveHttpPort(currentRequest_.config, usesDedicatedProbe);
    if (httpPort <= 0 || httpPort > 65535) {
        const QString message = tr("Outbound connection check port is unavailable.");
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, connectionStep, message);
        failStartup(OperationResult::fail(message));
        return;
    }
    setCheckpointStatus(CoreStartupCheckpointStatus::Started, connectionStep,
        tr("Checking the outbound connection."));

    const std::weak_ptr<char> guard = lifetimeGuard_;
    const QString activeServerId = serverIndexId.trimmed();
    const qint64 startTime = coreStartTime_;
    QThread* thread = launchBackgroundThread([this, guard, activeServerId, httpPort, connectionStep, startTime]() {
        const OutboundLivenessResult probeResult = deps_.livenessProbe.probe(httpPort);
        const bool reachable = probeResult.reachable;
        const QString lastError = probeResult.error;
        const OutboundFailureShape failureShape = probeResult.shape;
        const int attemptedHosts = probeResult.attemptedHosts;
        const qint64 elapsedMs = probeResult.elapsedMs;
        // Only on the failure path, and unproxied on purpose: asking the same question through the
        // core would answer nothing new, and the only thing worth knowing once the gate says "no"
        // is whether this machine can get anywhere without it.
        const bool directReachable = reachable ? true : directConnectivityReachable();
        QMetaObject::invokeMethod(this, [this, guard, activeServerId, httpPort, reachable, lastError, failureShape,
                                         attemptedHosts, elapsedMs, directReachable, connectionStep, startTime]() {
            if (guard.expired() || shuttingDown_.load() || phase_ != Phase::CheckOutboundConnection) {
                return;
            }
            if (coreStartTime_ != startTime) {
                return; // Never overwrite the checklist belonging to a newer activation.
            }
            if (!deps_.mainCore.isRunning() || deps_.profileResolver.currentIndexId() != activeServerId) {
                setCheckpointStatus(CoreStartupCheckpointStatus::Skipped, connectionStep,
                    tr("Outbound connection check result is stale."));
                stop(false);
                return;
            }
            if (!reachable) {
                emit logMessage(tr("Outbound check failed") + QStringLiteral(" | ")
                    + outboundGateFacts(activationId_, httpPort, attemptedHosts, elapsedMs));
                // The one piece of evidence a DNS explanation cannot be argued without, printed
                // next to the failure it is meant to explain rather than left to be reconstructed
                // from the config file.
                emit logMessage(tr("DNS in effect") + QStringLiteral(" | ")
                    + describeEffectiveDns(currentRequest_.config.dns()));

                if (!directReachable) {
                    // Both halves failed, so the fault is not the node: this machine cannot reach
                    // the probe hosts even with the core out of the picture. Failing closed here
                    // would take away a proxy the user may need precisely *because* the local
                    // network is in trouble, so the check is waived and the session comes up --
                    // with a warning that outlives the checklist in the status bar.
                    emit logMessage(tr("Outbound check waived: direct connectivity failed as well")
                        + QStringLiteral(" | ")
                        + outboundGateFacts(activationId_, httpPort, attemptedHosts, elapsedMs));
                    const QString warning = tr(
                        "The outbound check could not be completed: this machine cannot reach the probe hosts directly either.");
                    outboundReachable_ = true;
                    setCheckpointStatus(CoreStartupCheckpointStatus::Passed, connectionStep, warning);
                    if (applySystemProxyAfterOutboundCheck()) {
                        // Set after the phase change, never before: setPhase(Proxying) clears the
                        // warning, because one left over from a previous core must not describe the
                        // core now running. Setting it first is how this line was lost.
                        setServerWarning(warning);
                        // Only this branch's warning describes the local network rather than the
                        // node, so only this branch arms the retraction. Nothing else in the session
                        // can retract it: the health watch speaks for the node, and its own clear is
                        // never reached now that the node is known to be fine.
                        waivedOutboundWarningShown_ = true;
                        recheckWaivedOutboundWarning();
                        clearChecklistAfterStableRun(CoreStartupCompletionOverlayDelayMs);
                        queryServerLocation(activeServerId);
                    }
                    return;
                }

                QString message = lastError.trimmed().isEmpty()
                    ? tr("Outbound connection unavailable.") : lastError;
                // Say what the failure *was*, not just that there was one. The same list of hosts
                // reads as three very different problems depending on its shape, and the user is
                // the one who has to act on it.
                switch (failureShape) {
                case OutboundFailureShape::CertificateMismatch:
                    message = tr("%1 The server certificate does not match the address.").arg(message);
                    break;
                case OutboundFailureShape::Unreachable:
                    message = tr("%1 The node could not be reached.").arg(message);
                    break;
                case OutboundFailureShape::TunnelBroken:
                    message = tr("%1 The tunnel was established but carries no traffic.").arg(message);
                    break;
                case OutboundFailureShape::Unknown:
                    break;
                }
                setCheckpointStatus(CoreStartupCheckpointStatus::Failed, connectionStep, message);
                failStartup(OperationResult::fail(message));
                return;
            }
            outboundReachable_ = true;
            setCheckpointStatus(CoreStartupCheckpointStatus::Passed, connectionStep,
                tr("Outbound connection is reachable."));
            emit logMessage(tr("Outbound check passed") + QStringLiteral(" | ")
                + outboundGateFacts(activationId_, httpPort, attemptedHosts, elapsedMs));
            if (applySystemProxyAfterOutboundCheck()) {
                clearChecklistAfterStableRun(CoreStartupCompletionOverlayDelayMs);
                queryServerLocation(activeServerId);
            }
        }, Qt::QueuedConnection);
    }, [this](QThread* worker) { trackBackgroundThread(worker); });
    thread->start();
}

// --- Tier 2: optional exit-location display ---

void ProxySession::queryServerLocation(const QString& serverIndexId)
{
    if (phase_ != Phase::Proxying || !deps_.mainCore.isRunning() || serverIndexId.trimmed().isEmpty()) {
        return;
    }
    const bool usesDedicatedProbe = currentServer_.configType != ConfigType::Custom;
    const int httpPort = OutboundLocationProbeService::resolveHttpPort(currentRequest_.config, usesDedicatedProbe);
    if (httpPort <= 0 || httpPort > 65535) {
        const QString message = tr("Location probe port is unavailable.");
        if (currentServerWarning_.isEmpty()) {
            setServerWarning(message);
        }
        emit logMessage(message);
        return;
    }
    emit logMessage(tr("Detecting outbound IP location") + QStringLiteral(" | ")
        + activationTag(activationId_));
    const std::weak_ptr<char> guard = lifetimeGuard_;
    const QString activeServerId = serverIndexId.trimmed();
    const qint64 startTime = coreStartTime_;
    QThread* thread = launchBackgroundThread([this, guard, activeServerId, httpPort, startTime]() {
        const OutboundLocationProbeResult probeResult = deps_.locationProbe.probe(httpPort);
        const QString location = probeResult.location;
        const QString lastError = probeResult.error;
        QMetaObject::invokeMethod(this, [this, guard, activeServerId, location, lastError, startTime]() {
            if (guard.expired() || shuttingDown_.load() || phase_ != Phase::Proxying
                || !deps_.mainCore.isRunning() || coreStartTime_ != startTime
                || deps_.profileResolver.currentIndexId() != activeServerId) {
                return; // A display result is never allowed to cancel an activation.
            }
            setServerLocation(location);
            // The location signal may synchronously stop or switch the session.
            if (phase_ != Phase::Proxying || coreStartTime_ != startTime
                || deps_.profileResolver.currentIndexId() != activeServerId) {
                return;
            }
            if (currentServerLocation_.isEmpty()) {
                const QString message = lastError.trimmed().isEmpty()
                    ? tr("Outbound location unavailable.") : lastError;
                // A location warning must not replace a stronger health-watch warning.
                if (currentServerWarning_.isEmpty()) {
                    setServerWarning(message);
                }
                emit logMessage(message + QStringLiteral(" | ") + activationTag(activationId_));
            }
        }, Qt::QueuedConnection);
    }, [this](QThread* worker) { trackBackgroundThread(worker); });
    thread->start();
}

bool ProxySession::applySystemProxyAfterOutboundCheck()
{
    const QString proxyStep = tr("Apply system proxy");
    const SystemProxyMode configuredMode = normalizeSystemProxyMode(currentRequest_.config.sysProxyType);

    if (configuredMode != SystemProxyMode::ForcedChange) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, proxyStep,
            QCoreApplication::translate("ProxySession", "Global system proxy mode is not enabled."));
        failStartup(OperationResult::fail(QCoreApplication::translate("ProxySession", "Global system proxy mode is not enabled.")));
        return false;
    }

    // Capture before the first write, and only once per session: from here on the registry holds
    // our port, so a second capture would record our own value as "what the user had". The
    // snapshot is also what makes a later failure able to put the user's proxy back instead of
    // deleting it.
    const SystemProxySnapshot captured =
        deps_.activationCoordinator.ensureSystemProxySnapshotCaptured();

    setPhase(Phase::ApplySystemProxy);
    setCheckpointStatus(CoreStartupCheckpointStatus::Started, proxyStep,
        QCoreApplication::translate("ProxySession", "Applying Global system proxy."));
    const bool proxyUpdated = updateSystemProxyMode(SystemProxyMode::ForcedChange);
    if (!proxyUpdated) {
        setCheckpointStatus(CoreStartupCheckpointStatus::Failed, proxyStep,
            QCoreApplication::translate("ProxySession", "Failed to apply the configured Global system proxy."));
        // A write that failed half-way is still our doing. Put back what the capture saw rather
        // than leaving the registry between two states.
        if (isRestorableSystemProxySnapshot(captured)) {
            deps_.activationCoordinator.restoreSystemProxySnapshot(captured);
        }
        failStartup(OperationResult::fail(
            QCoreApplication::translate("ProxySession", "Failed to apply the configured Global system proxy after starting the core.")));
        emit statusSyncRequested();
        return false;
    }
    setCheckpointStatus(CoreStartupCheckpointStatus::Passed, proxyStep,
        QCoreApplication::translate("ProxySession", "Global system proxy is active."));
    managedSystemProxyActive_ = true;

    finishActivation();
    emit statusSyncRequested();
    return true;
}

// --- Stop flow ---

void ProxySession::stopInternal(bool immediate, bool clearPostStopAction)
{
    const bool keepChecklist = checklist_.consumeKeepOnNextStop();
    const bool checklistVisible = checklist_.visible();
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
        && shouldCleanupTunAfterCoreStop(deps_.environment.isWindowsPlatform(), coreTunEnabledAtStart_);
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
        if (stopRequested && !checklist_.visible()) {
            clearChecklist();
        }
    }

    const bool cleanupTunAfterStop = !auxiliary && shouldCleanupTunAfterCoreStop(
        deps_.environment.isWindowsPlatform(),
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

    const QProcess::ExitStatus exitStatus = static_cast<QProcess::ExitStatus>(status);
    const qint64 runDuration = coreStartTime_ > 0
        ? (QDateTime::currentMSecsSinceEpoch() - coreStartTime_)
        : 0;
    const ProxyCrashRestartPolicy::Decision restartDecision = crashRestartPolicy_.decide(
        exitCode,
        exitStatus,
        auxiliary,
        deps_.environment.isWindowsPlatform(),
        tunEnabledAtCoreExit,
        tunAdapterConflictDetected,
        runDuration);

    if (restartDecision.action == ProxyCrashRestartPolicy::Action::DisableAfterTunConflict) {
        emit logMessage(restartDecision.message);
        clearProxyStateAfterStopped();
        emit statusSyncRequested();
        emit stopped();
        return;
    }

    if (restartDecision.action == ProxyCrashRestartPolicy::Action::DisableRestart) {
        emit logMessage(restartDecision.message);
        if (!auxiliary) {
            clearProxyStateAfterStopped();
            emit statusSyncRequested();
        }
        emit stopped();
        return;
    }

    scheduleCoreRestart(restartDecision.message, restartDecision.auxiliary, restartDecision.delayMs);
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
    outboundReachable_ = false;
    const bool systemProxyEnabled = deps_.activationCoordinator.isSystemProxyEnabled();
    const bool snapshotRestorable =
        isRestorableSystemProxySnapshot(deps_.activationCoordinator.capturedSystemProxySnapshot());

    // Same ownership rule as a failed startup. `systemProxyEnabled` is still consulted, but only
    // so that a proxy something *else* already turned off is not written back over -- it is an
    // AND with ownership, never a substitute for it.
    switch (decideStopProxyAction(
        managedSystemProxyActive_, systemProxyEnabled, snapshotRestorable)) {
    case FailedStartupProxyAction::LeaveAlone:
        return;
    case FailedStartupProxyAction::Restore:
    case FailedStartupProxyAction::Clear:
        if (undoSystemProxyChange(SystemProxyUndoTrigger::CoreStopped)) {
            managedSystemProxyActive_ = false;
        }
        return;
    }
}

void ProxySession::clearProxyStateAfterStopped()
{
    outboundReachable_ = false;
    managedSystemProxyActive_ = false;
    currentServer_ = {};
    currentServerLocation_.clear();
    currentServerWarning_.clear();
    waivedOutboundWarningShown_ = false;
    directRecheckInProgress_ = false;
    if (directRecheckTimer_ != nullptr) {
        directRecheckTimer_->stop();
    }
}
// --- TUN cleanup ---

void ProxySession::setTunCleanupState(TunCleanupState state)
{
    tunCleanupState_ = state;
}

void ProxySession::removeStaleTunAdapterAsync(const std::function<void(const OperationResult&)>& completion)
{
    const std::weak_ptr<char> guard = lifetimeGuard_;
    QThread* thread = launchBackgroundThread([this, completion, guard]() {
        const OperationResult result = deps_.environment.removeStaleTunAdapter();
        QMetaObject::invokeMethod(this, [completion, result, guard]() {
            if (guard.expired()) {
                return;
            }
            if (completion) {
                completion(result);
            }
        }, Qt::QueuedConnection);
    }, [this](QThread* worker) { trackBackgroundThread(worker); });
    thread->start();
}

void ProxySession::removeStaleTunAdapter()
{
    const OperationResult result = deps_.environment.removeStaleTunAdapter();
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
                        if (!checklist_.isEmpty()) {
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

// --- Node health watch ---
//
// Why this exists: the crash-restart policy above only reacts to the core *process* dying. A core
// that is running perfectly while its active node has gone dark produced nothing at all -- in the
// log that motivated this, an AnyTLS node was dead for 2 h 39 m and the only trace was 183 core
// ERROR lines. This watch is the missing signal: it probes the local HTTP inbound (which is what
// the node is actually reached through) and, after two consecutive failures, raises the same
// server warning the rest of the app already renders in the status bar.
//
// It deliberately does NOT switch nodes. Switching stops and restarts the core, which would drop
// every connection the user has open; deciding that on the user's behalf, silently, is worse than
// telling them. SongBirdAuto makes the opposite trade because it is unattended by design.

void ProxySession::scheduleHealthWatch()
{
    if (shuttingDown_.load() || phase_ != Phase::Proxying || !availabilityCheck_) {
        return;
    }
    if (healthWatchTimer_ == nullptr) {
        healthWatchTimer_ = new QTimer(this);
        healthWatchTimer_->setSingleShot(true);
        QObject::connect(healthWatchTimer_, &QTimer::timeout, this, [this]() { runHealthWatch(); });
    }
    healthWatchTimer_->start(healthWatchIntervalMs_);
}

// --- Waived outbound warning ------------------------------------------------
//
// The gate has one branch that comes up anyway: the node answered nothing *and* this machine cannot
// reach the probe hosts directly, which makes the node innocent and the local network the suspect.
// That branch raises a warning and lets the session run.
//
// Nothing used to retract it. The health watch only speaks for the node, and this sentence is about
// the machine, so a user who fixed their network (joined a different Wi-Fi, restored a VPN) kept
// reading "this machine cannot reach the probe hosts directly either" for the rest of the session
// and had to restart the proxy to be told the truth. This is the retraction: while that specific
// warning is up, the same unproxied probe runs again on the health watch's cadence, and the warning
// is dropped the moment it answers.

void ProxySession::recheckWaivedOutboundWarning()
{
    if (!waivedOutboundWarningShown_ || shuttingDown_.load() || phase_ != Phase::Proxying) {
        return;
    }
    if (directRecheckTimer_ == nullptr) {
        directRecheckTimer_ = new QTimer(this);
        directRecheckTimer_->setSingleShot(true);
        QObject::connect(directRecheckTimer_, &QTimer::timeout, this, [this]() { runDirectRecheck(); });
    }
    directRecheckTimer_->start(healthWatchIntervalMs_);
}

void ProxySession::runDirectRecheck()
{
    if (!waivedOutboundWarningShown_ || shuttingDown_.load() || phase_ != Phase::Proxying
        || directRecheckInProgress_) {
        return;
    }

    directRecheckInProgress_ = true;
    const int timeoutMs = OutboundLivenessProbeService::LivenessProbeDirectTimeoutMs;
    const std::weak_ptr<char> guard = lifetimeGuard_;
    QThread* thread = launchBackgroundThread(
        [this, guard, timeoutMs]() {
            // Unproxied, and therefore independent of this object: the worker reads nothing but the
            // timeout, so a session torn down mid-probe only loses the answer.
            const bool directReachable = directConnectivityReachable();
            QMetaObject::invokeMethod(this, [this, guard, directReachable]() {
                if (guard.expired()) {
                    return;
                }
                finishDirectRecheck(directReachable);
            }, Qt::QueuedConnection);
        },
        [this](QThread* worker) { trackBackgroundThread(worker); });
    thread->start();
}

void ProxySession::finishDirectRecheck(bool directReachable)
{
    directRecheckInProgress_ = false;
    if (shuttingDown_.load() || phase_ != Phase::Proxying || !waivedOutboundWarningShown_) {
        return;
    }

    if (!directReachable) {
        // Still no route without the core. The warning stays and the retraction keeps trying --
        // a local outage is exactly the case the user expects to be told about once and then left
        // alone until it ends.
        emit logMessage(tr("Direct connectivity is still unavailable; keeping the outbound warning.")
            + QStringLiteral(" | ") + activationTag(activationId_));
        recheckWaivedOutboundWarning();
        return;
    }

    // The sentence is no longer true, so it goes. Logged because a warning that disappears without
    // a trace reads as a glitch.
    const QString cleared = tr(
        "Direct connectivity is available again; the outbound warning no longer applies.");
    waivedOutboundWarningShown_ = false;
    setServerWarning(QString());
    emit logMessage(cleared + QStringLiteral(" | ") + activationTag(activationId_));
}

void ProxySession::runHealthWatch()
{
    if (shuttingDown_.load() || phase_ != Phase::Proxying || healthWatchInProgress_) {
        return;
    }

    // Copy the seam before leaving the main thread: the worker must not read a member of an object
    // it does not own, and the host may clear it at any time.
    const AvailabilityCheck check = availabilityCheck_;
    if (!check) {
        return;
    }

    const Config config = currentRequest_.config;

    healthWatchInProgress_ = true;
    const std::weak_ptr<char> guard = lifetimeGuard_;
    QThread* thread = launchBackgroundThread(
        [this, guard, check, config]() {
            const OperationResult result = check(config);
            QMetaObject::invokeMethod(this, [this, result, guard]() {
                if (guard.expired()) {
                    return;
                }
                finishHealthWatch(result);
            }, Qt::QueuedConnection);
        },
        [this](QThread* worker) { trackBackgroundThread(worker); });
    thread->start();
}

void ProxySession::finishHealthWatch(const OperationResult& result)
{
    healthWatchInProgress_ = false;
    if (shuttingDown_.load() || phase_ != Phase::Proxying) {
        return;
    }

    // The probe's own message is the reason appended to the warning. It is already localized by the
    // service that produced it, so the warning stays one language.
    const ProxyHealthWatchPolicy::Decision decision =
        healthWatchPolicy_.recordResult(result.success, result.message);

    switch (decision.action) {
    case ProxyHealthWatchPolicy::Action::Warn:
        setServerWarning(decision.message);
        emit logMessage(decision.message);
        break;
    case ProxyHealthWatchPolicy::Action::Clear:
        // Clearing means the node answered again, so the warning the user is looking at is no
        // longer true. The sentence still goes to the log, because a warning that disappears
        // without a trace reads as a glitch.
        setServerWarning(QString());
        emit logMessage(decision.message);
        break;
    case ProxyHealthWatchPolicy::Action::None:
        break;
    }

    scheduleHealthWatch();
}

void ProxySession::cancelHealthWatch()
{
    if (healthWatchTimer_ != nullptr) {
        healthWatchTimer_->stop();
    }
    healthWatchInProgress_ = false;
    healthWatchPolicy_.reset();
}

// --- Server location ---

void ProxySession::setServerLocation(QString location)
{
    location = location.trimmed();
    if (currentServerLocation_ == location) {
        return;
    }
    currentServerLocation_ = std::move(location);
    emit serverLocationChanged(currentServerLocation_);
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
    return deps_.activationCoordinator.updateSystemProxyMode(mode);
}

// Puts the user's own proxy settings back, or clears when there is nothing to put back.
//
// `trigger` selects which pair of sentences the panel shows, rather than being spliced into one.
// Whole sentences are what the translation file carries: a "%1" that receives an English phrase
// cannot be translated into Chinese word order, so a fragment would ship as half-translated text.
// Returns false when the registry write failed; the caller decides what to do about it.
bool ProxySession::undoSystemProxyChange(SystemProxyUndoTrigger trigger)
{
    const SystemProxySnapshot snapshot = deps_.activationCoordinator.capturedSystemProxySnapshot();
    if (isRestorableSystemProxySnapshot(snapshot)) {
        const bool restored = deps_.activationCoordinator.restoreSystemProxySnapshot(snapshot);
        if (restored) {
            emit logMessage(trigger == SystemProxyUndoTrigger::CoreStopped
                ? tr("System proxy restored because the core stopped.")
                : tr("System proxy restored because core startup failed."));
        } else {
            emit logMessage(trigger == SystemProxyUndoTrigger::CoreStopped
                ? tr("Failed to restore the system proxy after the core stopped.")
                : tr("Failed to restore the system proxy after core startup failed."));
        }
        return restored;
    }

    const bool cleared = updateSystemProxyMode(SystemProxyMode::ForcedClear);
    if (cleared) {
        emit logMessage(trigger == SystemProxyUndoTrigger::CoreStopped
            ? tr("System proxy disabled because the core stopped.")
            : tr("System proxy disabled because core startup failed."));
    } else {
        emit logMessage(trigger == SystemProxyUndoTrigger::CoreStopped
            ? tr("Failed to disable system proxy after the core stopped.")
            : tr("Failed to disable system proxy after core startup failed."));
    }
    return cleared;
}

void ProxySession::trackBackgroundThread(QThread* thread)
{
    backgroundThreads_.track(thread);
}
