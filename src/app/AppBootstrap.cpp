#include "app/AppBootstrap.h"
#include "app/AppBootstrapObjects.h"
#include "app/UiThreadInvocation.h"
#include "appcore/FunctionRuntimeAdapters.h"
#include "app/AppUpdateInstallService.h"
#include "app/AppUpdateCheckCoordinator.h"
#include "app/ApplicationRestartCoordinator.h"
#include "appcore/AppRuntimeResolver.h"
#include "app/RuntimeStateSnapshotBuilder.h"
#include "app/StartupWindowPolicy.h"
#include "appcore/BackgroundTaskCoordinator.h"
#include "appcore/BackgroundThreadTracker.h"
#include "app/ConfigBackupCoordinator.h"
#include "app/ConfigPathResolver.h"
#include "appcore/CoreProcessCleanupService.h"
#include "appcore/CoreDiscoveryService.h"
#include "app/CoreUpdateCoordinator.h"
#include "app/DefaultServerSwitchCoordinator.h"
#include "app/GeoResourceUpdateCoordinator.h"
#include "app/IUserFeedback.h"
#include "appcore/OutboundLocationProbeService.h"
#include "appcore/ProxyRuntimeInterfaces.h"
#include "appcore/ProxySession.h"
#include "domain/models/RuntimeState.h"
#include "app/ServerCollectionCoordinator.h"
#include "app/ServerEditorCoordinator.h"
#include "app/SubscriptionWorkflowCoordinator.h"
#include "common/AppPlatform.h"
#include "common/DialogUtils.h"
#include "common/GitHubUrls.h"
#include "common/StartupSequencer.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QDir>
#include <QGuiApplication>
#include <QMetaObject>
#include <QMessageBox>
#include <QPointer>
#include <QRegularExpression>
#include <QSessionManager>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include <utility>

#include <functional>
#include <memory>
#include <optional>

#include "appcore/StartupAdminElevation.h"
#include "app/SettingsApplyCoordinator.h"
#include "app/SettingsDialogRunner.h"
#include "app/SettingsWorkflowCoordinator.h"
#include "app/SpeedTestCoordinator.h"
#include "app/SystemProxyCoordinator.h"
#include "app/TunModeCoordinator.h"
#include "app/TunRuntimeState.h"
#include "appcore/TunRuntimeService.h"
#include "common/ServerDisplayName.h"
#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"
#include "domain/models/RoutingProfiles.h"
#include "platform/IAutoRunService.h"
#include "platform/ISystemProxyService.h"
#include "persistence/JsonConfigRepository.h"
#include "runtime/ClientConfigWriter.h"
#include "runtime/QtCoreProcessHost.h"
#include "runtime/TunCompatCoreRequirement.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/CoreCatalog.h"
#include "runtime/core/ICoreBackend.h"
#include "services/ConfigBackupService.h"
#include "services/AppUpdateService.h"
#include "services/CoreUpdateService.h"
#include "services/GeoResourceUpdateService.h"
#include "services/RoutingService.h"
#include "services/ServerService.h"
#include "services/SpeedTestController.h"
#include "services/SubscriptionService.h"
#include "subscription/ShareUrlBuilder.h"
#include "ui/dialogs/AboutDialog.h"
#include "ui/dialogs/SettingsDialog.h"
#include "ui/dialogs/UwpLoopbackDialog.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/theme/AppTheme.h"
#include "ui/tray/TrayController.h"

