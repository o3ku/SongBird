#include "app/AppBootstrap.h"
#include "app/AppBootstrapObjects.h"
#include "app/FunctionRuntimeAdapters.h"
#include "app/AppUpdateInstallService.h"
#include "app/AppUpdateCheckCoordinator.h"
#include "app/ApplicationRestartCoordinator.h"
#include "app/AppRuntimeResolver.h"
#include "app/RuntimeStateSnapshotBuilder.h"
#include "app/BackgroundTaskCoordinator.h"
#include "app/BackgroundThreadTracker.h"
#include "app/ConfigBackupCoordinator.h"
#include "app/ConfigPathResolver.h"
#include "app/CoreProcessCleanupService.h"
#include "app/CoreDiscoveryService.h"
#include "app/CoreUpdateCoordinator.h"
#include "app/DefaultServerSwitchCoordinator.h"
#include "app/GeoResourceUpdateCoordinator.h"
#include "app/IUserFeedback.h"
#include "app/OutboundLocationProbeService.h"
#include "app/ProxyRuntimeInterfaces.h"
#include "app/ProxySession.h"
#include "app/RuntimeState.h"
#include "app/ServerCollectionCoordinator.h"
#include "app/ServerEditorCoordinator.h"
#include "app/SubscriptionWorkflowCoordinator.h"
#include "common/AppPlatform.h"
#include "common/DialogUtils.h"
#include "common/GitHubUrls.h"

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

#include "app/StartupAdminElevation.h"
#include "app/SettingsApplyCoordinator.h"
#include "app/SettingsDialogRunner.h"
#include "app/SettingsWorkflowCoordinator.h"
#include "app/SpeedTestCoordinator.h"
#include "app/SystemProxyCoordinator.h"
#include "app/TunModeCoordinator.h"
#include "app/TunRuntimeState.h"
#include "app/TunRuntimeService.h"
#include "common/ServerDisplayName.h"
#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"
#include "domain/models/RoutingProfiles.h"
#include "platform/windows/WindowsAutoRunService.h"
#include "platform/windows/WindowsSystemProxyService.h"
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

template <typename Callback>
void invokeOnUiThread(QObject* context, Callback&& callback)
{
    if (context == nullptr) {
        return;
    }

    if (QThread::currentThread() == context->thread()) {
        callback();
        return;
    }

    QMetaObject::invokeMethod(context, std::forward<Callback>(callback), Qt::QueuedConnection);
}