namespace {

constexpr auto AppReleaseDate = __DATE__;
constexpr int AppUpdateCheckIntervalMs = 24 * 60 * 60 * 1000;

bool isRoutineCoreLogLine(const QString& line)
{
    static const QRegularExpression importantLevelPattern(QStringLiteral(
        "(?:\\[(?:Warning|Error|Fatal|Panic)\\]|\\b(?:WARN|WARNING|ERROR|FATAL|PANIC)\\b)"));
    if (importantLevelPattern.match(line).hasMatch()) {
        return false;
    }

    static const QRegularExpression routineLevelPattern(QStringLiteral(
        "(?:\\[(?:Info|Debug)\\]|^\\s*(?:INFO|DEBUG)\\b|\\b(?:INFO|DEBUG)\\[)"));
    return routineLevelPattern.match(line).hasMatch();
}

QString proxySessionPhaseName(ProxySession::Phase phase)
{
    switch (phase) {
    case ProxySession::Phase::Stopped:
        return QStringLiteral("Stopped");
    case ProxySession::Phase::EnvironmentCleanup:
        return QStringLiteral("EnvironmentCleanup");
    case ProxySession::Phase::ValidateCoreApplication:
        return QStringLiteral("ValidateCoreApplication");
    case ProxySession::Phase::ValidateRuntimeResources:
        return QStringLiteral("ValidateRuntimeResources");
    case ProxySession::Phase::ValidateCoreConfig:
        return QStringLiteral("ValidateCoreConfig");
    case ProxySession::Phase::StartTunRuntime:
        return QStringLiteral("StartTunRuntime");
    case ProxySession::Phase::StartCoreProcess:
        return QStringLiteral("StartCoreProcess");
    case ProxySession::Phase::CheckOutboundLocation:
        return QStringLiteral("CheckOutboundLocation");
    case ProxySession::Phase::ApplySystemProxy:
        return QStringLiteral("ApplySystemProxy");
    case ProxySession::Phase::Proxying:
        return QStringLiteral("Proxying");
    case ProxySession::Phase::Stopping:
        return QStringLiteral("Stopping");
    }
    return QStringLiteral("Unknown");
}

bool interactivePromptsEnabled()
{
    return !qEnvironmentVariableIsSet("SONGBIRD_NONINTERACTIVE");
}

ProxySession::StartRequest makeProxySessionStartRequest(
    const Config& config,
    const QList<CoreType>& existingCoreTypes,
    bool skipTunCleanup,
    bool showOverlay,
    bool warnOnFailure)
{
    ProxySession::StartRequest request;
    request.config = config;
    request.existingCoreTypes = existingCoreTypes;
    request.skipTunCleanup = skipTunCleanup;
    request.showOverlay = showOverlay;
    request.warnOnFailure = warnOnFailure;
    return request;
}

} // namespace

AppBootstrap::AppBootstrap(
    QString configPath,
    bool startHidden,
    bool skipCoreChecks)
    : configPath_(std::move(configPath))
    , startHidden_(startHidden)
    , skipCoreChecks_(skipCoreChecks)
    , objects_(std::make_unique<AppBootstrapObjects>())
{
}

AppBootstrap::~AppBootstrap()
{
    shuttingDown_.store(true);
    lifetimeGuard_.reset();
    if (objects_->backgroundThreadTracker != nullptr) {
        objects_->backgroundThreadTracker->waitForAll();
    }

    if (objects_->proxySession != nullptr && (objects_->proxySession->isCoreRunning() || objects_->proxySession->isTransitioning())) {
        objects_->proxySession->stop(true);
    }
}

bool AppBootstrap::run()
{
    wireCoreServices();
    wireProxyStack();
    wireUiObjects();

    const auto trackBackgroundThread = [this](QThread* thread) {
        if (objects_->backgroundThreadTracker != nullptr) {
            objects_->backgroundThreadTracker->track(thread);
        }
    };

    wireUpdateCoordinators(trackBackgroundThread);
    wireWorkflowCoordinators(trackBackgroundThread);
    wireServerCoordinators();
    wireShutdownHooks();

    // The startup tail is declared as data instead of being inlined here: each
    // entry only calls a named method, and the single gate aborts the sequence
    // when the config cannot be reloaded, which is exactly what run() reports
    // as a startup failure.
    StartupSequencer sequencer;
    sequencer.addStep(QStringLiteral("cleanup-orphan-core-processes"), [this]() { cleanupOrphanCoreProcesses(); });
    sequencer.addStep(QStringLiteral("wire-main-window"), [this]() { wireMainWindow(); });
    sequencer.addStep(QStringLiteral("initialize-tray"), [this]() { initializeTray(); });
    sequencer.addStep(QStringLiteral("refresh-existing-core-types"), [this]() { refreshExistingCoreTypes(); });
    sequencer.addGate(QStringLiteral("reload-config"), [this]() { return reloadConfig(); });
    sequencer.addStep(QStringLiteral("backup-config"), [this]() { autoBackupCurrentConfig(); });
    sequencer.addStep(QStringLiteral("adopt-managed-system-proxy"), [this]() { adoptManagedSystemProxyOnStartup(); });
    sequencer.addStep(QStringLiteral("present-main-window"), [this]() { presentMainWindowOnStartup(); });
    sequencer.addStep(QStringLiteral("schedule-app-update-checks"), [this]() { scheduleAppUpdateChecks(); });

    return sequencer.runAll();
}

void AppBootstrap::initializeTray()
{
    objects_->mainWindow->setHideToTrayEnabled(objects_->trayController->initialize());
}

void AppBootstrap::adoptManagedSystemProxyOnStartup()
{
    if (objects_->systemProxyService == nullptr) {
        return;
    }

    const bool managedSystemProxyActive = shouldAdoptManagedSystemProxyOnStartup(
        normalizeSystemProxyMode(config_.sysProxyType),
        config_.ui().mainProxyEnabled,
        objects_->systemProxyService->isEnabled());
    objects_->proxySession->adoptManagedSystemProxy(managedSystemProxyActive);
}

void AppBootstrap::presentMainWindowOnStartup()
{
    objects_->mainWindow->show();

    const bool hideRequested = startHidden_ || !config_.ui().showMainOnStartup;
    const bool trayAvailable =
        objects_->trayController != nullptr && objects_->trayController->isAvailable();

    switch (decideStartupWindowAction(hideRequested, trayAvailable)) {
    case StartupWindowAction::ShowNormally:
        break;
    case StartupWindowAction::HideToTray:
        objects_->mainWindow->hide();
        break;
    case StartupWindowAction::ShowMinimizedWithoutTray:
        objects_->mainWindow->showMinimized();
        appendResult(OperationResult::ok(QCoreApplication::translate(
            "AppBootstrap", "Start hidden requested, but the system tray is unavailable. The window was minimized instead.")));
        break;
    }

    uiReady_ = true;
}

void AppBootstrap::scheduleAppUpdateChecks()
{
    QTimer::singleShot(0, objects_->mainWindow.get(), [this]() {
        applyStartupSystemProxyPreference();
    });
    QTimer::singleShot(3000, objects_->mainWindow.get(), [this]() {
        checkAppUpdates(false);
    });
    auto* appUpdateTimer = new QTimer(objects_->mainWindow.get());
    appUpdateTimer->setInterval(AppUpdateCheckIntervalMs);
    QObject::connect(appUpdateTimer, &QTimer::timeout, objects_->mainWindow.get(), [this]() {
        checkAppUpdates(false);
    });
    appUpdateTimer->start();
}

// wireMainWindow() and the signal-wiring methods it drives now live in
// AppBootstrapMainWindowWiring.cpp so this file stays a pure composition root.

void AppBootstrap::syncWindow()
{
    if (qApp != nullptr) {
        AppTheme::applyApplicationTheme(*qApp, config_.ui().themeName);
    }

    if (objects_->mainWindow) {
        MainWindowInit init;
        init.config = config_;
        init.existingCoreTypes = existingCoreTypes_;
        objects_->mainWindow->initialize(init);
    }

    if (objects_->trayController != nullptr) {
        objects_->trayController->setServers(
            config_.collection().servers,
            config_.collection().subscriptions,
            config_.currentIndexId);
        objects_->trayController->setRoutings(
            RoutingProfiles::routingItems(config_.collection()),
            config_.collection().routingModeId);
        objects_->trayController->setTunEnabled(config_.tun().tunModeItem.enableTun);
    }

    syncStatusIndicators();
}

bool AppBootstrap::isProxyActivationInProgress() const
{
    return objects_->proxySession != nullptr && objects_->proxySession->isActivationInProgress();
}

void AppBootstrap::syncStatusIndicators()
{
    systemProxyMode_ = normalizeSystemProxyMode(config_.sysProxyType);
    const bool systemProxyEnabled = objects_->systemProxyService != nullptr && objects_->systemProxyService->isEnabled();
    const bool autoRunEnabled = objects_->autoRunService != nullptr && objects_->autoRunService->isEnabled();
    const VmessItem* currentServer = findServerById(config_.currentIndexId);
    const QString currentServerName = currentServer == nullptr ? QString() : serverDisplayName(*currentServer);
    const QString currentServerLocation = objects_->proxySession != nullptr ? objects_->proxySession->serverLocation() : QString();
    const QString currentServerWarning = objects_->proxySession != nullptr ? objects_->proxySession->serverWarning() : QString();

    if (objects_->mainWindow != nullptr) {
        objects_->mainWindow->setExistingCoreTypes(existingCoreTypes_);
    }
    if (objects_->trayController != nullptr) {
        objects_->trayController->setAutoRunEnabled(autoRunEnabled);
        objects_->trayController->setTunEnabled(config_.tun().tunModeItem.enableTun);
    }

    if (objects_->runtimeState != nullptr && objects_->runtimeStateSnapshotBuilder != nullptr) {
        RuntimeStateSnapshotBuilder::Inputs inputs;
        inputs.config = &config_;
        inputs.currentServerName = currentServerName;
        inputs.currentServerLocation = currentServerLocation;
        inputs.currentServerWarning = currentServerWarning;
        inputs.proxyPhaseName = objects_->proxySession != nullptr ? proxySessionPhaseName(objects_->proxySession->phase()) : QStringLiteral("Stopped");
        inputs.systemProxyMode = systemProxyMode_;
        inputs.systemProxyApplied = systemProxyEnabled;
        inputs.autoRunEnabled = autoRunEnabled;
        inputs.proxyTransitioning = objects_->proxySession != nullptr && objects_->proxySession->isTransitioning();
        inputs.coreRunning = isCoreRunning();
        inputs.coreReady = isCoreReady();
        const RuntimeStateSnapshot snapshot = objects_->runtimeStateSnapshotBuilder->buildSnapshot(inputs);
        objects_->runtimeState->applySnapshot(snapshot);
    }

    if (objects_->backgroundTasks != nullptr) {
        objects_->backgroundTasks->syncState();
    }
}