template <typename Callback>
auto invokeOnUiThreadBlocking(QObject* context, Callback&& callback) -> decltype(callback())
{
    using Result = decltype(callback());
    if (context == nullptr) {
        return Result();
    }

    if (QThread::currentThread() == context->thread()) {
        return callback();
    }

    Result result{};
    QMetaObject::invokeMethod(
        context,
        [&]() {
            result = callback();
        },
        Qt::BlockingQueuedConnection);
    return result;
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
    objects_->repository = std::make_unique<JsonConfigRepository>(resolveConfigPath());
    objects_->serverService = std::make_unique<ServerService>(*objects_->repository, resolveCustomConfigDirectory());
    objects_->configBackupService = std::make_unique<ConfigBackupService>(resolveConfigPath());
    objects_->configBackupCoordinator = std::make_unique<ConfigBackupCoordinator>(
        *objects_->configBackupService,
        ConfigBackupCoordinator::Dependencies{
            [this]() -> QWidget* { return objects_->mainWindow.get(); },
            [this]() { return resolveConfigPath(); },
            [this]() { return config_; },
            [this](const OperationResult& result) { appendResult(result); },
            [this]() { return isCoreRunning(); },
            [this]() { objects_->proxySession->stop(true); },
            [this]() { uiStateRestored_ = false; },
            [this]() { return reloadConfig(); },
            [this]() { return resolveActiveServer() != nullptr; },
            [this]() { enableSystemProxy(true); },
            [this]() { clearProxyStateAfterCoreStopped(); },
            [this]() { syncStatusIndicators(); }
        });
    objects_->routingService = std::make_unique<RoutingService>(*objects_->repository);
    objects_->speedTestController = std::make_unique<SpeedTestController>(resolveCustomConfigDirectory());
    objects_->subscriptionService = std::make_unique<SubscriptionService>(*objects_->repository);
    objects_->geoResourceUpdateService = std::make_unique<GeoResourceUpdateService>(
        QFileInfo(resolveConfigPath()).dir().absolutePath());
    objects_->clientConfigWriter = std::make_unique<ClientConfigWriter>(resolveCustomConfigDirectory());
    objects_->coreProcessHost = std::make_unique<QtCoreProcessHost>();
    objects_->backgroundTasks = std::make_unique<BackgroundTaskCoordinator>();
    objects_->backgroundThreadTracker = std::make_unique<BackgroundThreadTracker>();
    objects_->coreProcessCleanupService = std::make_unique<CoreProcessCleanupService>();
    objects_->coreDiscoveryService = std::make_unique<CoreDiscoveryService>();
    objects_->appRuntimeResolver = std::make_unique<AppRuntimeResolver>(
        resolveConfigPath(),
        config_,
        existingCoreTypes_,
        objects_->coreDiscoveryService.get());
    RuntimeStateSnapshotBuilder::Callbacks runtimeStatusCallbacks;
    runtimeStatusCallbacks.appendLog = [this](const QString& message) {
        if (objects_->mainWindow != nullptr) {
            objects_->mainWindow->appendLog(message);
        }
    };
    objects_->runtimeStateSnapshotBuilder = std::make_unique<RuntimeStateSnapshotBuilder>(std::move(runtimeStatusCallbacks));
    objects_->outboundLocationProbeService = std::make_unique<OutboundLocationProbeService>();
    objects_->appUpdateInstallService = std::make_unique<AppUpdateInstallService>(
        [](const QString& program, const QStringList& arguments) {
            return restartProcessAsAdministrator(program, arguments);
        });
    ApplicationRestartCoordinator::Callbacks restartCallbacks;
    restartCallbacks.dialogParent = [this]() -> QWidget* { return objects_->mainWindow.get(); };
    restartCallbacks.isWindowsPlatform = []() { return isWindowsPlatform(); };
    restartCallbacks.isProcessElevated = []() { return isProcessElevated(); };
    restartCallbacks.tunEnabled = [this]() { return config_.tun().tunModeItem.enableTun; };
    restartCallbacks.appendResult = [this](const OperationResult& result) { appendResult(result); };
    restartCallbacks.persistUiState = [this]() { persistUiState(); };
    restartCallbacks.cleanupRuntimeForExit = [this](bool windowsShutdown) {
        cleanupRuntimeForExit(windowsShutdown);
    };
    restartCallbacks.setMainWindowAllowClose = [this](bool allowClose) {
        if (objects_->mainWindow != nullptr) {
            objects_->mainWindow->setAllowClose(allowClose);
        }
    };
    restartCallbacks.setShutdownUiStatePersisted = [this](bool persisted) {
        shutdownUiStatePersisted_ = persisted;
    };
    objects_->applicationRestartCoordinator = std::make_unique<ApplicationRestartCoordinator>(
        ApplicationRestartCoordinator::Dependencies{objects_->appUpdateInstallService.get()},
        std::move(restartCallbacks));
    objects_->tunRuntimeService = std::make_unique<TunRuntimeService>();
    objects_->auxiliaryCoreProcessHost = std::make_unique<QtCoreProcessHost>();
    objects_->autoRunService = std::make_unique<WindowsAutoRunService>();
    objects_->systemProxyService = std::make_unique<WindowsSystemProxyService>();
    auto runtimeEnvironment = std::make_unique<FunctionRuntimeEnvironment>();
    runtimeEnvironment->cleanupPortProcessesFn = [this]() { cleanupCoreProcessesUsingConfiguredPorts(); };
    runtimeEnvironment->removeStaleTunAdapterFn = [this]() { return removeStaleTunAdapterIfPresent(); };
    runtimeEnvironment->skipCoreChecksFn = [this]() { return skipCoreChecks_; };
    runtimeEnvironment->isWindowsPlatformFn = []() { return isWindowsPlatform(); };
    runtimeEnvironment->isProcessElevatedFn = []() { return isProcessElevated(); };
    objects_->runtimeEnvironment = std::move(runtimeEnvironment);

    auto proxyActivationCoordinator = std::make_unique<FunctionProxyActivationCoordinator>();
    proxyActivationCoordinator->cancelBackgroundTasksForStartupFn = [this]() { cancelBackgroundTasksForProxyStartup(); };
    proxyActivationCoordinator->refreshExistingCoreTypesFn = [this]() { refreshExistingCoreTypes(); };
    proxyActivationCoordinator->isSystemProxyEnabledFn = [this]() {
        return objects_->systemProxyService != nullptr && objects_->systemProxyService->isEnabled();
    };
    proxyActivationCoordinator->updateSystemProxyModeFn = [this](SystemProxyMode mode) {
        return updateSystemProxyMode(mode);
    };
    objects_->proxyActivationCoordinator = std::move(proxyActivationCoordinator);

    objects_->proxySession = std::make_unique<ProxySession>(ProxySession::Dependencies{
        *objects_->coreProcessHost,
        *objects_->auxiliaryCoreProcessHost,
        *objects_->clientConfigWriter,
        *objects_->outboundLocationProbeService,
        *objects_->backgroundTasks,
        *objects_->appRuntimeResolver,
        *objects_->runtimeEnvironment,
        *objects_->proxyActivationCoordinator
    });
    objects_->runtimeState = std::make_unique<RuntimeState>();
    SystemProxyCoordinator::Callbacks systemProxyCallbacks;
    systemProxyCallbacks.appendResult = [this](const OperationResult& result) { appendResult(result); };
    systemProxyCallbacks.syncStatusIndicators = [this]() { syncStatusIndicators(); };
    systemProxyCallbacks.startManagedProxyCore = [this](bool skipTunCleanup, bool showStartupOverlay) {
        startManagedProxyCoreInternal(skipTunCleanup, showStartupOverlay);
    };
    objects_->systemProxyCoordinator = std::make_unique<SystemProxyCoordinator>(
        SystemProxyCoordinator::Dependencies{
            config_,
            *objects_->serverService,
            objects_->systemProxyService.get(),
            objects_->proxySession.get()},
        std::move(systemProxyCallbacks));
    TunModeCoordinator::Callbacks tunModeCallbacks;
    tunModeCallbacks.isWindowsPlatform = []() { return isWindowsPlatform(); };
    tunModeCallbacks.isProcessElevated = []() { return isProcessElevated(); };
    tunModeCallbacks.isCoreRunning = [this]() { return isCoreRunning(); };
    tunModeCallbacks.resolveActiveServer = [this]() { return resolveActiveServerSnapshot(); };
    tunModeCallbacks.askRestartAsAdministratorForTun = [this]() { return askRestartAsAdministratorForTun(); };
    tunModeCallbacks.restartApplication = [this](bool requireAdministrator) {
        return restartApplication(requireAdministrator);
    };
    tunModeCallbacks.persistUiState = [this]() { persistUiState(); };
    tunModeCallbacks.appendResult = [this](const OperationResult& result) { appendResult(result); };
    tunModeCallbacks.syncWindow = [this]() { syncWindow(); };
    tunModeCallbacks.syncStatusIndicators = [this]() { syncStatusIndicators(); };
    tunModeCallbacks.restartCoreIfRunning = [this](const QString& reason, bool showOverlay) {
        restartCoreIfRunning(reason, showOverlay);
    };
    tunModeCallbacks.enableSystemProxy = [this](bool showOverlay) { enableSystemProxy(showOverlay); };
    objects_->tunModeCoordinator = std::make_unique<TunModeCoordinator>(
        TunModeCoordinator::Dependencies{
            config_,
            *objects_->serverService},
        std::move(tunModeCallbacks));
    objects_->backgroundTasks->setBlockingPredicate([this]() {
        return isProxyActivationInProgress();
    });
    QObject::connect(objects_->backgroundTasks.get(), &BackgroundTaskCoordinator::blockedByCoreStartup,
        objects_->backgroundTasks.get(), [this]() {
            appendResult(OperationResult::fail(
                QCoreApplication::translate("AppBootstrap", "Proxy startup is in progress.")));
        });
    objects_->mainWindow = std::make_unique<MainWindow>();
    objects_->mainWindow->setShareUrlResolver([this](const QString& indexId) {
        const VmessItem* server = findServerById(indexId);
        return server == nullptr ? QString() : ShareUrlBuilder::build(*server).trimmed();
    });
    objects_->trayController = std::make_unique<TrayController>(objects_->mainWindow.get());
    objects_->speedTestCoordinator = std::make_unique<SpeedTestCoordinator>(
        SpeedTestCoordinator::Dependencies{
            objects_->backgroundTasks.get(),
            objects_->speedTestController.get(),
            [this]() -> Config& { return config_; },
            [this](Config& config) {
                return objects_->serverService != nullptr && objects_->serverService->save(config);
            },
            [this](const QString& indexId) { return findServerById(indexId); },
            [this](const VmessItem& server) { return resolveLaunchCoreType(server); },
            [this](const VmessItem& server) { return resolveCoreInfo(server); },
            [this](const QString& indexId, const QString& result) {
                return objects_->serverService == nullptr
                    ? OperationResult::fail(QStringLiteral("Speed test result service is unavailable."))
                    : objects_->serverService->setTestResult(config_, indexId, result);
            },
            [this](const OperationResult& result) { appendResult(result); },
            [this](const QString& message) {
                if (objects_->mainWindow != nullptr) {
                    objects_->mainWindow->appendLog(message);
                }
            },
            [this](const QString& indexId, const QString& result) {
                if (objects_->mainWindow != nullptr) {
                    objects_->mainWindow->updateServerTestResult(indexId, result);
                }
            },
            [this](const QStringList& indexIds, const QString& result) {
                if (objects_->mainWindow != nullptr) {
                    objects_->mainWindow->updateServerTestResults(indexIds, result);
                }
            },
            [this]() {
                if (objects_->trayController != nullptr) {
                    objects_->trayController->setServers(
                        config_.collection().servers,
                        config_.collection().subscriptions,
                        config_.currentIndexId);
                }
            }
        },
        objects_->mainWindow.get());
    auto userFeedback = std::make_unique<FunctionUserFeedback>();
    userFeedback->uiContextFn = [this]() -> QObject* { return objects_->mainWindow.get(); };
    userFeedback->dialogParentFn = [this]() -> QWidget* { return objects_->mainWindow.get(); };
    userFeedback->appendLogFn = [this](const QString& message) {
        if (objects_->mainWindow != nullptr) {
            objects_->mainWindow->appendLog(message);
        }
    };
    userFeedback->recordOperationResultFn = [this](const OperationResult& result) {
        appendResult(result);
    };
    userFeedback->showOperationMessageFn = [this](const QString& title, const OperationResult& result, QWidget* parent) {
        showOperationMessage(title, result, parent);
    };
    userFeedback->askYesNoFn = [](QWidget* parent, const QString& title, const QString& text, IUserFeedback::YesNoDefault defaultButton) {
        return DialogUtils::askYesNoQuestion(
            parent,
            title,
            text,
            defaultButton == IUserFeedback::YesNoDefault::Yes ? QMessageBox::Yes : QMessageBox::No)
            == QMessageBox::Yes;
    };
    userFeedback->showInformationFn = [](QWidget* parent, const QString& title, const QString& text) {
        DialogUtils::showInformation(parent, title, text);
    };
    userFeedback->showTrayMessageFn = [this](const QString& title, const QString& message, bool critical, int timeoutMs) {
        if (objects_->trayController != nullptr && objects_->trayController->isAvailable()) {
            objects_->trayController->showMessage(title, message, critical, timeoutMs);
        }
    };
    userFeedback->openExternalUrlFn = [this](const QString& url) {
        openExternalUrl(url);
    };
    userFeedback->promptRestartForDownloadedAppUpdateFn = [this](const QString& path, QWidget* parent) {
        return promptRestartForDownloadedAppUpdate(path, parent);
    };
    objects_->userFeedback = std::move(userFeedback);

    const auto trackBackgroundThread = [this](QThread* thread) {
        if (objects_->backgroundThreadTracker != nullptr) {
            objects_->backgroundThreadTracker->track(thread);
        }
    };

    objects_->appUpdateCheckCoordinator = std::make_unique<AppUpdateCheckCoordinator>(
        AppUpdateCheckCoordinator::Dependencies{
            objects_->backgroundTasks.get(),
            objects_->userFeedback.get(),
            []() { return QCoreApplication::applicationVersion(); },
            [this]() { return config_.checkPreReleaseUpdate; },
            []() { return QCoreApplication::applicationDirPath(); },
            [this]() { return shuttingDown_.load(); },
            trackBackgroundThread,
            {},
            {},
            {},
            [](QObject* context, std::function<void()> callback) {
                invokeOnUiThread(context, std::move(callback));
            }
        },
        objects_->mainWindow.get());

    objects_->coreUpdateCoordinator = std::make_unique<CoreUpdateCoordinator>(
        CoreUpdateCoordinator::Dependencies{
            objects_->backgroundTasks.get(),
            lifetimeGuard_,
            [this]() { return shuttingDown_.load(); },
            [this]() -> QObject* { return objects_->mainWindow.get(); },
            [this]() -> QWidget* { return objects_->mainWindow.get(); },
            [this]() -> QObject* { return objects_->mainWindow.get(); },
            [](CoreType coreType) { return resolveRuntimeCoreType(coreType); },
            [this](CoreType coreType) { return resolveCoreInstallDirectory(coreType); },
            [this]() {
                return CoreUpdateConfig{config_.checkPreReleaseUpdate, config_.ignoreGeoUpdateCore};
            },
            [this](CoreType coreType) {
                const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
                return isCoreRunning()
                    && activeServer.has_value()
                    && resolveRuntimeCoreType(activeServer->coreType) == coreType;
            },
            [](QWidget* parent, const QString& title, const QString& prompt) {
                return DialogUtils::askYesNoQuestion(parent, title, prompt, QMessageBox::Yes) == QMessageBox::Yes;
            },
            [this](const OperationResult& result) { appendResult(result); },
            [this](const QString& message) {
                if (objects_->mainWindow != nullptr) {
                    objects_->mainWindow->appendLog(message);
                }
            },
            [this](const QString& title, const OperationResult& result, QWidget* parent) {
                showOperationMessage(title, result, parent);
            },
            [this]() {
                if (objects_->proxySession != nullptr) {
                    objects_->proxySession->stopForCoreUpdate();
                }
            },
            [this](bool showOverlay) { enableSystemProxy(showOverlay); },
            [this]() { clearProxyStateAfterCoreStopped(); },
            [this]() { syncStatusIndicators(); },
            [this]() { refreshExistingCoreTypes(); },
            trackBackgroundThread,
            {},
            {}
        },
        objects_->mainWindow.get());

    objects_->geoResourceUpdateCoordinator = std::make_unique<GeoResourceUpdateCoordinator>(
        GeoResourceUpdateCoordinator::Dependencies{
            objects_->backgroundTasks.get(),
            [this]() { return QFileInfo(resolveConfigPath()).dir().absolutePath(); },
            [this]() -> QObject* { return objects_->mainWindow.get(); },
            [this]() -> QWidget* { return objects_->mainWindow.get(); },
            [this]() { return std::weak_ptr<char>(lifetimeGuard_); },
            [this]() { return objects_->geoResourceUpdateService != nullptr && objects_->mainWindow != nullptr; },
            trackBackgroundThread,
            [this](const QString& message) {
                if (objects_->mainWindow != nullptr) {
                    objects_->mainWindow->appendLog(message);
                }
            },
            [this](const OperationResult& result) { appendResult(result); },
            [](QWidget* parent, const QString& title, const QString& message) {
                DialogUtils::showWarning(parent, title, message);
            },
            [](QWidget* parent, const QString& title, const QString& message) {
                DialogUtils::showInformation(parent, title, message);
            },
            [this](const QString& reason) { restartCoreIfRunning(reason); }
        },
        objects_->mainWindow.get());

    wireWorkflowCoordinators(trackBackgroundThread);

    DefaultServerSwitchCoordinator::Callbacks defaultServerCallbacks;
    defaultServerCallbacks.currentIndexId = [this]() { return config_.currentIndexId; };
    defaultServerCallbacks.isCoreRunning = [this]() { return isCoreRunning(); };
    defaultServerCallbacks.isTunEnabled = [this]() { return config_.tun().tunModeItem.enableTun; };
    defaultServerCallbacks.isShuttingDown = [this]() { return shuttingDown_.load(); };
    defaultServerCallbacks.uiContext = [this]() -> QObject* { return objects_->mainWindow.get(); };
    defaultServerCallbacks.lifetimeGuard = [this]() { return std::weak_ptr<char>(lifetimeGuard_); };
    defaultServerCallbacks.setDefaultServer = [this](const QString& indexId) {
        return objects_->serverService != nullptr
            ? objects_->serverService->setDefaultServer(config_, indexId)
            : OperationResult::fail(QStringLiteral("Server service is unavailable."));
    };
    defaultServerCallbacks.appendResult = [this](const OperationResult& result) { appendResult(result); };
    defaultServerCallbacks.syncWindow = [this]() { syncWindow(); };
    defaultServerCallbacks.clearServerWarning = [this]() {
        if (objects_->proxySession != nullptr) {
            objects_->proxySession->setServerWarning({});
        }
    };
    defaultServerCallbacks.setCurrentActivationPending = [this](bool pending) {
        setCurrentActivationPending_ = pending;
    };
    defaultServerCallbacks.switchRunningCoreToServer = [this](const QString& indexId, bool enableTun) {
        if (objects_->proxySession != nullptr) {
            objects_->proxySession->switchServer(indexId, enableTun, true);
        }
    };
    defaultServerCallbacks.restartCoreIfRunning = [this](const QString& reason, bool showOverlay) {
        restartCoreIfRunning(reason, showOverlay);
    };
    defaultServerCallbacks.enableSystemProxy = [this](bool showOverlay) { enableSystemProxy(showOverlay); };
    defaultServerCallbacks.setTunEnabled = [this](bool enabled) { setTunEnabled(enabled); };
    defaultServerCallbacks.startProxyAfterSwitch = [this](bool showOverlay) {
        startManagedProxyCoreInternal(false, showOverlay);
    };
    objects_->defaultServerSwitchCoordinator = std::make_unique<DefaultServerSwitchCoordinator>(
        std::move(defaultServerCallbacks),
        objects_->mainWindow.get());

    ServerCollectionCoordinator::Callbacks serverCollectionCallbacks;
    serverCollectionCallbacks.resolveActiveServer = [this]() { return resolveActiveServerSnapshot(); };
    serverCollectionCallbacks.isCoreRunning = [this]() { return isCoreRunning(); };
    serverCollectionCallbacks.appendResult = [this](const OperationResult& result) { appendResult(result); };
    serverCollectionCallbacks.syncWindow = [this]() { syncWindow(); };
    serverCollectionCallbacks.stopCore = [this](bool immediate) { stopCore(immediate); };
    serverCollectionCallbacks.restartCoreIfRunning = [this](const QString& reason, bool showOverlay) {
        restartCoreIfRunning(reason, showOverlay);
    };
    objects_->serverCollectionCoordinator = std::make_unique<ServerCollectionCoordinator>(
        ServerCollectionCoordinator::Dependencies{
            config_,
            *objects_->serverService,
            *objects_->subscriptionService},
        std::move(serverCollectionCallbacks));

    ServerEditorCoordinator::Callbacks serverEditorCallbacks;
    serverEditorCallbacks.dialogParent = [this]() -> QWidget* { return objects_->mainWindow.get(); };
    serverEditorCallbacks.findServer = [this](const QString& indexId) {
        return findServerSnapshotById(indexId);
    };
    serverEditorCallbacks.resolveActiveServer = [this]() { return resolveActiveServerSnapshot(); };
    serverEditorCallbacks.isCoreRunning = [this]() { return isCoreRunning(); };
    serverEditorCallbacks.appendLog = [this](const QString& message) {
        if (objects_->mainWindow != nullptr) {
            objects_->mainWindow->appendLog(message);
        }
    };
    serverEditorCallbacks.appendResult = [this](const OperationResult& result) { appendResult(result); };
    serverEditorCallbacks.syncWindow = [this]() { syncWindow(); };
    serverEditorCallbacks.restartCoreIfRunning = [this](const QString& reason, bool showOverlay) {
        restartCoreIfRunning(reason, showOverlay);
    };
    objects_->serverEditorCoordinator = std::make_unique<ServerEditorCoordinator>(
        ServerEditorCoordinator::Dependencies{
            config_,
            *objects_->serverService},
        std::move(serverEditorCallbacks));

    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, objects_->mainWindow.get(), [this]() {
        shuttingDown_.store(true);
        cleanupRuntimeForExit(windowsShutdownRequested_);
        if (!shutdownUiStatePersisted_) {
            persistUiState();
            shutdownUiStatePersisted_ = true;
        }
    });
    if (QGuiApplication* guiApplication = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        QObject::connect(guiApplication, &QGuiApplication::commitDataRequest, objects_->mainWindow.get(), [this](QSessionManager&) {
            windowsShutdownRequested_ = true;
            shuttingDown_.store(true);
            cleanupRuntimeForExit(true);
            if (!shutdownUiStatePersisted_) {
                persistUiState();
                shutdownUiStatePersisted_ = true;
            }
        });
    }

    cleanupOrphanCoreProcesses();
    wireMainWindow();
    objects_->mainWindow->setHideToTrayEnabled(objects_->trayController->initialize());
    refreshExistingCoreTypes();
    if (!reloadConfig()) {
        return false;
    }
    autoBackupCurrentConfig();
    if (objects_->systemProxyService != nullptr) {
        const bool managedSystemProxyActive = shouldAdoptManagedSystemProxyOnStartup(
            normalizeSystemProxyMode(config_.sysProxyType),
            config_.ui().mainProxyEnabled,
            objects_->systemProxyService->isEnabled());
        objects_->proxySession->adoptManagedSystemProxy(managedSystemProxyActive);
    }
    objects_->mainWindow->show();
    if (startHidden_ || !config_.ui().showMainOnStartup) {
        if (objects_->trayController != nullptr && objects_->trayController->isAvailable()) {
            objects_->mainWindow->hide();
        } else {
            objects_->mainWindow->showMinimized();
            appendResult(OperationResult::ok(QStringLiteral(
                "Start hidden requested, but the system tray is unavailable. The window was minimized instead.")));
        }
    }
    uiReady_ = true;
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

    return true;
}