bool AppBootstrap::reloadConfig()
{
    refreshExistingCoreTypes();
    const Config loadedConfig = objects_->repository->load();
    const QString loadError = objects_->repository->lastLoadError().trimmed();
    if (!loadError.isEmpty()) {
        appendResult(OperationResult::fail(loadError));
        DialogUtils::showCritical(
            objects_->mainWindow.get(),
            QCoreApplication::translate("AppBootstrap", "Failed to Load Configuration"),
            loadError);
        return false;
    }

    config_ = loadedConfig;
    syncWindow();
    if (!uiStateRestored_ && objects_->mainWindow != nullptr) {
        objects_->mainWindow->restoreUiState(config_);
        uiStateRestored_ = true;
        syncStatusIndicators();
    }
    appendResult(OperationResult::ok(
        QCoreApplication::translate("AppBootstrap", "Configuration reloaded from disk.")));
    appendStartupResourceCheckResults();
    if (uiReady_ && isCoreRunning()) {
        restartCoreIfRunning(
            QCoreApplication::translate("AppBootstrap", "Reloading core after reloading configuration."),
            true);
    }
    return true;
}

void AppBootstrap::applyStartupSystemProxyPreference()
{
    if (config_.ui().mainProxyEnabled) {
        if (skipCoreChecks_) {
            appendResult(OperationResult::ok(QCoreApplication::translate(
                "AppBootstrap", "Startup core checks skipped by command line.")));
        }
        enableSystemProxy();
        return;
    }

    disableSystemProxy();
}

void AppBootstrap::persistUiState()
{
    if (objects_->mainWindow == nullptr || objects_->serverService == nullptr) {
        return;
    }

    objects_->mainWindow->captureUiState(config_);
    const OperationResult saveResult = objects_->serverService->save(config_);
    if (!saveResult.success) {
        appendResult(OperationResult::fail(QStringLiteral("%1 %2").arg(
            QCoreApplication::translate("AppBootstrap", "Failed to save settings."),
            saveResult.message)));
    }
}

void AppBootstrap::appendStartupResourceCheckResults()
{
    QStringList lines;

    if (!config_.currentIndexId.trimmed().isEmpty()
        && findServerById(config_.currentIndexId) == nullptr
        && !config_.collection().servers.isEmpty()) {
        lines.append(QCoreApplication::translate("AppBootstrap",
            "Startup check: The configured default server does not exist. The first available server will be used."));
    }

    int missingCustomConfigCount = 0;
    for (const VmessItem& item : config_.collection().servers) {
        if (item.configType != ConfigType::Custom) {
            continue;
        }

        const QString customConfigPath = resolveCustomConfigPath(item.address);
        if (customConfigPath.trimmed().isEmpty() || !QFileInfo::exists(customConfigPath)) {
            ++missingCustomConfigCount;
        }
    }

    const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
    if (activeServer.has_value()) {
        if (activeServer->configType == ConfigType::Custom) {
            const QString customConfigPath = resolveCustomConfigPath(activeServer->address);
            if (customConfigPath.trimmed().isEmpty() || !QFileInfo::exists(customConfigPath)) {
                lines.append(QCoreApplication::translate("AppBootstrap",
                                 "Startup check: Custom config file is missing for default server: %1")
                                 .arg(QDir::toNativeSeparators(customConfigPath)));
            }
        }

        if (!skipCoreChecks_) {
            const CoreInfo coreInfo = resolveCoreInfo(*activeServer);
            if (coreInfo.program.trimmed().isEmpty()) {
                const CoreType runtimeCore = resolveLaunchCoreType(*activeServer);
                const QStringList candidates = resolveCoreCandidates(runtimeCore);
                lines.append(QCoreApplication::translate("AppBootstrap",
                                 "Startup check: No compatible core executable was found for default server \"%1\". Expected one of: %2.")
                                 .arg(elidedServerDisplayName(*activeServer, 64))
                                 .arg(objects_->coreDiscoveryService->expectedCoreFilesText(candidates)));
            }

            for (const CoreType auxiliaryCoreType : resolveAuxiliaryTunCompatCoreTypes(config_, *activeServer, existingCoreTypes_)) {
                const QStringList candidates = resolveCoreCandidates(auxiliaryCoreType);
                if (locateFirstExistingFile(candidates).isEmpty()) {
                    lines.append(QCoreApplication::translate("AppBootstrap",
                                     "Startup check: Default server \"%1\" also needs the %2 core for TUN compatibility. Expected one of: %3.")
                                     .arg(elidedServerDisplayName(*activeServer, 64))
                                     .arg(coreTypeDisplayName(auxiliaryCoreType))
                                     .arg(objects_->coreDiscoveryService->expectedCoreFilesText(candidates)));
                }
            }
        }
    }

    if (missingCustomConfigCount > 0) {
        lines.append(QCoreApplication::translate("AppBootstrap",
            "Startup check: %1 custom server config file(s) are missing.").arg(missingCustomConfigCount));
    }

    for (const QString& line : lines) {
        appendResult(OperationResult::ok(line));
    }
}

void AppBootstrap::appendResult(const OperationResult& result)
{
    if (result.message.trimmed().isEmpty()) {
        return;
    }

    QStringList lines;
    const QStringList rawLines = result.message.split(QChar('\n'));
    for (const QString& rawLine : rawLines) {
        const QString line = rawLine.trimmed();
        if (!line.isEmpty()) {
            lines.append(line);
        }
    }

    if (lines.isEmpty()) {
        return;
    }

    if (objects_->mainWindow != nullptr) {
        for (const QString& line : lines) {
            objects_->mainWindow->appendLog(line);
        }
    }
}

void AppBootstrap::showOperationMessage(
    const QString& title,
    const OperationResult& result,
    QWidget* parent,
    bool showDialog)
{
    appendResult(result);
    if (!showDialog || result.message.trimmed().isEmpty() || objects_->mainWindow == nullptr) {
        return;
    }

    QWidget* messageParent = parent != nullptr ? parent : objects_->mainWindow.get();
    if (result.success) {
        DialogUtils::showInformation(messageParent, title, result.message);
        return;
    }

    DialogUtils::showWarning(messageParent, title, result.message);
}

void AppBootstrap::startManagedProxyCoreInternal(
    bool skipTunCleanup,
    bool showStartupOverlay)
{
    if (objects_->proxySession == nullptr) {
        return;
    }

    objects_->proxySession->start(makeProxySessionStartRequest(
        config_,
        existingCoreTypes_,
        skipTunCleanup,
        showStartupOverlay,
        setCurrentActivationPending_));
}

void AppBootstrap::stopCore(bool immediate)
{
    setSystemProxyMode(SystemProxyMode::ForcedClear, false, immediate);
}

OperationResult AppBootstrap::removeStaleTunAdapterIfPresent() const
{
    if (objects_->tunRuntimeService == nullptr) {
        return OperationResult::fail(QCoreApplication::translate(
            "AppBootstrap", "TUN runtime service is unavailable."));
    }
    return objects_->tunRuntimeService->removeStaleAdapterIfPresent();
}

void AppBootstrap::cleanupOrphanCoreProcesses()
{
    if (objects_->coreProcessCleanupService == nullptr) {
        return;
    }

    const QStringList terminatedProcesses = objects_->coreProcessCleanupService->cleanupOrphanCoreProcesses();
    if (!terminatedProcesses.isEmpty()) {
        appendResult(OperationResult::ok(
            QCoreApplication::translate("AppBootstrap", "Cleaned up orphan core processes: %1").arg(terminatedProcesses.join(QStringLiteral(", ")))));
        appendResult(removeStaleTunAdapterIfPresent());
    }
}

void AppBootstrap::cleanupCoreProcessesUsingConfiguredPorts()
{
    if (objects_->coreProcessCleanupService == nullptr) {
        return;
    }

    const QStringList terminatedProcesses =
        objects_->coreProcessCleanupService->cleanupCoreProcessesUsingConfiguredPorts(
            config_,
            OutboundLocationProbeService::LocationProbePortOffset);
    if (!terminatedProcesses.isEmpty()) {
        appendResult(OperationResult::ok(
            QCoreApplication::translate("AppBootstrap", "Cleaned up core processes using configured ports: %1")
                .arg(terminatedProcesses.join(QStringLiteral(", ")))));
    }
}