void AppBootstrap::wireMainWindow()
{
    if (objects_->appUpdateCheckCoordinator != nullptr) {
        QObject::connect(
            objects_->appUpdateCheckCoordinator.get(),
            &AppUpdateCheckCoordinator::updateAvailable,
            objects_->mainWindow.get(),
            [this](const AppUpdateCheckResult& result, const QString&) {
                if (objects_->mainWindow != nullptr) {
                    objects_->mainWindow->setAvailableAppUpdateVersion(result.latestVersion);
                }
            });
        QObject::connect(
            objects_->appUpdateCheckCoordinator.get(),
            &AppUpdateCheckCoordinator::updateUnavailable,
            objects_->mainWindow.get(),
            [this]() {
                if (objects_->mainWindow != nullptr) {
                    objects_->mainWindow->clearAvailableAppUpdateVersion();
                }
            });
    }
    wireProxySessionSignals();
    wireRuntimeStateSignals();
    wireBackgroundTaskSignals();
    wireMainWindowCommands();
    wireTraySignals();
}

void AppBootstrap::wireProxySessionSignals()
{
    QObject::connect(objects_->proxySession.get(), &ProxySession::phaseChanged,
                     objects_->mainWindow.get(), [this](ProxySession::Phase phase) {
                         Q_UNUSED(phase);
                         syncStatusIndicators();
                     });
    QObject::connect(objects_->proxySession.get(), &ProxySession::checklistUpdated,
                     objects_->mainWindow.get(), &MainWindow::setCoreStartupChecklist);
    QObject::connect(objects_->proxySession.get(), &ProxySession::checklistCleared,
                     objects_->mainWindow.get(), &MainWindow::clearCoreStartupChecklist);
    QObject::connect(objects_->proxySession.get(), &ProxySession::statusSyncRequested,
                     objects_->mainWindow.get(), [this]() {
                         syncStatusIndicators();
                     });
    QObject::connect(objects_->proxySession.get(), &ProxySession::coreOutput,
                     objects_->mainWindow.get(), [this](const QString& line) { appendResult(OperationResult::ok(line)); });
    QObject::connect(objects_->proxySession.get(), &ProxySession::auxiliaryCoreOutput,
                     objects_->mainWindow.get(), [this](const QString& line) {
                         appendResult(OperationResult::ok(QStringLiteral("tun-compat | %1").arg(line)));
                     });
    QObject::connect(objects_->proxySession.get(), &ProxySession::failed,
                     objects_->mainWindow.get(), [this](const QString& reason) {
                         appendResult(OperationResult::fail(reason));
                         saveSystemProxyMode(SystemProxyMode::ForcedClear);
                         if (setCurrentActivationPending_) {
                             appendResult(OperationResult::fail(
                                 QCoreApplication::translate(
                                     "AppBootstrap",
                                     "Node set successfully. It may be inaccessible. Please verify manually.")));
                             setCurrentActivationPending_ = false;
                         }
                         syncStatusIndicators();
                     });
    QObject::connect(objects_->proxySession.get(), &ProxySession::activated,
                     objects_->mainWindow.get(), [this]() {
                         setCurrentActivationPending_ = false;
                     });
    QObject::connect(objects_->proxySession.get(), &ProxySession::stopped,
                     objects_->mainWindow.get(), [this]() {
                         syncStatusIndicators();
                     });
    QObject::connect(objects_->proxySession.get(), &ProxySession::logMessage,
                     objects_->mainWindow.get(), [this](const QString& msg) { appendResult(OperationResult::ok(msg)); });
    QObject::connect(objects_->proxySession.get(), &ProxySession::coreUpdateResumeRequested,
                     objects_->mainWindow.get(), [this]() {
                         if (objects_->coreUpdateCoordinator != nullptr) {
                             objects_->coreUpdateCoordinator->continuePendingCoreUpdate();
                         }
                     });
    QObject::connect(objects_->proxySession.get(), &ProxySession::serverSwitchResumeRequested,
                     objects_->mainWindow.get(), [this](const QString& indexId, bool enableTun, bool showOverlay) {
                         if (objects_->defaultServerSwitchCoordinator != nullptr) {
                             objects_->defaultServerSwitchCoordinator->scheduleSwitchAfterCoreStopped(
                                 indexId,
                                 enableTun,
                                 showOverlay);
                         }
                     });
}