void AppBootstrap::cancelBackgroundTasksForProxyStartup()
{
    const BackgroundTaskCoordinator::Kind activeKind = objects_->backgroundTasks == nullptr
        ? BackgroundTaskCoordinator::Kind::None
        : objects_->backgroundTasks->active();

    if (objects_->speedTestCoordinator != nullptr && objects_->backgroundTasks != nullptr
        && activeKind == BackgroundTaskCoordinator::Kind::SpeedTest) {
        objects_->speedTestCoordinator->cancelActiveSpeedTest();
    }

    if (activeKind == BackgroundTaskCoordinator::Kind::SubscriptionUpdate
        && objects_->mainWindow != nullptr) {
        objects_->mainWindow->setSubscriptionUpdateRunning(false);
    }

    if (objects_->backgroundThreadTracker != nullptr) {
        objects_->backgroundThreadTracker->requestInterruptionAll();
    }

    if (objects_->backgroundTasks != nullptr) {
        objects_->backgroundTasks->cancelActive();
    }
}

void AppBootstrap::restartCoreIfRunning(const QString& reason, bool showStartupOverlay)
{
    if (objects_->proxySession == nullptr || !isCoreRunning()) {
        setCurrentActivationPending_ = false;
        return;
    }

    if (!reason.isEmpty()) {
        appendResult(OperationResult::ok(reason));
    }
    objects_->proxySession->restartIfRunning(
        makeProxySessionStartRequest(
            config_,
            existingCoreTypes_,
            false,
            showStartupOverlay,
            setCurrentActivationPending_));
}

bool AppBootstrap::saveSystemProxyMode(SystemProxyMode mode)
{
    if (objects_->systemProxyCoordinator == nullptr) {
        appendResult(OperationResult::fail(QCoreApplication::translate(
            "AppBootstrap", "System proxy mode cannot be changed before the configuration service is ready.")));
        return false;
    }

    return objects_->systemProxyCoordinator->saveMode(mode);
}

void AppBootstrap::setSystemProxyMode(
    SystemProxyMode mode,
    bool skipTunCleanup,
    bool immediateStop,
    bool showStartupOverlay)
{
    if (objects_->systemProxyCoordinator != nullptr) {
        objects_->systemProxyCoordinator->setMode(mode, skipTunCleanup, immediateStop, showStartupOverlay);
    }
}

void AppBootstrap::clearProxyStateAfterCoreStopped()
{
    if (objects_->systemProxyCoordinator != nullptr) {
        objects_->systemProxyCoordinator->clearStateAfterCoreStopped();
    }
}

void AppBootstrap::cleanupRuntimeForExit(bool windowsShutdown)
{
    if (objects_->systemProxyCoordinator != nullptr) {
        objects_->systemProxyCoordinator->cleanupRuntimeForExit(windowsShutdown);
    }
}

void AppBootstrap::enableSystemProxy(bool showStartupOverlay)
{
    if (objects_->systemProxyCoordinator != nullptr) {
        objects_->systemProxyCoordinator->enable(showStartupOverlay);
    }
}

void AppBootstrap::retryCoreStartup(bool showStartupOverlay)
{
    if (objects_->systemProxyCoordinator != nullptr) {
        objects_->systemProxyCoordinator->retryStartup(showStartupOverlay);
    }
}

void AppBootstrap::disableSystemProxy()
{
    if (objects_->systemProxyCoordinator != nullptr) {
        objects_->systemProxyCoordinator->disable();
    }
}

void AppBootstrap::setAutoRunEnabled(bool enabled)
{
    if (config_.ui().autoRunEnabled == enabled) {
        syncStatusIndicators();
        return;
    }

    Config updatedConfig = config_;
    updatedConfig.ui().autoRunEnabled = enabled;
    applySettingsDialogResult(updatedConfig);
}

void AppBootstrap::setTunEnabled(bool enabled)
{
    if (objects_->tunModeCoordinator == nullptr) {
        appendResult(OperationResult::fail(QCoreApplication::translate(
            "AppBootstrap", "TUN mode cannot be changed before the configuration service is ready.")));
        syncStatusIndicators();
        return;
    }

    objects_->tunModeCoordinator->setEnabled(enabled);
}

void AppBootstrap::importFromClipboard()
{
    const QString text = QApplication::clipboard() == nullptr
        ? QString()
        : QApplication::clipboard()->text().trimmed();
    if (text.isEmpty()) {
        appendResult(OperationResult::fail(QCoreApplication::translate("AppBootstrap", "Clipboard text is empty.")));
        return;
    }

    if (objects_->subscriptionWorkflowCoordinator != nullptr) {
        objects_->subscriptionWorkflowCoordinator->importClipboard(text);
    }
}

void AppBootstrap::updateCurrentSubscription(const QString& subscriptionId)
{
    const QString trimmedSubscriptionId = subscriptionId.trimmed();
    if (trimmedSubscriptionId.isEmpty()) {
        appendResult(OperationResult::fail(
            QCoreApplication::translate(
                "AppBootstrap",
                "Select a specific subscription tab before updating the current subscription.")));
        return;
    }

    QList<int> rowIndexes;
    for (int index = 0; index < config_.collection().subscriptions.size(); ++index) {
        if (config_.collection().subscriptions.at(index).id.trimmed() == trimmedSubscriptionId) {
            rowIndexes.append(index);
        }
    }

    if (rowIndexes.isEmpty()) {
        appendResult(OperationResult::fail(
            QCoreApplication::translate(
                "AppBootstrap",
                "The selected subscription could not be found.")));
        return;
    }

    if (objects_->subscriptionWorkflowCoordinator != nullptr) {
        objects_->subscriptionWorkflowCoordinator->updateSelected(rowIndexes);
    }
}

void AppBootstrap::updateCurrentSubscriptionViaProxy(const QString& subscriptionId)
{
    const QString trimmedSubscriptionId = subscriptionId.trimmed();
    if (trimmedSubscriptionId.isEmpty()) {
        appendResult(OperationResult::fail(
            QCoreApplication::translate(
                "AppBootstrap",
                "Select a specific subscription tab before updating the current subscription.")));
        return;
    }

    if (objects_->subscriptionWorkflowCoordinator != nullptr) {
        objects_->subscriptionWorkflowCoordinator->updateByIds(
            QStringList{trimmedSubscriptionId},
            true,
            QCoreApplication::translate("AppBootstrap", "Updating the current subscription through the local proxy..."));
    }
}

void AppBootstrap::handleRoutingSelectionResult(
    const OperationResult& result,
    const QString& previousRoutingModeId)
{
    appendResult(result);
    syncWindow();

    if (result.success
        && isCoreRunning()
        && previousRoutingModeId != config_.collection().routingModeId) {
        restartCoreIfRunning(QCoreApplication::translate(
            "AppBootstrap", "Reloading core to apply routing changes."), true);
    }
}

void AppBootstrap::autoBackupCurrentConfig()
{
    if (objects_->configBackupCoordinator != nullptr) {
        objects_->configBackupCoordinator->autoBackupCurrentConfig();
    }
}

void AppBootstrap::restoreConfigFromBackup()
{
    if (objects_->configBackupCoordinator != nullptr) {
        objects_->configBackupCoordinator->restoreConfigFromBackup();
    }
}

void AppBootstrap::checkAppUpdates(bool manual)
{
    if (objects_->appUpdateCheckCoordinator != nullptr) {
        objects_->appUpdateCheckCoordinator->checkAppUpdates(manual);
    }
}

void AppBootstrap::openSettingsDialog(int initialTab)
{
    if (objects_->settingsWorkflowCoordinator != nullptr) {
        objects_->settingsWorkflowCoordinator->openSettingsDialog(initialTab);
    }
}

void AppBootstrap::applySettingsDialogResult(const Config& updatedConfig)
{
    if (objects_->settingsApplyCoordinator != nullptr) {
        objects_->settingsApplyCoordinator->apply(updatedConfig);
    }
}

void AppBootstrap::openAboutDialog()
{
    if (objects_->mainWindow == nullptr) {
        return;
    }

    AboutDialog dialog(objects_->mainWindow.get());
    dialog.setRepoUrl(songbirdProjectPageUrl());
    dialog.setVersion(QCoreApplication::applicationVersion());
    dialog.setReleaseDate(QString::fromLatin1(AppReleaseDate));
    dialog.setConfigPath(QDir::toNativeSeparators(resolveConfigPath()));
    dialog.setProxyActive(isCoreReady());
    dialog.exec();
}

void AppBootstrap::openUwpLoopbackDialog()
{
    if (objects_->mainWindow == nullptr) {
        return;
    }

    if (uwpLoopbackDialog_.isNull()) {
        uwpLoopbackDialog_ = new UwpLoopbackDialog(objects_->mainWindow.get());
        uwpLoopbackDialog_->setAttribute(Qt::WA_DeleteOnClose, false);
    }

    if (uwpLoopbackDialog_->isVisible()) {
        uwpLoopbackDialog_->raise();
        uwpLoopbackDialog_->activateWindow();
        return;
    }

    uwpLoopbackDialog_->exec();
}