void AppBootstrap::wireRuntimeStateSignals()
{
    QObject::connect(objects_->runtimeState.get(), &RuntimeState::snapshotApplied,
                     objects_->mainWindow.get(), &MainWindow::applyRuntimeState);
    QObject::connect(objects_->runtimeState.get(), &RuntimeState::currentServerChanged,
                     objects_->trayController.get(), [this](const QString& name, const QString&, const QString&) {
                         if (objects_->trayController != nullptr) {
                             objects_->trayController->setCurrentServerName(name);
                         }
                     });
    QObject::connect(objects_->runtimeState.get(), &RuntimeState::proxyUiStateChanged,
                     objects_->trayController.get(), [this](ProxyUiState state) {
                         if (objects_->trayController == nullptr) {
                             return;
                         }
                         objects_->trayController->setProxyUiState(state);
                     });
    QObject::connect(objects_->runtimeState.get(), &RuntimeState::systemProxyStateChanged,
                     objects_->trayController.get(), [this](int mode, bool enabled) {
                         if (objects_->trayController != nullptr) {
                             objects_->trayController->setSystemProxyState(mode, enabled);
                         }
                     });
    QObject::connect(objects_->runtimeState.get(), &RuntimeState::autoRunChanged,
                     objects_->trayController.get(), [this](bool enabled) {
                         if (objects_->trayController != nullptr) {
                             objects_->trayController->setAutoRunEnabled(enabled);
                         }
                     });
    QObject::connect(objects_->runtimeState.get(), &RuntimeState::routingStatusChanged,
                     objects_->trayController.get(), [this](const QString& routingText, const QString&) {
                         if (objects_->trayController != nullptr) {
                             objects_->trayController->setRoutingSummary(routingText);
                         }
                     });
}

void AppBootstrap::wireBackgroundTaskSignals()
{
    QObject::connect(objects_->backgroundTasks.get(), &BackgroundTaskCoordinator::runningChanged,
                     objects_->mainWindow.get(), &MainWindow::setBackgroundTaskRunning);
    QObject::connect(objects_->backgroundTasks.get(), &BackgroundTaskCoordinator::descriptionChanged,
                     objects_->mainWindow.get(), &MainWindow::setBackgroundTaskDescription);
    QObject::connect(objects_->backgroundTasks.get(), &BackgroundTaskCoordinator::runningChanged,
                     objects_->trayController.get(), &TrayController::setBackgroundTaskRunning);
    QObject::connect(objects_->backgroundTasks.get(), &BackgroundTaskCoordinator::descriptionChanged,
                     objects_->trayController.get(), &TrayController::setBackgroundTaskDescription);
}

void AppBootstrap::wireMainWindowCommands()
{
    QObject::connect(objects_->mainWindow.get(), &MainWindow::addServerRequested, objects_->mainWindow.get(), [this]() {
        if (objects_->serverEditorCoordinator != nullptr) {
            objects_->serverEditorCoordinator->addServer();
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::editServerRequested, objects_->mainWindow.get(), [this](const QString& indexId) {
        if (objects_->serverEditorCoordinator != nullptr) {
            objects_->serverEditorCoordinator->editServer(indexId);
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::importFromClipboardRequested, objects_->mainWindow.get(), [this]() {
        importFromClipboard();
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::updateSubscriptionsRequested, objects_->mainWindow.get(), [this]() {
        if (objects_->subscriptionWorkflowCoordinator != nullptr) {
            objects_->subscriptionWorkflowCoordinator->updateAll();
        }
    });
    QObject::connect(
        objects_->mainWindow.get(),
        &MainWindow::updateCurrentSubscriptionRequested,
        objects_->mainWindow.get(),
        [this](const QString& subscriptionId) {
            updateCurrentSubscription(subscriptionId);
        });
    QObject::connect(
        objects_->mainWindow.get(),
        &MainWindow::updateCurrentSubscriptionViaProxyRequested,
        objects_->mainWindow.get(),
        [this](const QString& subscriptionId) {
            updateCurrentSubscriptionViaProxy(subscriptionId);
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::hideSubscriptionRequested, objects_->mainWindow.get(), [this](const QString& subscriptionId) {
        if (objects_->serverCollectionCoordinator != nullptr) {
            objects_->serverCollectionCoordinator->hideSubscription(subscriptionId);
        }
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::deleteSubscriptionRequested, objects_->mainWindow.get(), [this](const QString& subscriptionId) {
        if (objects_->serverCollectionCoordinator != nullptr) {
            objects_->serverCollectionCoordinator->deleteSubscription(subscriptionId);
        }
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::updateCoreRequested, objects_->mainWindow.get(), [this](int coreTypeValue) {
        if (objects_->coreUpdateCoordinator == nullptr) {
            return;
        }

        CoreUpdateCoordinator::Request request;
        request.coreTypeValue = coreTypeValue;
        request.startAfterSuccess = false;
        request.progressContext = objects_->mainWindow.get();
        request.dialogParent = objects_->mainWindow.get();
        objects_->coreUpdateCoordinator->updateCore(request);
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::updateGeoResourcesRequested, objects_->mainWindow.get(), [this]() {
        if (objects_->geoResourceUpdateCoordinator != nullptr) {
            objects_->geoResourceUpdateCoordinator->updateGeoResources();
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::enableSystemProxyRequested, objects_->mainWindow.get(), [this]() {
        enableSystemProxy(true);
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::retryCoreStartupRequested, objects_->mainWindow.get(), [this]() {
        retryCoreStartup(true);
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::disableSystemProxyRequested, objects_->mainWindow.get(), [this]() {
        disableSystemProxy();
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::tunEnabledChanged, objects_->mainWindow.get(), [this](bool enabled) {
        setTunEnabled(enabled);
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::routingModeSelected, objects_->mainWindow.get(), [this](const QString& routingModeId) {
        const QString previousRoutingModeId = config_.collection().routingModeId;
        const OperationResult result = objects_->routingService->setRoutingMode(config_, routingModeId);
        handleRoutingSelectionResult(result, previousRoutingModeId);
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::settingsRequested, objects_->mainWindow.get(), [this]() {
        openSettingsDialog();
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::openSettingsAtSubscriptionsTabRequested, objects_->mainWindow.get(), [this]() {
        openSettingsDialog(1);
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::openSettingsAtRoutingTabRequested, objects_->mainWindow.get(), [this]() {
        openSettingsDialog(2);
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::aboutRequested, objects_->mainWindow.get(), [this]() {
        openAboutDialog();
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::checkAppUpdateRequested, objects_->mainWindow.get(), [this]() {
        checkAppUpdates(true);
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::downloadAppUpdateRequested, objects_->mainWindow.get(), [this]() {
        if (objects_->appUpdateCheckCoordinator != nullptr) {
            objects_->appUpdateCheckCoordinator->downloadLatestAvailableUpdate(objects_->mainWindow.get());
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::uwpLoopbackRequested, objects_->mainWindow.get(), [this]() {
        openUwpLoopbackDialog();
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::removeServersRequested, objects_->mainWindow.get(), [this](const QStringList& indexIds) {
        if (objects_->serverCollectionCoordinator != nullptr) {
            objects_->serverCollectionCoordinator->removeServers(indexIds);
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::moveServersRequested, objects_->mainWindow.get(), [this](const QStringList& indexIds, int operation) {
        if (objects_->serverCollectionCoordinator != nullptr) {
            objects_->serverCollectionCoordinator->moveServers(indexIds, static_cast<ServerMoveOperation>(operation));
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::reorderServersRequested, objects_->mainWindow.get(), [this](const QStringList& orderedIndexIds) {
        if (objects_->serverCollectionCoordinator != nullptr) {
            objects_->serverCollectionCoordinator->reorderServers(orderedIndexIds);
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::setDefaultServerRequested, objects_->mainWindow.get(), [this](const QString& indexId) {
        if (objects_->defaultServerSwitchCoordinator != nullptr) {
            objects_->defaultServerSwitchCoordinator->setDefaultServer(indexId);
        }
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::setDefaultServerWithTunRequested, objects_->mainWindow.get(), [this](const QString& indexId) {
        if (objects_->defaultServerSwitchCoordinator != nullptr) {
            objects_->defaultServerSwitchCoordinator->setDefaultServerWithTun(indexId);
        }
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::testServersRequested, objects_->mainWindow.get(), [this](const QStringList& indexIds) {
        if (objects_->speedTestCoordinator != nullptr) {
            objects_->speedTestCoordinator->startSpeedTest(indexIds);
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::hiddenToTray, objects_->mainWindow.get(), [this]() {
        objects_->mainWindow->appendLog(QStringLiteral("Main window hidden to tray."));
    });
}

void AppBootstrap::wireTraySignals()
{
    QObject::connect(objects_->trayController.get(), &TrayController::defaultServerRequested, objects_->mainWindow.get(), [this](const QString& indexId) {
        if (objects_->defaultServerSwitchCoordinator != nullptr) {
            objects_->defaultServerSwitchCoordinator->setDefaultServer(indexId);
        }
    });

    QObject::connect(objects_->trayController.get(), &TrayController::routingRequested, objects_->mainWindow.get(), [this](const QString& routingModeId) {
        const QString previousRoutingModeId = config_.collection().routingModeId;
        const OperationResult result = objects_->routingService->selectRouting(config_, routingModeId);
        handleRoutingSelectionResult(result, previousRoutingModeId);
    });

    QObject::connect(objects_->trayController.get(), &TrayController::autoRunToggled, objects_->mainWindow.get(), [this](bool enabled) {
        setAutoRunEnabled(enabled);
    });

    QObject::connect(objects_->trayController.get(), &TrayController::quitRequested, objects_->mainWindow.get(), [this]() {
        objects_->mainWindow->requestExit();
    });
}

void AppBootstrap::syncWindow()
{
    if (qApp != nullptr) {
        AppTheme::applyApplicationTheme(*qApp, config_.ui().themeName);
    }

    if (objects_->mainWindow) {
        objects_->mainWindow->setConfig(config_);
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
            QStringLiteral("Failed to Load Configuration"),
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
    appendResult(OperationResult::ok(QStringLiteral("Configuration reloaded from disk.")));
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
            appendResult(OperationResult::ok(QStringLiteral("Startup core checks skipped by command line.")));
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
    objects_->serverService->save(config_);
}

void AppBootstrap::appendStartupResourceCheckResults()
{
    QStringList lines;

    if (!config_.currentIndexId.trimmed().isEmpty()
        && findServerById(config_.currentIndexId) == nullptr
        && !config_.collection().servers.isEmpty()) {
        lines.append(QStringLiteral(
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
                lines.append(QStringLiteral("Startup check: Custom config file is missing for default server: %1")
                                 .arg(QDir::toNativeSeparators(customConfigPath)));
            }
        }

        if (!skipCoreChecks_) {
            const CoreInfo coreInfo = resolveCoreInfo(*activeServer);
            if (coreInfo.program.trimmed().isEmpty()) {
                const CoreType runtimeCore = resolveLaunchCoreType(*activeServer);
                const QStringList candidates = resolveCoreCandidates(runtimeCore);
                lines.append(QStringLiteral("Startup check: No compatible core executable was found for default server \"%1\". Expected one of: %2.")
                                 .arg(elidedServerDisplayName(*activeServer, 64))
                                 .arg(objects_->coreDiscoveryService->expectedCoreFilesText(candidates)));
            }

            for (const CoreType auxiliaryCoreType : resolveAuxiliaryTunCompatCoreTypes(config_, *activeServer, existingCoreTypes_)) {
                const QStringList candidates = resolveCoreCandidates(auxiliaryCoreType);
                if (locateFirstExistingFile(candidates).isEmpty()) {
                    lines.append(QStringLiteral("Startup check: Default server \"%1\" also needs the %2 core for TUN compatibility. Expected one of: %3.")
                                     .arg(elidedServerDisplayName(*activeServer, 64))
                                     .arg(coreTypeDisplayName(auxiliaryCoreType))
                                     .arg(objects_->coreDiscoveryService->expectedCoreFilesText(candidates)));
                }
            }
        }
    }

    if (missingCustomConfigCount > 0) {
        lines.append(QStringLiteral("Startup check: %1 custom server config file(s) are missing.").arg(missingCustomConfigCount));
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
        return OperationResult::fail(QStringLiteral("TUN runtime service is unavailable."));
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
            QStringLiteral("Cleaned up orphan core processes: %1").arg(terminatedProcesses.join(QStringLiteral(", ")))));
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
            QStringLiteral("Cleaned up core processes using configured ports: %1")
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
        appendResult(OperationResult::fail(QStringLiteral("System proxy mode cannot be changed before the configuration service is ready.")));
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

void AppBootstrap::applySystemProxyModeOnExit(bool windowsShutdown)
{
    if (objects_->systemProxyCoordinator != nullptr) {
        objects_->systemProxyCoordinator->applyModeOnExit(windowsShutdown);
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
        appendResult(OperationResult::fail(QStringLiteral("TUN mode cannot be changed before the configuration service is ready.")));
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
        appendResult(OperationResult::fail(QStringLiteral("Clipboard text is empty.")));
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
        restartCoreIfRunning(QStringLiteral("Reloading core to apply routing changes."), true);
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