void AppBootstrap::openExternalUrl(const QString& url)
{
    if (url.trimmed().isEmpty()) {
        return;
    }

    if (!QDesktopServices::openUrl(QUrl(url))) {
        appendResult(OperationResult::fail(
            QCoreApplication::translate("AppBootstrap", "Failed to open the requested link.")));
    }
}

bool AppBootstrap::askRestartAsAdministratorForTun() const
{
    return objects_->applicationRestartCoordinator != nullptr
        && objects_->applicationRestartCoordinator->askRestartAsAdministratorForTun();
}

bool AppBootstrap::promptRestartAsAdministratorForTun()
{
    return objects_->applicationRestartCoordinator != nullptr
        && objects_->applicationRestartCoordinator->promptRestartAsAdministratorForTun();
}

void AppBootstrap::promptRestartForLanguageChange()
{
    if (objects_->applicationRestartCoordinator != nullptr) {
        objects_->applicationRestartCoordinator->promptRestartForLanguageChange();
    }
}

bool AppBootstrap::promptRestartForDownloadedAppUpdate(const QString& newExecutablePath, QWidget* parent)
{
    return objects_->applicationRestartCoordinator != nullptr
        && objects_->applicationRestartCoordinator->promptRestartForDownloadedAppUpdate(newExecutablePath, parent);
}

bool AppBootstrap::restartApplication(bool requireAdministrator)
{
    return objects_->applicationRestartCoordinator != nullptr
        && objects_->applicationRestartCoordinator->restartApplication(requireAdministrator);
}

bool AppBootstrap::isCoreRunning() const
{
    return objects_->proxySession != nullptr && objects_->proxySession->isCoreRunning();
}

bool AppBootstrap::isCoreReady() const
{
    return objects_->proxySession != nullptr && objects_->proxySession->isActive();
}

QString AppBootstrap::resolveConfigPath() const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->resolveConfigPath()
        : configPath_;
}

const VmessItem* AppBootstrap::resolveActiveServer() const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->resolveActiveServerPointer()
        : nullptr;
}

std::optional<VmessItem> AppBootstrap::resolveActiveServerSnapshot() const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->resolveActiveServerSnapshot()
        : std::nullopt;
}

const VmessItem* AppBootstrap::findServerById(const QString& indexId) const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->findServerById(indexId)
        : nullptr;
}

std::optional<VmessItem> AppBootstrap::findServerSnapshotById(const QString& indexId) const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->findServerSnapshotById(indexId)
        : std::nullopt;
}

QString AppBootstrap::resolveCustomConfigDirectory() const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->resolveCustomConfigDirectory()
        : QFileInfo(resolveConfigPath()).dir().filePath(QStringLiteral("guiConfigs"));
}

QString AppBootstrap::resolveCustomConfigPath(const QString& address) const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->resolveCustomConfigPath(address)
        : QString{};
}

QString AppBootstrap::resolveRuntimeConfigPath(const VmessItem& server) const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->resolveRuntimeConfigPath(server)
        : QString{};
}

bool AppBootstrap::updateSystemProxyMode(SystemProxyMode mode) const
{
    return objects_->systemProxyCoordinator == nullptr || objects_->systemProxyCoordinator->updateMode(mode);
}

CoreType AppBootstrap::resolveEffectiveCoreType(const VmessItem& server) const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->resolveEffectiveCoreType(server)
        : CoreType::Unknown;
}

CoreType AppBootstrap::resolveLaunchCoreType(const VmessItem& server) const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->resolveLaunchCoreType(server)
        : CoreType::Unknown;
}

CoreInfo AppBootstrap::resolveCoreInfo(const VmessItem& server) const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->resolveCoreInfo(server)
        : CoreInfo{};
}

QStringList AppBootstrap::resolveCoreCandidates(CoreType coreType) const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->resolveCoreCandidates(coreType)
        : QStringList{};
}

QString AppBootstrap::locateFirstExistingFile(const QStringList& candidates) const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->locateFirstExistingFile(candidates)
        : QString{};
}

void AppBootstrap::refreshExistingCoreTypes()
{
    existingCoreTypes_ = detectExistingCoreTypes();
}

QList<CoreType> AppBootstrap::detectExistingCoreTypes() const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->detectExistingCoreTypes()
        : QList<CoreType>{};
}

QString AppBootstrap::detectCoreVersion(CoreType coreType) const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->detectCoreVersion(coreType)
        : QString{};
}

QString AppBootstrap::resolveCoreInstallDirectory(CoreType coreType) const
{
    return objects_->appRuntimeResolver != nullptr
        ? objects_->appRuntimeResolver->resolveCoreInstallDirectory(coreType)
        : QCoreApplication::applicationDirPath();
}
