#include "app/AppBootstrap.h"
#include "app/AppUpdateInstallService.h"
#include "app/BackgroundTaskCoordinator.h"
#include "app/ConfigPathResolver.h"
#include "app/CoreProcessCleanupService.h"
#include "app/CoreDiscoveryService.h"
#include "app/OutboundLocationProbeService.h"
#include "app/ProxySession.h"
#include "app/RuntimeState.h"
#include "common/AppPlatform.h"
#include "common/DialogUtils.h"
#include "common/GitHubUrls.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QGuiApplication>
#include <QIODevice>
#include <QMetaObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSessionManager>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include <utility>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <shellapi.h>
#endif

#include <memory>
#include <optional>

#include "app/StartupAdminElevation.h"
#include "app/AppBootstrapUiCommandPlan.h"
#include "app/SettingsDialogSession.h"
#include "app/SettingsDialogApplyPlan.h"
#include "app/SingleInstanceBootstrap.h"
#include "app/TunSettingsApplyDecision.h"
#include "app/TunRuntimeState.h"
#include "app/TunRuntimeService.h"
#include "common/ServerDisplayName.h"
#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"
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
#include "services/SpeedTestService.h"
#include "services/SubscriptionService.h"
#include "services/SubscriptionUpdateService.h"
#include "subscription/CustomConfigTextParser.h"
#include "subscription/SubscriptionImportTextParser.h"
#include "ui/dialogs/AddServerDialog.h"
#include "ui/dialogs/AboutDialog.h"
#include "ui/dialogs/CustomServerDialog.h"
#include "ui/dialogs/SettingsDialog.h"
#include "ui/dialogs/UwpLoopbackDialog.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/theme/AppTheme.h"
#include "ui/tray/TrayController.h"

namespace {

constexpr auto DefaultIeProxyExceptions =
    "localhost;127.*;10.*;172.16.*;172.17.*;172.18.*;172.19.*;172.20.*;172.21.*;172.22.*;"
    "172.23.*;172.24.*;172.25.*;172.26.*;172.27.*;172.28.*;172.29.*;172.30.*;172.31.*;192.168.*";
constexpr auto AppReleaseDate = __DATE__;

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

QStringList splitProxyExceptions(const QString& value)
{
    QString normalized = value;
    normalized.replace(QChar(','), QChar(';'));

    QStringList parts;
    for (const QString& item : normalized.split(QChar(';'), Qt::SkipEmptyParts)) {
        const QString trimmed = item.trimmed();
        if (!trimmed.isEmpty()) {
            parts.append(trimmed);
        }
    }
    return parts;
}

void appendUniqueProxyException(QStringList& entries, const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    for (const QString& existing : std::as_const(entries)) {
        if (existing.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return;
        }
    }
    entries.append(trimmed);
}

QStringList collectRouteDerivedProxyExceptions(const Config& config)
{
    QList<RoutingRule> effectiveRules;
    for (const RoutingRule& rule : config.collection().routingCustomRules) {
        if (rule.enabled) {
            effectiveRules.append(rule);
        }
    }

    if (config.collection().enableRoutingAdvanced
        && config.collection().routingIndex >= 0
        && config.collection().routingIndex < config.collection().routingItems.size()) {
        const RoutingItem& selectedRouting = config.collection().routingItems.at(config.collection().routingIndex);
        for (const RoutingRule& rule : selectedRouting.rules) {
            if (rule.enabled) {
                effectiveRules.append(rule);
            }
        }
    }

    QStringList derived;
    for (const RoutingRule& rule : std::as_const(effectiveRules)) {
        if (rule.outboundTag.compare(QStringLiteral("direct"), Qt::CaseInsensitive) != 0) {
            continue;
        }

        for (const QString& domainValue : rule.domain) {
            const QString domain = domainValue.trimmed();
            if (domain.startsWith(QStringLiteral("domain:"), Qt::CaseInsensitive)) {
                const QString suffix = domain.mid(QStringLiteral("domain:").size()).trimmed();
                if (suffix.isEmpty()) {
                    continue;
                }
                appendUniqueProxyException(derived, suffix);
                appendUniqueProxyException(derived, QStringLiteral("*.%1").arg(suffix));
                continue;
            }

            if (domain.startsWith(QStringLiteral("full:"), Qt::CaseInsensitive)) {
                appendUniqueProxyException(derived, domain.mid(QStringLiteral("full:").size()).trimmed());
            }
        }
    }

    return derived;
}
bool interactivePromptsEnabled()
{
    return !qEnvironmentVariableIsSet("SONGBIRD_NONINTERACTIVE");
}

QString tunAdminRequiredStartMessage()
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "TUN mode requires administrator privileges on Windows. Run SongBird as administrator before starting the core.");
}

QString tunAdminRequiredSaveMessage()
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "TUN mode requires administrator privileges on Windows. The settings were saved and will take effect after restarting SongBird as administrator.");
}

QString tunAdminRestartPromptTitle()
{
    return QCoreApplication::translate("AppBootstrap", "Administrator Permission");
}

QString tunAdminRestartPromptMessage()
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "TUN mode requires administrator privileges on Windows.\nRestart SongBird as administrator now to apply the saved TUN setting?");
}

QString tunAdminRestartFailureMessage()
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "Failed to restart SongBird with administrator privileges.");
}

OperationResult importCustomConfigTextWithService(
    const QString& text,
    Config& config,
    ServerService& serverService)
{
    QString extension;
    bool ok = false;
    VmessItem item = CustomConfigTextParser::parse(text, &extension, &ok);
    if (!ok) {
        return OperationResult::fail(QString());
    }

    QString tempDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempDirectory.trimmed().isEmpty()) {
        tempDirectory = QDir::tempPath();
    }
    if (!QDir().mkpath(tempDirectory)) {
        return OperationResult::fail(QStringLiteral("Failed to create temporary directory for custom config import."));
    }

    const QString fileName = extension.trimmed().isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
        : QStringLiteral("%1.%2").arg(QUuid::createUuid().toString(QUuid::WithoutBraces), extension.trimmed());
    const QString tempFilePath = QDir(tempDirectory).filePath(fileName);

    QFile file(tempFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return OperationResult::fail(QStringLiteral("Failed to create temporary custom config file."));
    }
    if (file.write(text.toUtf8()) < 0) {
        file.close();
        QFile::remove(tempFilePath);
        return OperationResult::fail(QStringLiteral("Failed to write temporary custom config file."));
    }
    file.close();

    item.address = tempFilePath;
    const OperationResult result = serverService.addServer(config, item);
    QFile::remove(tempFilePath);
    return result;
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
{
}

AppBootstrap::~AppBootstrap()
{
    shuttingDown_.store(true);
    lifetimeGuard_.reset();
    waitForBackgroundThreads();

    if (proxySession_ != nullptr && (proxySession_->isCoreRunning() || proxySession_->isTransitioning())) {
        proxySession_->stop(true);
    }
}

bool AppBootstrap::run()
{
    repository_ = std::make_unique<JsonConfigRepository>(resolveConfigPath());
    serverService_ = std::make_unique<ServerService>(*repository_, resolveCustomConfigDirectory());
    configBackupService_ = std::make_unique<ConfigBackupService>(resolveConfigPath());
    routingService_ = std::make_unique<RoutingService>(*repository_);
    speedTestService_ = std::make_unique<SpeedTestService>(resolveCustomConfigDirectory());
    subscriptionService_ = std::make_unique<SubscriptionService>(*repository_);
    geoResourceUpdateService_ = std::make_unique<GeoResourceUpdateService>(
        QFileInfo(resolveConfigPath()).dir().absolutePath());
    clientConfigWriter_ = std::make_unique<ClientConfigWriter>(resolveCustomConfigDirectory());
    coreProcessHost_ = std::make_unique<QtCoreProcessHost>();
    backgroundTasks_ = std::make_unique<BackgroundTaskCoordinator>();
    coreProcessCleanupService_ = std::make_unique<CoreProcessCleanupService>();
    coreDiscoveryService_ = std::make_unique<CoreDiscoveryService>();
    outboundLocationProbeService_ = std::make_unique<OutboundLocationProbeService>();
    appUpdateInstallService_ = std::make_unique<AppUpdateInstallService>(
        [](const QString& program, const QStringList& arguments) {
            return restartProcessAsAdministrator(program, arguments);
        });
    tunRuntimeService_ = std::make_unique<TunRuntimeService>();
    auxiliaryCoreProcessHost_ = std::make_unique<QtCoreProcessHost>();
    autoRunService_ = std::make_unique<WindowsAutoRunService>();
    systemProxyService_ = std::make_unique<WindowsSystemProxyService>();
    proxySession_ = std::make_unique<ProxySession>(ProxySession::Dependencies{
        *coreProcessHost_,
        *auxiliaryCoreProcessHost_,
        *clientConfigWriter_,
        *outboundLocationProbeService_,
        *backgroundTasks_,
        ProxySession::RuntimeResolver{
            [this]() { return resolveActiveServerSnapshot(); },
            [this](const VmessItem& s) { return resolveLaunchCoreType(s); },
            [this](const VmessItem& s) { return resolveCoreInfo(s); },
            [this](const VmessItem& s) { return resolveRuntimeConfigPath(s); },
            [this](CoreType ct) { return resolveCoreCandidates(ct); },
            [this](const QStringList& c) { return locateFirstExistingFile(c); },
            [this]() { cleanupCoreProcessesUsingConfiguredPorts(); },
            [this]() { removeStaleSingBoxCache(); },
            [this]() { refreshExistingCoreTypes(); },
            [this]() { return removeStaleTunAdapterIfPresent(); },
            [this]() { return skipCoreChecks_; },
            [this]() { return isWindowsPlatform(); },
            [this]() { return isProcessElevated(); },
            [this]() { return systemProxyService_ != nullptr && systemProxyService_->isEnabled(); },
            [this]() { cancelBackgroundTasksForProxyStartup(); },
            [this]() { return config_.currentIndexId.trimmed(); },
            [this](CoreType ct) { return resolveRuntimeCoreType(ct); },
            [this](CoreType ct) { return resolveCoreInstallDirectory(ct); },
            [this](SystemProxyMode mode) { return updateSystemProxyMode(mode); }
        }
    });
    runtimeState_ = std::make_unique<RuntimeState>();
    backgroundTasks_->setBlockingPredicate([this]() {
        return isProxyActivationInProgress();
    });
    QObject::connect(backgroundTasks_.get(), &BackgroundTaskCoordinator::blockedByCoreStartup,
        backgroundTasks_.get(), [this]() {
            appendResult(OperationResult::fail(
                QCoreApplication::translate("AppBootstrap", "Proxy startup is in progress.")));
        });
    mainWindow_ = std::make_unique<MainWindow>();
    QObject::connect(speedTestService_.get(), &SpeedTestService::runningChanged, mainWindow_.get(), [this](bool running) {
        if (!running) {
            backgroundTasks_->resetSpeedTestProgress();
            if (!backgroundTasks_->isCurrent(speedTestTaskToken_)) {
                speedTestResultsDirty_ = false;
                speedTestTaskToken_ = {};
                return;
            }
            if (speedTestResultsDirty_ && serverService_ != nullptr && !serverService_->save(config_)) {
                appendResult(OperationResult::fail(QStringLiteral("Failed to save configuration after updating test results.")));
            }
            speedTestResultsDirty_ = false;
            backgroundTasks_->finish(speedTestTaskToken_);
            speedTestTaskToken_ = {};
        } else {
            speedTestResultsDirty_ = false;
            backgroundTasks_->syncState();
        }
    });
    QObject::connect(speedTestService_.get(), &SpeedTestService::logGenerated, mainWindow_.get(), [this](const QString& message) {
        if (!backgroundTasks_->isCurrent(speedTestTaskToken_)) {
            return;
        }
        QPointer<MainWindow> mainWindowGuard(mainWindow_.get());
        if (!mainWindowGuard.isNull()) {
            mainWindowGuard->appendLog(message);
            mainWindowGuard->showTransientStatus(
                message,
                3000,
                MainWindow::TransientStatusPriority::Routine);
        }
    });
    QObject::connect(speedTestService_.get(), &SpeedTestService::testResultReady, mainWindow_.get(), [this](const QString& indexId, const QString& result) {
        if (!backgroundTasks_->isCurrent(speedTestTaskToken_)) {
            return;
        }
        if (serverService_ == nullptr) {
            return;
        }

        const OperationResult updateResult = serverService_->setTestResult(config_, indexId, result);
        if (!updateResult.success) {
            appendResult(updateResult);
            return;
        }
        speedTestResultsDirty_ = true;
        const VmessItem* speedTestServer = findServerById(indexId);
        const QString serverName = speedTestServer == nullptr ? QString() : serverDisplayName(*speedTestServer);
        backgroundTasks_->recordSpeedTestResult(serverName);
        backgroundTasks_->syncState();

        if (mainWindow_ != nullptr) {
            mainWindow_->updateServerTestResult(indexId, result);
        }
        if (trayController_ != nullptr) {
            trayController_->setServers(
                config_.collection().servers,
                config_.currentIndexId);
        }
    });
    QObject::connect(speedTestService_.get(), &SpeedTestService::finished, mainWindow_.get(), [this](const QString& summary) {
        if (!backgroundTasks_->isCurrent(speedTestTaskToken_)) {
            return;
        }
        if (mainWindow_ != nullptr) {
            mainWindow_->showTransientStatus(summary, 4000);
        }
    });

    trayController_ = std::make_unique<TrayController>(mainWindow_.get());

    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, mainWindow_.get(), [this]() {
        shuttingDown_.store(true);
        cleanupRuntimeForExit(windowsShutdownRequested_);
        if (!shutdownUiStatePersisted_) {
            persistUiState();
            shutdownUiStatePersisted_ = true;
        }
    });
    if (QGuiApplication* guiApplication = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        QObject::connect(guiApplication, &QGuiApplication::commitDataRequest, mainWindow_.get(), [this](QSessionManager&) {
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
    mainWindow_->setHideToTrayEnabled(trayController_->initialize());
    refreshExistingCoreTypes();
    if (!reloadConfig()) {
        return false;
    }
    autoBackupCurrentConfig();
    if (systemProxyService_ != nullptr) {
        const bool managedSystemProxyActive = shouldAdoptManagedSystemProxyOnStartup(
            normalizeSystemProxyMode(config_.sysProxyType),
            config_.ui().mainProxyEnabled,
            systemProxyService_->isEnabled());
        proxySession_->adoptManagedSystemProxy(managedSystemProxyActive);
    }
    mainWindow_->show();
    if (startHidden_ || !config_.ui().showMainOnStartup) {
        if (trayController_ != nullptr && trayController_->isAvailable()) {
            mainWindow_->hide();
        } else {
            mainWindow_->showMinimized();
            appendResult(OperationResult::ok(QStringLiteral(
                "Start hidden requested, but the system tray is unavailable. The window was minimized instead.")));
        }
    }
    uiReady_ = true;
    QTimer::singleShot(0, mainWindow_.get(), [this]() {
        applyStartupSystemProxyPreference();
    });
    QTimer::singleShot(3000, mainWindow_.get(), [this]() {
        checkAppUpdates(false);
    });

    return true;
}

void AppBootstrap::wireMainWindow()
{
    QObject::connect(proxySession_.get(), &ProxySession::phaseChanged,
                     mainWindow_.get(), [this](ProxySession::Phase phase) {
                         switch (phase) {
                         case ProxySession::Phase::Stopped:
                             setRuntimePhase(RuntimePhase::Stopped);
                             break;
                         case ProxySession::Phase::EnvironmentCleanup:
                             setRuntimePhase(RuntimePhase::EnvironmentCleanup);
                             break;
                         case ProxySession::Phase::ValidateCoreApplication:
                             setRuntimePhase(RuntimePhase::ValidateCoreApplication);
                             break;
                         case ProxySession::Phase::ValidateRuntimeResources:
                             setRuntimePhase(RuntimePhase::ValidateRuntimeResources);
                             break;
                         case ProxySession::Phase::ValidateCoreConfig:
                             setRuntimePhase(RuntimePhase::ValidateCoreConfig);
                             break;
                         case ProxySession::Phase::StartTunRuntime:
                             setRuntimePhase(RuntimePhase::StartTunRuntime);
                             break;
                         case ProxySession::Phase::StartCoreProcess:
                             setRuntimePhase(RuntimePhase::StartCoreProcess);
                             break;
                         case ProxySession::Phase::CheckOutboundLocation:
                             setRuntimePhase(RuntimePhase::CheckOutboundLocation);
                             break;
                         case ProxySession::Phase::ApplySystemProxy:
                             setRuntimePhase(RuntimePhase::ApplySystemProxy);
                             break;
                         case ProxySession::Phase::Proxying:
                             setRuntimePhase(RuntimePhase::Proxying);
                             break;
                         case ProxySession::Phase::Stopping:
                             setRuntimePhase(RuntimePhase::Stopping);
                             break;
                         }
                     });
    QObject::connect(proxySession_.get(), &ProxySession::checklistUpdated,
                     mainWindow_.get(), &MainWindow::setCoreStartupChecklist);
    QObject::connect(proxySession_.get(), &ProxySession::checklistCleared,
                     mainWindow_.get(), &MainWindow::clearCoreStartupChecklist);
    QObject::connect(proxySession_.get(), &ProxySession::statusSyncRequested,
                     mainWindow_.get(), [this]() {
                         syncStatusIndicators();
                     });
    QObject::connect(proxySession_.get(), &ProxySession::coreOutput,
                     mainWindow_.get(), [this](const QString& line) { appendResult(OperationResult::ok(line)); });
    QObject::connect(proxySession_.get(), &ProxySession::auxiliaryCoreOutput,
                     mainWindow_.get(), [this](const QString& line) {
                         appendResult(OperationResult::ok(QStringLiteral("tun-compat | %1").arg(line)));
                     });
    QObject::connect(proxySession_.get(), &ProxySession::failed,
                     mainWindow_.get(), [this](const QString& reason) {
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
    QObject::connect(proxySession_.get(), &ProxySession::activated,
                     mainWindow_.get(), [this]() {
                         setCurrentActivationPending_ = false;
                     });
    QObject::connect(proxySession_.get(), &ProxySession::stopped,
                     mainWindow_.get(), [this]() {
                         syncStatusIndicators();
                     });
    QObject::connect(proxySession_.get(), &ProxySession::logMessage,
                     mainWindow_.get(), [this](const QString& msg) { appendResult(OperationResult::ok(msg)); });
    QObject::connect(proxySession_.get(), &ProxySession::transientStatus,
                     mainWindow_.get(), [this](const QString& msg, int timeoutMs) {
                         if (mainWindow_) { mainWindow_->showTransientStatus(msg, timeoutMs); }
                     });
    QObject::connect(proxySession_.get(), &ProxySession::coreUpdateResumeRequested,
                     mainWindow_.get(), [this]() { continuePendingCoreUpdate(); });
    QObject::connect(proxySession_.get(), &ProxySession::serverSwitchResumeRequested,
                     mainWindow_.get(), [this](const QString& indexId, bool enableTun, bool showOverlay) {
                         scheduleDefaultServerSwitchAfterCoreStopped(indexId, enableTun, showOverlay);
                     });

    QObject::connect(runtimeState_.get(), &RuntimeState::snapshotApplied,
                     mainWindow_.get(), &MainWindow::applyRuntimeState);
    QObject::connect(runtimeState_.get(), &RuntimeState::currentServerChanged,
                     trayController_.get(), [this](const QString& name, const QString&, const QString&) {
                         if (trayController_ != nullptr) {
                             trayController_->setCurrentServerName(name);
                         }
                     });
    QObject::connect(runtimeState_.get(), &RuntimeState::proxyUiStateChanged,
                     trayController_.get(), [this](ProxyUiState state) {
                         if (trayController_ == nullptr) {
                             return;
                         }
                         trayController_->setProxyUiState(state);
                     });
    QObject::connect(runtimeState_.get(), &RuntimeState::systemProxyStateChanged,
                     trayController_.get(), [this](int mode, bool enabled) {
                         if (trayController_ != nullptr) {
                             trayController_->setSystemProxyState(mode, enabled);
                         }
                     });
    QObject::connect(runtimeState_.get(), &RuntimeState::autoRunChanged,
                     trayController_.get(), [this](bool enabled) {
                         if (trayController_ != nullptr) {
                             trayController_->setAutoRunEnabled(enabled);
                         }
                     });
    QObject::connect(runtimeState_.get(), &RuntimeState::routingStatusChanged,
                     trayController_.get(), [this](const QString& routingText, const QString&, bool advancedEnabled) {
                         if (trayController_ != nullptr) {
                             trayController_->setRoutingSummary(routingText, advancedEnabled);
                         }
                     });

    QObject::connect(backgroundTasks_.get(), &BackgroundTaskCoordinator::runningChanged,
                     mainWindow_.get(), &MainWindow::setBackgroundTaskRunning);
    QObject::connect(backgroundTasks_.get(), &BackgroundTaskCoordinator::descriptionChanged,
                     mainWindow_.get(), &MainWindow::setBackgroundTaskDescription);
    QObject::connect(backgroundTasks_.get(), &BackgroundTaskCoordinator::runningChanged,
                     trayController_.get(), &TrayController::setBackgroundTaskRunning);
    QObject::connect(backgroundTasks_.get(), &BackgroundTaskCoordinator::descriptionChanged,
                     trayController_.get(), &TrayController::setBackgroundTaskDescription);

    QObject::connect(mainWindow_.get(), &MainWindow::addServerRequested, mainWindow_.get(), [this]() {
        AddServerDialog dialog(mainWindow_.get());
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        appendResult(serverService_->addServer(config_, dialog.server()));
        syncWindow();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::editServerRequested, mainWindow_.get(), [this](const QString& indexId) {
        const std::optional<VmessItem> existing = findServerSnapshotById(indexId);
        QPointer<MainWindow> mainWindowGuard(mainWindow_.get());
        if (!existing.has_value()) {
            if (!mainWindowGuard.isNull()) {
                mainWindowGuard->appendLog(QStringLiteral("The selected server could not be found for editing."));
            }
            return;
        }

        const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
        const QString activeServerId = activeServer.has_value() ? activeServer->indexId : QString();
        const bool shouldRestartCore = isCoreRunning() && activeServerId == indexId;
        OperationResult result;

        if (existing->configType == ConfigType::Custom) {
            CustomServerDialog dialog(mainWindowGuard.data());
            dialog.setWindowTitle(QStringLiteral("Edit Custom Server"));
            dialog.setServer(*existing, serverService_->resolveCustomConfigPath(existing->address));
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }

            result = serverService_->updateServer(config_, indexId, dialog.server());
        } else {
            AddServerDialog dialog(mainWindowGuard.data());
            dialog.setWindowTitle(QStringLiteral("Edit Server"));
            dialog.setServer(*existing);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }

            result = serverService_->updateServer(config_, indexId, dialog.server());
        }
        appendResult(result);
        syncWindow();
        if (result.success && shouldRestartCore) {
            restartCoreIfRunning(QStringLiteral("Reloading core after editing the active server."), true);
        }
    });

    QObject::connect(mainWindow_.get(), &MainWindow::importFromClipboardRequested, mainWindow_.get(), [this]() {
        importFromClipboard();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::updateSubscriptionsRequested, mainWindow_.get(), [this]() {
        updateAllSubscriptions();
    });
    QObject::connect(
        mainWindow_.get(),
        &MainWindow::updateCurrentSubscriptionRequested,
        mainWindow_.get(),
        [this](const QString& subscriptionId) {
            updateCurrentSubscription(subscriptionId);
        });
    QObject::connect(
        mainWindow_.get(),
        &MainWindow::updateCurrentSubscriptionViaProxyRequested,
        mainWindow_.get(),
        [this](const QString& subscriptionId) {
            updateCurrentSubscriptionViaProxy(subscriptionId);
        });
    QObject::connect(mainWindow_.get(), &MainWindow::hideSubscriptionRequested, mainWindow_.get(), [this](const QString& subscriptionId) {
        hideSubscription(subscriptionId);
    });
    QObject::connect(mainWindow_.get(), &MainWindow::deleteSubscriptionRequested, mainWindow_.get(), [this](const QString& subscriptionId) {
        deleteSubscription(subscriptionId);
    });
    QObject::connect(mainWindow_.get(), &MainWindow::updateCoreRequested, mainWindow_.get(), [this](int coreTypeValue) {
        updateCore(coreTypeValue);
    });
    QObject::connect(mainWindow_.get(), &MainWindow::updateGeoResourcesRequested, mainWindow_.get(), [this]() {
        updateGeoResources();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::enableSystemProxyRequested, mainWindow_.get(), [this]() {
        enableSystemProxy(true);
    });
    QObject::connect(mainWindow_.get(), &MainWindow::retryCoreStartupRequested, mainWindow_.get(), [this]() {
        retryCoreStartup(true);
    });
    QObject::connect(mainWindow_.get(), &MainWindow::disableSystemProxyRequested, mainWindow_.get(), [this]() {
        disableSystemProxy();
    });
    QObject::connect(mainWindow_.get(), &MainWindow::tunEnabledChanged, mainWindow_.get(), [this](bool enabled) {
        setTunEnabled(enabled);
    });
    QObject::connect(mainWindow_.get(), &MainWindow::routingModeSelected, mainWindow_.get(), [this](int mode) {
        const bool previousAdvancedEnabled = config_.collection().enableRoutingAdvanced;
        const int previousRoutingIndex = config_.collection().routingIndex;
        const OperationResult result = routingService_->setRoutingMode(config_, mode >= 0, mode);
        handleRoutingSelectionResult(result, previousAdvancedEnabled, previousRoutingIndex);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::settingsRequested, mainWindow_.get(), [this]() {
        openSettingsDialog();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::openSettingsAtSubscriptionsTabRequested, mainWindow_.get(), [this]() {
        openSettingsDialog(1);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::openSettingsAtRoutingTabRequested, mainWindow_.get(), [this]() {
        openSettingsDialog(2);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::aboutRequested, mainWindow_.get(), [this]() {
        openAboutDialog();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::checkAppUpdateRequested, mainWindow_.get(), [this]() {
        checkAppUpdates(true);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::uwpLoopbackRequested, mainWindow_.get(), [this]() {
        openUwpLoopbackDialog();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::removeServersRequested, mainWindow_.get(), [this](const QStringList& indexIds) {
        const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
        const QString activeServerId = activeServer.has_value() ? activeServer->indexId : QString();
        const bool activeServerRemoved = !activeServerId.isEmpty() && indexIds.contains(activeServerId);
        const OperationResult result = serverService_->removeServers(config_, indexIds);
        appendResult(result);
        syncWindow();
        if (!result.success || !activeServerRemoved || !isCoreRunning()) {
            return;
        }

        if (!resolveActiveServerSnapshot().has_value()) {
            appendResult(OperationResult::ok(QStringLiteral("Stopping core because the active server was removed.")));
            stopCore(false);
            return;
        }

        restartCoreIfRunning(QStringLiteral("Reloading core after removing the active server."), true);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::moveServersRequested, mainWindow_.get(), [this](const QStringList& indexIds, int operation) {
        appendResult(serverService_->moveServers(config_, indexIds, static_cast<ServerMoveOperation>(operation)));
        syncWindow();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::reorderServersRequested, mainWindow_.get(), [this](const QStringList& orderedIndexIds) {
        appendResult(serverService_->reorderServers(config_, orderedIndexIds));
        syncWindow();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::setDefaultServerRequested, mainWindow_.get(), [this](const QString& indexId) {
        if (requestDefaultServerSwitchAfterCoreStop(indexId, false)) {
            return;
        }

        const QString previousIndexId = config_.currentIndexId;
        const OperationResult result = serverService_->setDefaultServer(config_, indexId);
        handleDefaultServerSelectionResult(result, previousIndexId);
    });
    QObject::connect(mainWindow_.get(), &MainWindow::setDefaultServerWithTunRequested, mainWindow_.get(), [this](const QString& indexId) {
        setDefaultServerWithTun(indexId);
    });
    QObject::connect(mainWindow_.get(), &MainWindow::testServersRequested, mainWindow_.get(), [this](const QStringList& indexIds) {
        startSpeedTest(indexIds);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::hiddenToTray, mainWindow_.get(), [this]() {
        mainWindow_->appendLog(QStringLiteral("Main window hidden to tray."));
    });

    QObject::connect(trayController_.get(), &TrayController::defaultServerRequested, mainWindow_.get(), [this](const QString& indexId) {
        if (requestDefaultServerSwitchAfterCoreStop(indexId, false)) {
            return;
        }

        const QString previousIndexId = config_.currentIndexId;
        const OperationResult result = serverService_->setDefaultServer(config_, indexId);
        handleDefaultServerSelectionResult(result, previousIndexId);
    });

    QObject::connect(trayController_.get(), &TrayController::routingRequested, mainWindow_.get(), [this](int index) {
        const int previousRoutingIndex = config_.collection().routingIndex;
        const bool previousAdvancedEnabled = config_.collection().enableRoutingAdvanced;
        const OperationResult result = routingService_->selectRouting(config_, index);
        handleRoutingSelectionResult(result, previousAdvancedEnabled, previousRoutingIndex);
    });

    QObject::connect(trayController_.get(), &TrayController::quitRequested, mainWindow_.get(), [this]() {
        mainWindow_->requestExit();
    });

}

void AppBootstrap::syncWindow()
{
    if (qApp != nullptr) {
        AppTheme::applyApplicationTheme(*qApp, config_.ui().themeName);
    }

    if (mainWindow_) {
        mainWindow_->setConfig(config_);
    }

    if (trayController_ != nullptr) {
        trayController_->setServers(
            config_.collection().servers,
            config_.currentIndexId);
        trayController_->setRoutings(
            config_.collection().routingItems,
            config_.collection().routingIndex,
            config_.collection().enableRoutingAdvanced);
    }

    syncStatusIndicators();
}

bool AppBootstrap::isProxyActivationInProgress() const
{
    return proxySession_ != nullptr && proxySession_->isActivationInProgress();
}

void AppBootstrap::syncStatusIndicators()
{
    systemProxyMode_ = normalizeSystemProxyMode(config_.sysProxyType);
    const bool systemProxyEnabled = systemProxyService_ != nullptr && systemProxyService_->isEnabled();
    const bool autoRunEnabled = autoRunService_ != nullptr && autoRunService_->isEnabled();
    const VmessItem* currentServer = findServerById(config_.currentIndexId);
    const QString currentServerName = currentServer == nullptr ? QString() : serverDisplayName(*currentServer);
    const QString currentServerLocation = proxySession_ != nullptr ? proxySession_->serverLocation() : QString();
    const QString currentServerWarning = proxySession_ != nullptr ? proxySession_->serverWarning() : QString();

    if (mainWindow_ != nullptr) {
        mainWindow_->setExistingCoreTypes(existingCoreTypes_);
    }

    if (runtimeState_ != nullptr) {
        RuntimeStateSnapshot snapshot;
        snapshot.currentServerName = currentServerName;
        snapshot.currentServerLocation = currentServerLocation;
        snapshot.currentServerWarning = currentServerWarning;
        snapshot.routingSummary = buildRoutingSummaryText();
        snapshot.listenSummary = buildListenSummaryText();
        snapshot.proxyUiState = computeProxyUiState();
        snapshot.systemProxyMode = systemProxyMode_;
        snapshot.systemProxyApplied = systemProxyEnabled;
        snapshot.autoRunEnabled = autoRunEnabled;
        snapshot.routingAdvancedEnabled = config_.collection().enableRoutingAdvanced;
        logProxySyncDiagnostic(snapshot);
        runtimeState_->applySnapshot(snapshot);
    }

    if (backgroundTasks_ != nullptr) {
        backgroundTasks_->syncState();
    }
}

void AppBootstrap::logProxySyncDiagnostic(const RuntimeStateSnapshot& snapshot)
{
    const auto stateName = [](ProxyUiState state) -> QString {
        switch (state) {
        case ProxyUiState::Idle:
            return QStringLiteral("Idle");
        case ProxyUiState::Transitioning:
            return QStringLiteral("Transitioning");
        case ProxyUiState::Active:
            return QStringLiteral("Active");
        case ProxyUiState::Inconsistent:
            return QStringLiteral("Inconsistent");
        }
        return QStringLiteral("Unknown");
    };

    if (snapshot.proxyUiState != ProxyUiState::Inconsistent) {
        if (lastProxyUiStateLogged_.has_value()
            && *lastProxyUiStateLogged_ == ProxyUiState::Inconsistent
            && mainWindow_ != nullptr) {
            mainWindow_->appendLog(
                QStringLiteral("Proxy UI state recovered to %1").arg(stateName(snapshot.proxyUiState)));
        }
        lastProxyUiStateLogged_ = snapshot.proxyUiState;
        lastProxySyncDiagnostic_.clear();
        return;
    }

    if (lastProxyUiStateLogged_.has_value() && *lastProxyUiStateLogged_ == ProxyUiState::Inconsistent) {
        return;
    }

    const auto phaseName = [](RuntimePhase phase) -> QString {
        switch (phase) {
        case RuntimePhase::Stopped:
            return QStringLiteral("Stopped");
        case RuntimePhase::EnvironmentCleanup:
            return QStringLiteral("EnvironmentCleanup");
        case RuntimePhase::ValidateCoreApplication:
            return QStringLiteral("ValidateCoreApplication");
        case RuntimePhase::ValidateRuntimeResources:
            return QStringLiteral("ValidateRuntimeResources");
        case RuntimePhase::ValidateCoreConfig:
            return QStringLiteral("ValidateCoreConfig");
        case RuntimePhase::StartTunRuntime:
            return QStringLiteral("StartTunRuntime");
        case RuntimePhase::StartCoreProcess:
            return QStringLiteral("StartCoreProcess");
        case RuntimePhase::CheckOutboundLocation:
            return QStringLiteral("CheckOutboundLocation");
        case RuntimePhase::ApplySystemProxy:
            return QStringLiteral("ApplySystemProxy");
        case RuntimePhase::Proxying:
            return QStringLiteral("Proxying");
        case RuntimePhase::Stopping:
            return QStringLiteral("Stopping");
        }
        return QStringLiteral("Unknown");
    };

    const QString diagnostic = QStringLiteral(
        "Proxy UI state inconsistent: phase=%1 coreProcessRunning=%2 coreReady=%3 "
        "systemProxyApplied=%4 currentServerLocation=\"%5\" systemProxyMode=%6")
        .arg(phaseName(runtimePhase_))
        .arg(isCoreRunning() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(isCoreReady() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(snapshot.systemProxyApplied ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(snapshot.currentServerLocation.trimmed())
        .arg(systemProxyModeDisplayName(snapshot.systemProxyMode));

    lastProxySyncDiagnostic_ = diagnostic;
    lastProxyUiStateLogged_ = ProxyUiState::Inconsistent;
    if (mainWindow_ != nullptr) {
        mainWindow_->appendLog(diagnostic);
    }
}

ProxyUiState AppBootstrap::computeProxyUiState() const
{
    if (proxySession_ == nullptr) {
        return ProxyUiState::Idle;
    }

    if (proxySession_->isTransitioning()) {
        return ProxyUiState::Transitioning;
    }

    const bool coreRunning = isCoreRunning();
    const bool coreReady = isCoreReady();
    const bool locationAvailable = !proxySession_->serverLocation().trimmed().isEmpty();

    if (coreReady
        && (systemProxyService_ != nullptr && systemProxyService_->isEnabled())
        && locationAvailable) {
        return ProxyUiState::Active;
    }
    if (!coreReady && !coreRunning) {
        return ProxyUiState::Idle;
    }
    return ProxyUiState::Inconsistent;
}

void AppBootstrap::setRuntimePhase(RuntimePhase phase)
{
    if (runtimePhase_ == phase) {
        return;
    }

    runtimePhase_ = phase;
    syncStatusIndicators();
}

bool AppBootstrap::reloadConfig()
{
    refreshExistingCoreTypes();
    const Config loadedConfig = repository_->load();
    const QString loadError = repository_->lastLoadError().trimmed();
    if (!loadError.isEmpty()) {
        appendResult(OperationResult::fail(loadError));
        DialogUtils::showCritical(
            mainWindow_.get(),
            QStringLiteral("Failed to Load Configuration"),
            loadError);
        return false;
    }

    config_ = loadedConfig;
    syncWindow();
    if (!uiStateRestored_ && mainWindow_ != nullptr) {
        mainWindow_->restoreUiState(config_);
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

namespace {

VmessItem runtimeServerForLaunchCore(const VmessItem& server, CoreType launchCore)
{
    VmessItem runtimeServer = server;
    runtimeServer.coreType = launchCore;
    return runtimeServer;
}

} // namespace

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
    if (mainWindow_ == nullptr || serverService_ == nullptr) {
        return;
    }

    mainWindow_->captureUiState(config_);
    serverService_->save(config_);
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
                                 .arg(coreDiscoveryService_->expectedCoreFilesText(candidates)));
            }

            for (const CoreType auxiliaryCoreType : resolveAuxiliaryTunCompatCoreTypes(config_, *activeServer, existingCoreTypes_)) {
                const QStringList candidates = resolveCoreCandidates(auxiliaryCoreType);
                if (locateFirstExistingFile(candidates).isEmpty()) {
                    lines.append(QStringLiteral("Startup check: Default server \"%1\" also needs the %2 core for TUN compatibility. Expected one of: %3.")
                                     .arg(elidedServerDisplayName(*activeServer, 64))
                                     .arg(coreTypeDisplayName(auxiliaryCoreType))
                                     .arg(coreDiscoveryService_->expectedCoreFilesText(candidates)));
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

    if (mainWindow_ != nullptr) {
        for (const QString& line : lines) {
            mainWindow_->appendLog(line);
        }
        mainWindow_->showTransientStatus(lines.constFirst(), result.success ? 4000 : 8000);
    }
}

void AppBootstrap::showOperationMessage(
    const QString& title,
    const OperationResult& result,
    QWidget* parent,
    bool showDialog)
{
    appendResult(result);
    if (!showDialog || result.message.trimmed().isEmpty() || mainWindow_ == nullptr) {
        return;
    }

    QWidget* messageParent = parent != nullptr ? parent : mainWindow_.get();
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
    if (proxySession_ == nullptr) {
        return;
    }

    proxySession_->start(makeProxySessionStartRequest(
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
    if (tunRuntimeService_ == nullptr) {
        return OperationResult::fail(QStringLiteral("TUN runtime service is unavailable."));
    }
    return tunRuntimeService_->removeStaleAdapterIfPresent();
}

void AppBootstrap::removeStaleSingBoxCache()
{
    // Keep the sing-box cache file across restarts so remote rule-set downloads
    // can be reused when the upstream proxy is unstable.
}

void AppBootstrap::cleanupOrphanCoreProcesses()
{
    if (coreProcessCleanupService_ == nullptr) {
        return;
    }

    const QStringList terminatedProcesses = coreProcessCleanupService_->cleanupOrphanCoreProcesses();
    if (!terminatedProcesses.isEmpty()) {
        appendResult(OperationResult::ok(
            QStringLiteral("Cleaned up orphan core processes: %1").arg(terminatedProcesses.join(QStringLiteral(", ")))));
        appendResult(removeStaleTunAdapterIfPresent());
    }
}

void AppBootstrap::cleanupCoreProcessesUsingConfiguredPorts()
{
    if (coreProcessCleanupService_ == nullptr) {
        return;
    }

    const QStringList terminatedProcesses =
        coreProcessCleanupService_->cleanupCoreProcessesUsingConfiguredPorts(
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
    const BackgroundTaskCoordinator::Kind activeKind = backgroundTasks_ == nullptr
        ? BackgroundTaskCoordinator::Kind::None
        : backgroundTasks_->active();

    if (speedTestService_ != nullptr && backgroundTasks_ != nullptr
        && activeKind == BackgroundTaskCoordinator::Kind::SpeedTest) {
        speedTestService_->cancel();
        speedTestResultsDirty_ = false;
        speedTestTaskToken_ = {};
    }

    if (activeKind == BackgroundTaskCoordinator::Kind::SubscriptionUpdate
        && mainWindow_ != nullptr) {
        mainWindow_->setSubscriptionUpdateRunning(false);
    }

    const QList<QPointer<QThread>> threads = backgroundThreads_;
    for (const QPointer<QThread>& threadGuard : threads) {
        QThread* thread = threadGuard.data();
        if (thread != nullptr) {
            thread->requestInterruption();
        }
    }

    if (backgroundTasks_ != nullptr) {
        backgroundTasks_->cancelActive();
    }
}

void AppBootstrap::restartCoreIfRunning(const QString& reason, bool showStartupOverlay)
{
    if (proxySession_ == nullptr || !isCoreRunning()) {
        setCurrentActivationPending_ = false;
        return;
    }

    if (!reason.isEmpty()) {
        appendResult(OperationResult::ok(reason));
    }
    proxySession_->restartIfRunning(
        makeProxySessionStartRequest(
            config_,
            existingCoreTypes_,
            false,
            showStartupOverlay,
            setCurrentActivationPending_));
}

bool AppBootstrap::saveSystemProxyMode(SystemProxyMode mode)
{
    if (serverService_ == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("System proxy mode cannot be changed before the configuration service is ready.")));
        return false;
    }

    const int previousValue = config_.sysProxyType;
    const bool previousMainProxyEnabled = config_.ui().mainProxyEnabled;
    config_.sysProxyType = toLegacySystemProxyModeValue(mode);
    config_.ui().mainProxyEnabled = mode == SystemProxyMode::ForcedChange;
    if (!serverService_->save(config_)) {
        config_.sysProxyType = previousValue;
        config_.ui().mainProxyEnabled = previousMainProxyEnabled;
        appendResult(OperationResult::fail(QStringLiteral("Failed to save the selected system proxy mode.")));
        syncStatusIndicators();
        return false;
    }

    return true;
}

void AppBootstrap::setSystemProxyMode(
    SystemProxyMode mode,
    bool skipTunCleanup,
    bool immediateStop,
    bool showStartupOverlay)
{
    if (proxySession_ == nullptr) {
        return;
    }

    const bool coreProcessRunning = proxySession_->isCoreRunning();
    const bool coreReady = proxySession_->isActive();
    const bool coreActivationPending = proxySession_->isActivationInProgress();
    if (mode == SystemProxyMode::ForcedChange
        && !coreReady
        && coreActivationPending) {
        if (showStartupOverlay) {
            proxySession_->requestChecklistOverlay();
        }
        appendResult(OperationResult::ok(QStringLiteral("Core start is already in progress.")));
        syncStatusIndicators();
        return;
    }

    if (!saveSystemProxyMode(mode)) {
        return;
    }

    if (mode == SystemProxyMode::ForcedChange) {
        if (coreReady) {
            const bool proxyApplied = updateSystemProxyMode(mode);
            if (proxyApplied) {
                proxySession_->adoptManagedSystemProxy(true);
            }
            appendResult(proxyApplied
                ? OperationResult::ok(QStringLiteral("System proxy mode set to %1.").arg(systemProxyModeDisplayName(mode)))
                : OperationResult::fail(QStringLiteral("Failed to apply system proxy mode %1.").arg(systemProxyModeDisplayName(mode))));
            syncStatusIndicators();
            return;
        }
        appendResult(OperationResult::ok(
            QStringLiteral("System proxy mode set to %1. It will be applied after the core starts.")
                .arg(systemProxyModeDisplayName(mode))));
        appendResult(OperationResult::ok(QStringLiteral("Starting the active core because Global system proxy mode was enabled.")));
        startManagedProxyCoreInternal(skipTunCleanup, showStartupOverlay);
        return;
    }

    const bool shouldStop = coreProcessRunning || coreActivationPending;
    const bool proxyApplied = updateSystemProxyMode(mode);
    if (proxyApplied && mode == SystemProxyMode::ForcedClear) {
        proxySession_->adoptManagedSystemProxy(false);
    }
    appendResult(proxyApplied
        ? OperationResult::ok(QStringLiteral("System proxy mode set to %1.").arg(systemProxyModeDisplayName(mode)))
        : OperationResult::fail(QStringLiteral("Failed to apply system proxy mode %1.").arg(systemProxyModeDisplayName(mode))));
    if (shouldStop) {
        appendResult(OperationResult::ok(QStringLiteral("Stopping the active core because system proxy was cleared.")));
        proxySession_->stop(immediateStop);
        return;
    }

    syncStatusIndicators();
}

void AppBootstrap::clearProxyStateAfterCoreStopped()
{
    if (config_.sysProxyType == toLegacySystemProxyModeValue(SystemProxyMode::ForcedClear)
        && !config_.ui().mainProxyEnabled) {
        return;
    }

    saveSystemProxyMode(SystemProxyMode::ForcedClear);
}

void AppBootstrap::applySystemProxyModeOnExit(bool windowsShutdown)
{
    if (exitProxyStateApplied_ || systemProxyService_ == nullptr) {
        return;
    }

    exitProxyStateApplied_ = true;
    const bool managedSystemProxyActive = proxySession_ != nullptr && proxySession_->isManagedProxyActive();
    if (windowsShutdown) {
        if (!managedSystemProxyActive) {
            return;
        }
        systemProxyService_->resetOnShutdown();
        proxySession_->adoptManagedSystemProxy(false);
        return;
    }

    if (!managedSystemProxyActive) {
        return;
    }

    const SystemProxyMode mode = resolveSystemProxyModeOnExit(
        normalizeSystemProxyMode(config_.sysProxyType),
        windowsShutdown);
    updateSystemProxyMode(mode);
    proxySession_->adoptManagedSystemProxy(expectedSystemProxyEnabled(mode));
}

void AppBootstrap::cleanupRuntimeForExit(bool windowsShutdown)
{
    applySystemProxyModeOnExit(windowsShutdown);
    if (proxySession_ != nullptr && (proxySession_->isCoreRunning() || proxySession_->isTransitioning())) {
        proxySession_->stop(true);
    }
    setRuntimePhase(RuntimePhase::Stopped);
}

void AppBootstrap::enableSystemProxy(bool showStartupOverlay)
{
    setSystemProxyMode(SystemProxyMode::ForcedChange, false, false, showStartupOverlay);
}

void AppBootstrap::retryCoreStartup(bool showStartupOverlay)
{
    enableSystemProxy(showStartupOverlay);
}

void AppBootstrap::disableSystemProxy()
{
    setSystemProxyMode(SystemProxyMode::ForcedClear);
}

void AppBootstrap::setTunEnabled(bool enabled)
{
    if (serverService_ == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("TUN mode cannot be changed before the configuration service is ready.")));
        syncStatusIndicators();
        return;
    }

    if (config_.tun().tunModeItem.enableTun == enabled) {
        syncStatusIndicators();
        return;
    }

    if (enabled && !resolveActiveServerSnapshot().has_value()) {
        appendResult(OperationResult::fail(QCoreApplication::translate("AppBootstrap", "Please select a server first.")));
        syncWindow();
        return;
    }

    const Config previousConfig = config_;
    const Config updatedConfig = prepareTunToggleConfigForSave(
        config_,
        enabled,
        isWindowsPlatform(),
        isProcessElevated());
    const bool shouldEnableProxyWithTun =
        enabled
        && !previousConfig.tun().tunModeItem.enableTun
        && !isCoreRunning();

    const TunSettingsSaveBehavior tunSaveBehavior = evaluateTunSettingsSaveBehavior(
        previousConfig,
        updatedConfig,
        isWindowsPlatform(),
        isProcessElevated(),
        isCoreRunning());
    const TunSettingsApplyDecision& tunDecision = tunSaveBehavior.applyDecision;

    if (tunSaveBehavior.shouldPromptForAdminRestart && !askRestartAsAdministratorForTun()) {
        syncWindow();
        return;
    }

    config_ = tunSaveBehavior.configToPersist;
    if (!serverService_->save(config_)) {
        config_ = previousConfig;
        appendResult(OperationResult::fail(QStringLiteral("Failed to save the TUN mode setting.")));
        syncWindow();
        return;
    }

    appendResult(OperationResult::ok(
        enabled
            ? QStringLiteral("TUN mode enabled.")
            : QStringLiteral("TUN mode disabled.")));

    if (tunSaveBehavior.shouldPromptForAdminRestart) {
        appendResult(OperationResult::fail(tunAdminRequiredSaveMessage()));
    }

    syncWindow();

    if (tunDecision.shouldRestartRunningCore) {
        restartCoreIfRunning(QCoreApplication::translate(
            "AppBootstrap",
            "Reloading core after applying TUN mode changes."),
            true);
    } else if (shouldEnableProxyWithTun && !tunSaveBehavior.shouldPromptForAdminRestart) {
        enableSystemProxy(true);
    } else {
        syncStatusIndicators();
    }

    if (tunSaveBehavior.shouldPromptForAdminRestart) {
        persistUiState();
        config_.ui().mainProxyEnabled = true;
        config_.sysProxyType = toLegacySystemProxyModeValue(SystemProxyMode::ForcedChange);
        serverService_->save(config_);
        if (!restartApplication(true)) {
            config_ = previousConfig;
            serverService_->save(config_);
            appendResult(OperationResult::fail(tunAdminRestartFailureMessage()));
            syncWindow();
        }
    }
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

    for (const QString& line : SubscriptionImportTextParser::nonEmptyLines(text)) {
        if (SubscriptionImportTextParser::isSubscriptionUrl(line)) {
            importSubscriptionUrlsFromTextAsync(text);
            return;
        }
    }

    importClipboardTextAsync(text);
}

void AppBootstrap::importClipboardTextAsync(const QString& text)
{
    if (subscriptionService_ == nullptr || serverService_ == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("Clipboard import service is unavailable.")));
        return;
    }

    const BackgroundTaskCoordinator::Token token =
        backgroundTasks_->tryBeginUserTask(BackgroundTaskCoordinator::Kind::SubscriptionUpdate);
    if (!token.isValid()) {
        return;
    }

    const QString startupMessage = QCoreApplication::translate(
        "AppBootstrap",
        "Importing clipboard content in the background...");
    if (mainWindow_ != nullptr) {
        mainWindow_->appendLog(startupMessage);
        mainWindow_->showTransientStatus(
            startupMessage,
            3000,
            MainWindow::TransientStatusPriority::Routine);
        mainWindow_->setSubscriptionUpdateRunning(true);
    }

    const QString configPath = resolveConfigPath();
    const QString customConfigDirectory = resolveCustomConfigDirectory();
    QObject* uiContext = mainWindow_ == nullptr
        ? static_cast<QObject*>(QCoreApplication::instance())
        : mainWindow_.get();
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this, text, configPath, customConfigDirectory, uiContext, token, lifetimeGuard]() {
        JsonConfigRepository repository(configPath);
        ServerService serverService(repository, customConfigDirectory);
        SubscriptionService subscriptionService(repository);
        QNetworkAccessManager networkAccessManager;
        SubscriptionUpdateService subscriptionUpdateService(repository, subscriptionService, networkAccessManager);
        Config workerConfig = repository.load();

        OperationResult result = subscriptionUpdateService.importFromText(workerConfig, text);
        if (!result.success) {
            const OperationResult customImportResult =
                importCustomConfigTextWithService(text, workerConfig, serverService);
            if (customImportResult.success) {
                result = customImportResult;
            }
        }

        invokeOnUiThread(uiContext, [this, result, token, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            if (backgroundTasks_ == nullptr
                || !backgroundTasks_->isCurrent(token)) {
                return;
            }
            backgroundTasks_->finish(token);
            if (mainWindow_ != nullptr) {
                mainWindow_->setSubscriptionUpdateRunning(false);
            }
            if (repository_ != nullptr && result.success) {
                config_ = repository_->load();
            }
            appendResult(result);
            if (result.success) {
                syncWindow();
            }
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::importSubscriptionUrlsFromTextAsync(const QString& text)
{
    if (subscriptionService_ == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("Subscription import service is unavailable.")));
        return;
    }

    const BackgroundTaskCoordinator::Token token =
        backgroundTasks_->tryBeginUserTask(BackgroundTaskCoordinator::Kind::SubscriptionUpdate);
    if (!token.isValid()) {
        return;
    }

    const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
    const QString activeSubscriptionId = activeServer.has_value() ? activeServer->subId.trimmed() : QString();
    if (mainWindow_ != nullptr) {
        const QString message = QCoreApplication::translate(
            "AppBootstrap",
            "Importing subscription URLs from the clipboard in the background...");
        mainWindow_->appendLog(message);
        mainWindow_->showTransientStatus(
            message,
            3000,
            MainWindow::TransientStatusPriority::Routine);
    }

    for (auto& server : config_.collection().servers) {
        server.testResult = QStringLiteral("...");
    }
    syncWindow();

    if (mainWindow_ != nullptr) {
        mainWindow_->setSubscriptionUpdateRunning(true);
    }
    const QString configPath = resolveConfigPath();
    QObject* uiContext = mainWindow_.get();
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this, text, configPath, activeSubscriptionId, uiContext, token, lifetimeGuard]() {
        JsonConfigRepository repository(configPath);
        SubscriptionService subscriptionService(repository);
        QNetworkAccessManager networkAccessManager;
        SubscriptionUpdateService subscriptionUpdateService(repository, subscriptionService, networkAccessManager);
        Config workerConfig = repository.load();
        QList<SubItem> subscriptions = subscriptionService.list(workerConfig);
        QStringList subscriptionIds;
        QStringList visitedUrls;
        QString lastSubscriptionId;

        for (const QString& line : SubscriptionImportTextParser::nonEmptyLines(text)) {
            if (!SubscriptionImportTextParser::isSubscriptionUrl(line)) {
                continue;
            }

            const QString normalizedUrl = line.trimmed();
            if (normalizedUrl.isEmpty()) {
                continue;
            }

            bool alreadyVisited = false;
            for (const QString& visitedUrl : visitedUrls) {
                if (visitedUrl.compare(normalizedUrl, Qt::CaseInsensitive) == 0) {
                    alreadyVisited = true;
                    break;
                }
            }
            if (alreadyVisited) {
                continue;
            }
            visitedUrls.append(normalizedUrl);

            QString subscriptionId;
            for (SubItem& item : subscriptions) {
                if (item.url.trimmed().compare(normalizedUrl, Qt::CaseInsensitive) != 0) {
                    continue;
                }

                if (item.id.trimmed().isEmpty()) {
                    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                }
                if (item.remarks.trimmed().isEmpty()) {
                    item.remarks = SubscriptionImportTextParser::extractHostOrIpFromUrl(normalizedUrl);
                }
                item.enabled = true;
                subscriptionId = item.id.trimmed();
                break;
            }

            if (subscriptionId.isEmpty()) {
                SubItem item;
                item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                item.remarks = SubscriptionImportTextParser::extractHostOrIpFromUrl(normalizedUrl);
                if (item.remarks.trimmed().isEmpty()) {
                    item.remarks = QStringLiteral("import sub");
                }
                item.url = normalizedUrl;
                item.enabled = true;
                subscriptions.append(item);
                subscriptionId = item.id;
            }

            if (!subscriptionId.isEmpty() && !subscriptionIds.contains(subscriptionId)) {
                subscriptionIds.append(subscriptionId);
                lastSubscriptionId = subscriptionId;
            }
        }

        if (subscriptionIds.isEmpty()) {
            invokeOnUiThread(uiContext, [this, token, lifetimeGuard]() {
                if (lifetimeGuard.expired()) {
                    return;
                }
                if (backgroundTasks_ == nullptr
                    || !backgroundTasks_->isCurrent(token)) {
                    return;
                }
                backgroundTasks_->finish(token);
                if (mainWindow_ != nullptr) {
                    mainWindow_->setSubscriptionUpdateRunning(false);
                }
                appendResult(OperationResult::fail(QStringLiteral("No supported share URL or subscription payload was detected.")));
            });
            return;
        }

        const OperationResult saveResult = subscriptionService.saveSubscriptions(workerConfig, subscriptions);
        OperationResult result = saveResult;
        if (saveResult.success) {
            workerConfig.ui().mainSelectedSubId = lastSubscriptionId;
            repository.save(workerConfig);
            const OperationResult updateResult = subscriptionUpdateService.updateByIds(workerConfig, subscriptionIds);

            QStringList lines;
            lines.append(QStringLiteral("Imported %1 subscription URL(s).").arg(subscriptionIds.size()));
            if (!updateResult.message.trimmed().isEmpty()) {
                lines.append(updateResult.message);
            }

            result = updateResult.success
                ? OperationResult::ok(lines.join(QStringLiteral("\n")))
                : OperationResult::fail(lines.join(QStringLiteral("\n")));
        }

        invokeOnUiThread(uiContext, [this, result, subscriptionIds, activeSubscriptionId, lastSubscriptionId, token, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            if (backgroundTasks_ == nullptr
                || !backgroundTasks_->isCurrent(token)) {
                return;
            }
            backgroundTasks_->finish(token);
            if (mainWindow_ != nullptr) {
                mainWindow_->setSubscriptionUpdateRunning(false);
            }
            if (repository_ != nullptr) {
                config_ = repository_->load();
            }
            appendResult(result);
            syncWindow();
            if (mainWindow_ != nullptr && !lastSubscriptionId.trimmed().isEmpty()) {
                mainWindow_->selectSubscriptionTab(lastSubscriptionId);
            }
            if (result.success
                && !activeSubscriptionId.isEmpty()
                && subscriptionIds.contains(activeSubscriptionId)) {
                restartCoreIfRunning(QStringLiteral("Reloading core after updating subscriptions."), true);
            }
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::updateAllSubscriptions()
{
    if (backgroundTasks_->isKindActive(BackgroundTaskCoordinator::Kind::SubscriptionUpdate)) {
        appendResult(OperationResult::fail(QStringLiteral("A subscription update is already running in the background.")));
        return;
    }

    updateSubscriptionsByIds(
        QStringList(),
        false,
        QCoreApplication::translate("AppBootstrap", "Updating subscriptions in the background..."));
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

    updateSelectedSubscriptions(rowIndexes);
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

    updateSubscriptionsByIds(
        QStringList{trimmedSubscriptionId},
        true,
        QCoreApplication::translate("AppBootstrap", "Updating the current subscription through the local proxy..."));
}

void AppBootstrap::hideSubscription(const QString& subscriptionId)
{
    if (subscriptionService_ == nullptr) {
        return;
    }

    appendResult(subscriptionService_->setSubscriptionEnabled(config_, subscriptionId, false));
    syncWindow();
}

void AppBootstrap::deleteSubscription(const QString& subscriptionId)
{
    if (subscriptionService_ == nullptr) {
        return;
    }

    const QString normalizedId = subscriptionId.trimmed();
    const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
    const QString activeSubscriptionId = activeServer.has_value() ? activeServer->subId.trimmed() : QString();
    const bool activeSubscriptionRemoved = !activeSubscriptionId.isEmpty() && activeSubscriptionId == normalizedId;

    const OperationResult result = subscriptionService_->removeSubscription(config_, normalizedId);
    appendResult(result);
    syncWindow();
    if (!result.success || !activeSubscriptionRemoved || !isCoreRunning()) {
        return;
    }

    if (!resolveActiveServerSnapshot().has_value()) {
        appendResult(OperationResult::ok(QStringLiteral("Stopping core because the active subscription was deleted.")));
        stopCore(false);
        return;
    }

    restartCoreIfRunning(QStringLiteral("Reloading core after deleting the active subscription."), true);
}

void AppBootstrap::handleRoutingSelectionResult(
    const OperationResult& result,
    bool previousAdvancedEnabled,
    int previousRoutingIndex)
{
    appendResult(result);
    syncWindow();

    const RoutingSelectionPlan plan = evaluateRoutingSelectionPlan(
        result.success,
        previousAdvancedEnabled,
        config_.collection().enableRoutingAdvanced,
        previousRoutingIndex,
        config_.collection().routingIndex,
        isCoreRunning());
    if (plan.shouldRestartRunningCore) {
        restartCoreIfRunning(QStringLiteral("Reloading core to apply routing changes."), true);
    }
}

void AppBootstrap::handleDefaultServerSelectionResult(const OperationResult& result, const QString& previousIndexId)
{
    appendResult(result);
    syncWindow();

    const DefaultServerSelectionPlan plan = evaluateDefaultServerSelectionPlan(
        result.success,
        previousIndexId,
        config_.currentIndexId,
        isCoreRunning());
    if (!result.success) {
        return;
    }

    if (plan.shouldClearCurrentServerWarning) {
        proxySession_->setServerWarning({});
    }
    if (plan.shouldMarkCurrentActivationPending) {
        setCurrentActivationPending_ = true;
    }
    if (plan.shouldRestartRunningCore) {
        restartCoreIfRunning(QStringLiteral("Reloading core after switching the default server."), true);
    }
    if (plan.shouldClearCurrentActivationPending) {
        setCurrentActivationPending_ = false;
    }
    if (plan.shouldEnableProxy) {
        enableSystemProxy(true);
    }
}

bool AppBootstrap::requestDefaultServerSwitchAfterCoreStop(const QString& indexId, bool enableTun)
{
    if (proxySession_ == nullptr) {
        return false;
    }

    const QString trimmedId = indexId.trimmed();
    if (trimmedId.isEmpty() || !proxySession_->isCoreRunning() || config_.currentIndexId == trimmedId) {
        return false;
    }

    setCurrentActivationPending_ = true;
    appendResult(OperationResult::ok(QStringLiteral("Stopping current core before switching the default server.")));
    proxySession_->switchServer(trimmedId, enableTun, true);
    return true;
}

void AppBootstrap::scheduleDefaultServerSwitchAfterCoreStopped(
    const QString& indexId,
    bool enableTun,
    bool showStartupOverlay)
{
    QPointer<QObject> mainWindowGuard(mainWindow_.get());
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QTimer::singleShot(
        0,
        mainWindowGuard.isNull() ? QCoreApplication::instance() : mainWindowGuard.data(),
        [this, indexId, enableTun, showStartupOverlay, lifetimeGuard]() {
            if (lifetimeGuard.expired() || shuttingDown_.load()) {
                return;
            }

            switchDefaultServerAfterCoreStopped(indexId, enableTun, showStartupOverlay);
        });
}

void AppBootstrap::switchDefaultServerAfterCoreStopped(
    const QString& indexId,
    bool enableTun,
    bool showStartupOverlay)
{
    if (indexId.isEmpty() || serverService_ == nullptr || shuttingDown_.load()) {
        setCurrentActivationPending_ = false;
        return;
    }

    const QString previousIndexId = config_.currentIndexId;
    const OperationResult result = serverService_->setDefaultServer(config_, indexId);
    appendResult(result);
    syncWindow();
    if (!result.success) {
        setCurrentActivationPending_ = false;
        return;
    }

    proxySession_->setServerWarning({});

    if (enableTun && !config_.tun().tunModeItem.enableTun) {
        setTunEnabled(true);
        return;
    }

    if (previousIndexId != config_.currentIndexId || enableTun) {
        appendResult(OperationResult::ok(QStringLiteral("Starting core after switching the default server.")));
        startManagedProxyCoreInternal(false, showStartupOverlay);
        return;
    }

    setCurrentActivationPending_ = false;
}

void AppBootstrap::setDefaultServerWithTun(const QString& indexId)
{
    if (serverService_ == nullptr) {
        return;
    }

    if (requestDefaultServerSwitchAfterCoreStop(indexId, true)) {
        return;
    }

    const QString previousIndexId = config_.currentIndexId;
    const OperationResult result = serverService_->setDefaultServer(config_, indexId);
    appendResult(result);
    syncWindow();
    if (!result.success) {
        return;
    }

    proxySession_->setServerWarning({});
    setCurrentActivationPending_ = true;

    if (config_.tun().tunModeItem.enableTun) {
        if (isCoreRunning() && previousIndexId != config_.currentIndexId) {
            restartCoreIfRunning(QStringLiteral("Reloading core after switching the default server."), true);
            return;
        }
        enableSystemProxy(true);
        return;
    }

    setTunEnabled(true);
}

void AppBootstrap::updateCore(int coreTypeValue)
{
    updateCore(coreTypeValue, false);
}

void AppBootstrap::updateCore(int coreTypeValue, bool startAfterSuccess)
{
    updateCore(coreTypeValue, startAfterSuccess, mainWindow_.get(), mainWindow_.get(), false, false, {}, {});
}

void AppBootstrap::updateCore(
    int coreTypeValue,
    bool startAfterSuccess,
    QObject* progressContext,
    QWidget* dialogParent,
    bool skipConfirmation,
    bool skipLocalVersionCheck,
    const std::function<void(const QString&)>& progressObserver,
    const std::function<void(const OperationResult&)>& completionObserver)
{
    if (proxySession_ == nullptr) {
        return;
    }

    if (mainWindow_ == nullptr) {
        return;
    }

    const auto notifyCompletion = [&completionObserver](const OperationResult& result) {
        if (completionObserver) {
            completionObserver(result);
        }
    };

    const BackgroundTaskCoordinator::Token token =
        backgroundTasks_->tryBeginUserTask(BackgroundTaskCoordinator::Kind::CoreUpdate);
    if (!token.isValid()) {
        return;
    }

    const CoreType coreType = resolveRuntimeCoreType(static_cast<CoreType>(coreTypeValue));
    const QString title = QCoreApplication::translate("AppBootstrap", "Install / Update %1 Core")
                              .arg(coreTypeDisplayName(coreType));
    const QString installDirectory = resolveCoreInstallDirectory(coreType);
    const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
    const bool shouldRestartRunningCore = isCoreRunning()
        && activeServer.has_value()
        && resolveRuntimeCoreType(activeServer->coreType) == coreType;
    const CoreUpdateConfig workerConfig{
        config_.checkPreReleaseUpdate,
        config_.ignoreGeoUpdateCore};
    QPointer<QObject> progressContextGuard(progressContext);
    QPointer<QWidget> dialogParentGuard(dialogParent);
    bool stoppedForInstall = false;

    QWidget* messageParent = dialogParentGuard.isNull() ? mainWindow_.get() : dialogParentGuard.data();
    const QString prompt = QCoreApplication::translate("CoreUpdateService", "Install / Update %1?\nThe running core will be stopped before installation if needed.")
                               .arg(coreTypeDisplayName(coreType));
    const bool confirmed = skipConfirmation || (messageParent != nullptr
        && DialogUtils::askYesNoQuestion(
               messageParent,
               title,
               prompt,
               QMessageBox::Yes)
            == QMessageBox::Yes);
    if (!confirmed) {
        if (backgroundTasks_->isCurrent(token)) {
            backgroundTasks_->finish(token);
        }
        const OperationResult result = OperationResult::ok(
            QCoreApplication::translate("AppBootstrap", "%1 update was canceled.")
                .arg(coreTypeDisplayName(coreType)));
        appendResult(result);
        notifyCompletion(result);
        return;
    }

    if (shouldRestartRunningCore) {
        const QString message = QCoreApplication::translate(
            "AppBootstrap",
            "Stopping the running core before installing the update.");
        appendResult(OperationResult::ok(message));
        pendingCoreUpdateType_ = coreType;
        pendingCoreUpdateStartAfterSuccess_ = startAfterSuccess;
        pendingCoreUpdateSkipLocalVersionCheck_ = skipLocalVersionCheck;
        pendingCoreUpdateProgressContext_ = progressContextGuard;
        pendingCoreUpdateDialogParent_ = dialogParentGuard;
        pendingCoreUpdateProgressObserver_ = progressObserver;
        pendingCoreUpdateCompletionObserver_ = completionObserver;
        pendingCoreUpdateTaskToken_ = token;
        proxySession_->stopForCoreUpdate();
        return;
    }

    const QString startupMessage = QCoreApplication::translate("AppBootstrap", "Starting %1 update...")
                                       .arg(coreTypeDisplayName(coreType));
    mainWindow_->appendLog(startupMessage);
    mainWindow_->showTransientStatus(
        startupMessage,
        3000,
        MainWindow::TransientStatusPriority::Routine);

    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this,
                                       coreType,
                                       workerConfig,
                                       installDirectory,
                                       title,
                                       stoppedForInstall,
                                       startAfterSuccess,
                                       skipLocalVersionCheck,
                                       progressContextGuard,
                                       dialogParentGuard,
                                       progressObserver,
                                       completionObserver,
                                       token,
                                       lifetimeGuard]() {
        runCoreUpdateTask(
            token,
            coreType,
            workerConfig,
            installDirectory,
            progressContextGuard,
            skipLocalVersionCheck,
            progressObserver,
            [this, title, stoppedForInstall, startAfterSuccess, dialogParentGuard, completionObserver, token, lifetimeGuard](const OperationResult& result) {
                if (lifetimeGuard.expired()) {
                    return;
                }
                if (backgroundTasks_ == nullptr
                    || !backgroundTasks_->isCurrent(token)) {
                    return;
                }
                finalizeCoreUpdate(
                    token,
                    title,
                    result,
                    stoppedForInstall,
                    startAfterSuccess,
                    dialogParentGuard,
                    completionObserver);
            });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::runCoreUpdateTask(
    BackgroundTaskCoordinator::Token token,
    CoreType coreType,
    CoreUpdateConfig workerConfig,
    QString installDirectory,
    QPointer<QObject> progressContextGuard,
    bool skipLocalVersionCheck,
    std::function<void(const QString&)> progressObserver,
    std::function<void(const OperationResult&)> completionObserver)
{
    CoreUpdateService coreUpdateService;
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;

    const auto reportProgress = [this, progressContextGuard, progressObserver, token, lifetimeGuard](const QString& message) {
        if (message.trimmed().isEmpty()) {
            return;
        }

        QObject* uiTarget = progressContextGuard.isNull() ? mainWindow_.get() : progressContextGuard.data();
        invokeOnUiThread(uiTarget, [this, message, progressObserver, token, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            if (backgroundTasks_ == nullptr
                || !backgroundTasks_->isCurrent(token)) {
                return;
            }
            if (mainWindow_ == nullptr) {
                return;
            }

            mainWindow_->appendLog(message);
            mainWindow_->showTransientStatus(
                message,
                0,
                MainWindow::TransientStatusPriority::Routine);
            if (progressObserver) {
                progressObserver(message);
            }
        });
    };

    CoreUpdateService::UpdateOptions updateOptions;
    updateOptions.progressHandler = reportProgress;
    updateOptions.cancelCheck = [this]() {
        QThread* currentThread = QThread::currentThread();
        return shuttingDown_.load()
            || (currentThread != nullptr && currentThread->isInterruptionRequested());
    };
    updateOptions.skipLocalVersionCheck = skipLocalVersionCheck;

    const OperationResult result = coreUpdateService.update(
        coreType,
        workerConfig,
        installDirectory,
        updateOptions);

    QObject* uiTarget = progressContextGuard.isNull() ? mainWindow_.get() : progressContextGuard.data();
    invokeOnUiThread(uiTarget, [this, completionObserver, result, token, lifetimeGuard]() {
        if (lifetimeGuard.expired()) {
            return;
        }
        if (backgroundTasks_ == nullptr
            || !backgroundTasks_->isCurrent(token)) {
            return;
        }
        if (completionObserver) {
            completionObserver(result);
        }
    });
}

void AppBootstrap::finalizeCoreUpdate(
    BackgroundTaskCoordinator::Token token,
    const QString& title,
    const OperationResult& result,
    bool stoppedForInstall,
    bool startAfterSuccess,
    QPointer<QWidget> dialogParentGuard,
    const std::function<void(const OperationResult&)>& completionObserver)
{
    if (backgroundTasks_ == nullptr
        || !backgroundTasks_->isCurrent(token)) {
        return;
    }

    backgroundTasks_->finish(token);
    if (pendingCoreUpdateTaskToken_.generation == token.generation) {
        pendingCoreUpdateTaskToken_ = {};
    }
    if (shuttingDown_.load()) {
        return;
    }

    if (mainWindow_ != nullptr && !result.message.trimmed().isEmpty()) {
        mainWindow_->showTransientStatus(result.message, 5000);
    }
    if (result.success) {
        refreshExistingCoreTypes();
    }
    if (completionObserver) {
        completionObserver(result);
    }

    if (stoppedForInstall) {
        if (result.success && result.requiresRestart && startAfterSuccess) {
            const QString message = QCoreApplication::translate(
                "AppBootstrap",
                "Restarting the updated core...");
            if (mainWindow_ != nullptr) {
                mainWindow_->appendLog(message);
                mainWindow_->showTransientStatus(
                    message,
                    0,
                    MainWindow::TransientStatusPriority::Routine);
            }
            enableSystemProxy(true);
        } else if (!result.success) {
            const QString message = QCoreApplication::translate(
                "AppBootstrap",
                "Update failed. Restoring the previous running core...");
            if (mainWindow_ != nullptr) {
                mainWindow_->appendLog(message);
                mainWindow_->showTransientStatus(message, 0);
            }
            enableSystemProxy(false);
        } else {
            clearProxyStateAfterCoreStopped();
            syncStatusIndicators();
        }
    }

    if (!stoppedForInstall && startAfterSuccess && result.success) {
        const QString message = QCoreApplication::translate(
            "AppBootstrap",
            "Starting the downloaded core...");
        if (mainWindow_ != nullptr) {
            mainWindow_->appendLog(message);
            mainWindow_->showTransientStatus(
                message,
                0,
                MainWindow::TransientStatusPriority::Routine);
        }
        enableSystemProxy(true);
    }

    if (mainWindow_ == nullptr || result.message.trimmed().isEmpty()) {
        return;
    }

    QWidget* messageParent = dialogParentGuard.isNull() ? mainWindow_.get() : dialogParentGuard.data();
    showOperationMessage(title, result, messageParent);
}

void AppBootstrap::continuePendingCoreUpdate()
{
    if (mainWindow_ == nullptr || !pendingCoreUpdateTaskToken_.isValid()) {
        return;
    }

    const CoreType coreType = pendingCoreUpdateType_;
    const bool startAfterSuccess = pendingCoreUpdateStartAfterSuccess_;
    const bool skipLocalVersionCheck = pendingCoreUpdateSkipLocalVersionCheck_;
    QPointer<QObject> progressContextGuard = pendingCoreUpdateProgressContext_;
    QPointer<QWidget> dialogParentGuard = pendingCoreUpdateDialogParent_;
    std::function<void(const QString&)> progressObserver = pendingCoreUpdateProgressObserver_;
    std::function<void(const OperationResult&)> completionObserver = pendingCoreUpdateCompletionObserver_;
    const BackgroundTaskCoordinator::Token token = pendingCoreUpdateTaskToken_;

    pendingCoreUpdateType_ = CoreType::Unknown;
    pendingCoreUpdateStartAfterSuccess_ = false;
    pendingCoreUpdateSkipLocalVersionCheck_ = false;
    pendingCoreUpdateProgressContext_.clear();
    pendingCoreUpdateDialogParent_.clear();
    pendingCoreUpdateProgressObserver_ = {};
    pendingCoreUpdateCompletionObserver_ = {};
    pendingCoreUpdateTaskToken_ = {};

    if (backgroundTasks_ == nullptr || !backgroundTasks_->isCurrent(token)) {
        return;
    }

    const QString title = QCoreApplication::translate("AppBootstrap", "Install / Update %1 Core")
                              .arg(coreTypeDisplayName(coreType));
    const QString installDirectory = resolveCoreInstallDirectory(coreType);
    const CoreUpdateConfig workerConfig{
        config_.checkPreReleaseUpdate,
        config_.ignoreGeoUpdateCore};
    const QString startupMessage = QCoreApplication::translate("AppBootstrap", "Starting %1 update...")
                                       .arg(coreTypeDisplayName(coreType));
    mainWindow_->appendLog(startupMessage);
    mainWindow_->showTransientStatus(
        startupMessage,
        3000,
        MainWindow::TransientStatusPriority::Routine);

    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this,
                                       coreType,
                                       workerConfig,
                                       installDirectory,
                                       title,
                                       startAfterSuccess,
                                       skipLocalVersionCheck,
                                       progressContextGuard,
                                       dialogParentGuard,
                                       progressObserver,
                                       completionObserver,
                                       token,
                                       lifetimeGuard]() {
        runCoreUpdateTask(
            token,
            coreType,
            workerConfig,
            installDirectory,
            progressContextGuard,
            skipLocalVersionCheck,
            progressObserver,
            [this, title, startAfterSuccess, dialogParentGuard, completionObserver, token, lifetimeGuard](const OperationResult& result) {
                if (lifetimeGuard.expired()) {
                    return;
                }
                if (backgroundTasks_ == nullptr
                    || !backgroundTasks_->isCurrent(token)) {
                    return;
                }
                finalizeCoreUpdate(
                    token,
                    title,
                    result,
                    true,
                    startAfterSuccess,
                    dialogParentGuard,
                    completionObserver);
            });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::updateGeoResources()
{
    if (geoResourceUpdateService_ == nullptr || mainWindow_ == nullptr) {
        return;
    }

    const BackgroundTaskCoordinator::Token token =
        backgroundTasks_->tryBeginUserTask(BackgroundTaskCoordinator::Kind::GeoUpdate);
    if (!token.isValid()) {
        return;
    }

    const QString targetDirectory = QFileInfo(resolveConfigPath()).dir().absolutePath();
    QObject* uiContext = mainWindow_.get();
    const QString title = QCoreApplication::translate("AppBootstrap", "Update Geo Files");
    const QString message = QCoreApplication::translate("AppBootstrap", "Updating Geo resources in the background...");
    mainWindow_->appendLog(message);
    mainWindow_->showTransientStatus(
        message,
        3000,
        MainWindow::TransientStatusPriority::Routine);

    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this, targetDirectory, title, uiContext, token, lifetimeGuard]() {
        GeoResourceUpdateService geoResourceUpdateService(targetDirectory);
        const QList<OperationResult> results{
            geoResourceUpdateService.update(QStringLiteral("geosite")),
            geoResourceUpdateService.update(QStringLiteral("geoip"))};

        bool hasSuccess = false;
        QStringList failureLines;
        QStringList successLines;
        for (const OperationResult& result : results) {
            if (result.success) {
                hasSuccess = true;
                if (!result.message.trimmed().isEmpty()) {
                    successLines.append(result.message);
                }
            } else if (!result.message.trimmed().isEmpty()) {
                failureLines.append(result.message);
            }
        }

        invokeOnUiThread(uiContext, [this, title, results, hasSuccess, failureLines, successLines, token, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            if (backgroundTasks_ == nullptr
                || !backgroundTasks_->isCurrent(token)) {
                return;
            }
            backgroundTasks_->finish(token);
            for (const OperationResult& result : results) {
                appendResult(result);
            }

            if (mainWindow_ != nullptr) {
                if (!failureLines.isEmpty()) {
                    DialogUtils::showWarning(mainWindow_.get(), title, failureLines.join(QChar('\n')));
                } else if (!successLines.isEmpty()) {
                    DialogUtils::showInformation(mainWindow_.get(), title, successLines.join(QChar('\n')));
                }
            }

            if (hasSuccess) {
                restartCoreIfRunning(QCoreApplication::translate(
                    "AppBootstrap",
                    "Reloading core after updating Geo resources."));
            }
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::updateSelectedSubscriptions(const QList<int>& rowIndexes)
{
    QStringList subscriptionIds;
    for (int rowIndex : rowIndexes) {
        if (rowIndex < 0 || rowIndex >= config_.collection().subscriptions.size()) {
            continue;
        }

        const QString subscriptionId = config_.collection().subscriptions.at(rowIndex).id.trimmed();
        if (!subscriptionId.isEmpty() && !subscriptionIds.contains(subscriptionId)) {
            subscriptionIds.append(subscriptionId);
        }
    }

    updateSubscriptionsByIds(
        subscriptionIds,
        false,
        QCoreApplication::translate("AppBootstrap", "Updating selected subscriptions in the background..."));
}

void AppBootstrap::updateSubscriptionsByIds(
    const QStringList& subscriptionIds,
    bool useProxy,
    const QString& startupMessage)
{
    const BackgroundTaskCoordinator::Token token =
        backgroundTasks_->tryBeginUserTask(BackgroundTaskCoordinator::Kind::SubscriptionUpdate);
    if (!token.isValid()) {
        return;
    }

    const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
    const QString activeSubscriptionId = activeServer.has_value() ? activeServer->subId.trimmed() : QString();
    if (mainWindow_ != nullptr) {
        mainWindow_->appendLog(startupMessage);
        mainWindow_->showTransientStatus(
            startupMessage,
            3000,
            MainWindow::TransientStatusPriority::Routine);
    }

    // Clear test results as loading indicator.
    for (auto& server : config_.collection().servers) {
        server.testResult = QStringLiteral("...");
    }
    syncWindow();

    if (mainWindow_ != nullptr) {
        mainWindow_->setSubscriptionUpdateRunning(true);
    }
    const QString configPath = resolveConfigPath();
    QObject* uiContext = mainWindow_.get();
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this, configPath, subscriptionIds, activeSubscriptionId, uiContext, useProxy, token, lifetimeGuard]() {
        JsonConfigRepository repository(configPath);
        SubscriptionService subscriptionService(repository);
        QNetworkAccessManager networkAccessManager;
        SubscriptionUpdateService subscriptionUpdateService(repository, subscriptionService, networkAccessManager);
        Config workerConfig = repository.load();
        const OperationResult result = subscriptionIds.isEmpty()
            ? subscriptionUpdateService.updateAll(workerConfig, useProxy)
            : subscriptionUpdateService.updateByIds(workerConfig, subscriptionIds, useProxy);

        invokeOnUiThread(uiContext, [this, result, subscriptionIds, activeSubscriptionId, token, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            if (backgroundTasks_ == nullptr
                || !backgroundTasks_->isCurrent(token)) {
                return;
            }
            backgroundTasks_->finish(token);
            if (mainWindow_ != nullptr) {
                mainWindow_->setSubscriptionUpdateRunning(false);
            }
            if (repository_ != nullptr) {
                config_ = repository_->load();
            }
            appendResult(result);
            syncWindow();
            if (result.success
                && !activeSubscriptionId.isEmpty()
                && (subscriptionIds.isEmpty() || subscriptionIds.contains(activeSubscriptionId))) {
                restartCoreIfRunning(QStringLiteral("Reloading core after updating subscriptions."), true);
            }
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::autoBackupCurrentConfig()
{
    if (configBackupService_ == nullptr) {
        return;
    }

    const OperationResult result = configBackupService_->backupCurrentConfig(config_);
    if (!result.success) {
        appendResult(result);
    }
}

void AppBootstrap::restoreConfigFromBackup()
{
    if (mainWindow_ == nullptr || configBackupService_ == nullptr) {
        return;
    }

    QString initialDirectory = configBackupService_->backupDirectoryPath();
    if (initialDirectory.trimmed().isEmpty() || !QDir(initialDirectory).exists()) {
        initialDirectory = QFileInfo(resolveConfigPath()).dir().absolutePath();
    }

    const QString backupPath = DialogUtils::getOpenFileName(
        mainWindow_.get(),
        QCoreApplication::translate("AppBootstrap", "Restore GUI Config"),
        initialDirectory,
        QCoreApplication::translate("AppBootstrap", "Config Files (*.json);;All Files (*.*)"));
    if (backupPath.trimmed().isEmpty()) {
        return;
    }

    JsonConfigRepository validationRepository(backupPath);
    validationRepository.load();
    const QString validationError = validationRepository.lastLoadError().trimmed();
    if (!validationError.isEmpty()) {
        appendResult(OperationResult::fail(validationError));
        DialogUtils::showWarning(
            mainWindow_.get(),
            QCoreApplication::translate("AppBootstrap", "Restore GUI Config"),
            validationError);
        return;
    }

    const bool wasCoreRunning = isCoreRunning();
    if (wasCoreRunning) {
        proxySession_->stop(true);
    }

    const OperationResult backupResult = configBackupService_->backupCurrentConfig(config_);
    if (!backupResult.success) {
        appendResult(backupResult);
        return;
    }

    const OperationResult restoreResult = configBackupService_->restoreFromPath(backupPath);
    if (!restoreResult.success) {
        appendResult(restoreResult);
        return;
    }

    uiStateRestored_ = false;
    if (!reloadConfig()) {
        return;
    }

    if (wasCoreRunning && resolveActiveServer() != nullptr) {
        enableSystemProxy(true);
    } else {
        clearProxyStateAfterCoreStopped();
        syncStatusIndicators();
    }

    appendResult(OperationResult::ok(
        QCoreApplication::translate("AppBootstrap", "Configuration restored from backup.")));
}

void AppBootstrap::checkAppUpdates(bool manual)
{
    if (mainWindow_ == nullptr) {
        return;
    }

    if (appUpdateCheckRunning_) {
        if (manual) {
            mainWindow_->showTransientStatus(
                QCoreApplication::translate("AppBootstrap", "Update check is already running."),
                3000,
                MainWindow::TransientStatusPriority::Routine);
        }
        return;
    }

    appUpdateCheckRunning_ = true;
    const QString title = QCoreApplication::translate("AppBootstrap", "Check for Updates");
    if (manual) {
        const QString message = QCoreApplication::translate("AppBootstrap", "Checking for SongBird updates...");
        mainWindow_->appendLog(message);
        mainWindow_->showTransientStatus(
            message,
            0,
            MainWindow::TransientStatusPriority::Routine);
    }

    const QString currentVersion = QCoreApplication::applicationVersion();
    const bool allowPrerelease = config_.checkPreReleaseUpdate;
    QPointer<QObject> uiContext(mainWindow_.get());
    QPointer<QWidget> dialogParent(mainWindow_.get());
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;

    QThread* thread = QThread::create([this, title, currentVersion, allowPrerelease, manual, uiContext, dialogParent, lifetimeGuard]() {
        AppUpdateService service;
        AppUpdateCheckResult updateResult;
        const OperationResult result = service.checkForUpdate(
            currentVersion,
            allowPrerelease,
            &updateResult);

        QObject* target = uiContext.isNull()
            ? static_cast<QObject*>(QCoreApplication::instance())
            : uiContext.data();
        invokeOnUiThread(target, [this, title, result, updateResult, manual, dialogParent, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }

            appUpdateCheckRunning_ = false;
            if (mainWindow_ == nullptr || shuttingDown_.load()) {
                return;
            }

            QWidget* messageParent = dialogParent.isNull() ? mainWindow_.get() : dialogParent.data();
            if (!result.success) {
                if (manual) {
                    showOperationMessage(title, result, messageParent);
                }
                return;
            }

            if (!updateResult.updateAvailable) {
                if (manual) {
                    showOperationMessage(title, result, messageParent);
                }
                return;
            }

            appendResult(result);
            mainWindow_->showTransientStatus(result.message, 10000);
            if (trayController_ != nullptr && trayController_->isAvailable()) {
                trayController_->showMessage(
                    QCoreApplication::translate("AppBootstrap", "Update Available"),
                    result.message,
                    false,
                    8000);
            }

            if (!manual) {
                return;
            }

            QString prompt = QCoreApplication::translate(
                "AppBootstrap",
                "%1\n\nCurrent version: %2\nLatest version: %3\n\nDownload the update in the background?")
                                 .arg(result.message)
                                 .arg(updateResult.currentVersion.trimmed().isEmpty()
                                     ? QCoreApplication::translate("AppBootstrap", "Unknown")
                                     : updateResult.currentVersion)
                                 .arg(updateResult.latestVersion);
            if (!updateResult.releaseName.trimmed().isEmpty()) {
                prompt.append(QStringLiteral("\n"));
                prompt.append(QCoreApplication::translate("AppBootstrap", "Release: %1")
                                  .arg(updateResult.releaseName.trimmed()));
            }
            if (!updateResult.assetName.trimmed().isEmpty()) {
                prompt.append(QStringLiteral("\n"));
                prompt.append(QCoreApplication::translate("AppBootstrap", "Package: %1")
                                  .arg(updateResult.assetName.trimmed()));
            }

            if (!updateResult.assetDownloadUrl.isValid()) {
                prompt.append(QStringLiteral("\n\n"));
                prompt.append(QCoreApplication::translate(
                    "AppBootstrap",
                    "No downloadable Windows executable was found. Open the release page instead?"));
                if (DialogUtils::askYesNoQuestion(
                        messageParent,
                        QCoreApplication::translate("AppBootstrap", "Update Available"),
                        prompt,
                        QMessageBox::Yes)
                    == QMessageBox::Yes) {
                    openExternalUrl(updateResult.releaseUrl.toString());
                }
                return;
            }

            if (DialogUtils::askYesNoQuestion(
                    messageParent,
                    QCoreApplication::translate("AppBootstrap", "Update Available"),
                    prompt,
                    QMessageBox::Yes)
                == QMessageBox::Yes) {
                downloadAppUpdate(updateResult, dialogParent);
            }
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::downloadAppUpdate(const AppUpdateCheckResult& updateResult, QPointer<QWidget> dialogParent)
{
    if (mainWindow_ == nullptr || backgroundTasks_ == nullptr) {
        return;
    }

    const BackgroundTaskCoordinator::Token token =
        backgroundTasks_->tryBeginUserTask(BackgroundTaskCoordinator::Kind::AppUpdate);
    if (!token.isValid()) {
        return;
    }

    const QString title = QCoreApplication::translate("AppBootstrap", "Download SongBird Update");
    const QString updateDirectory = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("updates"));
    const QString assetName = QFileInfo(updateResult.assetName).suffix().compare(QStringLiteral("exe"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("SongBird.new.exe")
        : updateResult.assetName;
    const QString startMessage = QCoreApplication::translate("AppBootstrap", "Downloading SongBird %1...")
                                     .arg(updateResult.latestVersion);
    mainWindow_->appendLog(startMessage);
    mainWindow_->showTransientStatus(startMessage, 0, MainWindow::TransientStatusPriority::Routine);

    QPointer<QObject> uiContext(mainWindow_.get());
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this, updateResult, updateDirectory, assetName, title, dialogParent, token, uiContext, lifetimeGuard]() {
        AppUpdateService service;
        QString savedPath;
        const OperationResult result = service.downloadUpdate(
            updateResult.assetDownloadUrl,
            assetName,
            updateDirectory,
            &savedPath);

        QObject* target = uiContext.isNull()
            ? static_cast<QObject*>(QCoreApplication::instance())
            : uiContext.data();
        invokeOnUiThread(target, [this, result, savedPath, title, dialogParent, token, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            if (backgroundTasks_ == nullptr || !backgroundTasks_->isCurrent(token)) {
                return;
            }

            backgroundTasks_->finish(token);
            if (mainWindow_ == nullptr || shuttingDown_.load()) {
                return;
            }

            appendResult(result);
            QWidget* messageParent = dialogParent.isNull() ? mainWindow_.get() : dialogParent.data();
            if (!result.success) {
                showOperationMessage(title, result, messageParent);
                return;
            }

            if (!savedPath.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
                const QString message = QCoreApplication::translate(
                    "AppBootstrap",
                    "%1\n\nRestart SongBird and use the downloaded package to update to the new version.")
                        .arg(savedPath.trimmed().isEmpty()
                            ? result.message
                            : QCoreApplication::translate("AppBootstrap", "Downloaded package: %1")
                                  .arg(QDir::toNativeSeparators(savedPath)));
                DialogUtils::showInformation(
                    messageParent,
                    QCoreApplication::translate("AppBootstrap", "Update Downloaded"),
                    message);
                return;
            }

            promptRestartForDownloadedAppUpdate(savedPath, messageParent);
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::openSettingsDialog(int initialTab)
{
    if (mainWindow_ == nullptr || serverService_ == nullptr) {
        return;
    }

    refreshExistingCoreTypes();
    SettingsDialogSession session(
        mainWindow_.get(),
        [this](QThread* thread) { trackBackgroundThread(thread); },
        [this](CoreType coreType) { return detectCoreVersion(coreType); },
        [this](CoreType requestedCoreType, QPointer<SettingsDialog> dialogGuard) {
            const auto updateStarted = std::make_shared<bool>(false);
            updateCore(
                static_cast<int>(requestedCoreType),
                false,
                dialogGuard.data(),
                dialogGuard.data(),
                false,
                false,
                [dialogGuard, requestedCoreType, updateStarted](const QString& message) {
                    if (!dialogGuard.isNull()) {
                        if (!*updateStarted) {
                            dialogGuard->beginCoreUpdate(requestedCoreType);
                            *updateStarted = true;
                        }
                        dialogGuard->setCoreUpdateProgress(requestedCoreType, message);
                    }
                },
                [this, dialogGuard, requestedCoreType](const OperationResult& result) {
                    if (dialogGuard.isNull()) {
                        return;
                    }

                    if (result.success) {
                        dialogGuard->setExistingCoreTypes(existingCoreTypes_);
                        dialogGuard->setCoreVersion(requestedCoreType, detectCoreVersion(requestedCoreType));
                    }
                    dialogGuard->finishCoreUpdate(requestedCoreType, result.success, result.message);
                });
        });

    const SettingsDialogSession::Result sessionResult = session.exec(
        config_,
        initialTab,
        existingCoreTypes_,
        lifetimeGuard_);
    if (sessionResult.outcome == SettingsDialogSession::Outcome::Cancelled) {
        return;
    }

    if (sessionResult.outcome == SettingsDialogSession::Outcome::RestoreBackup) {
        restoreConfigFromBackup();
        return;
    }

    if (sessionResult.outcome == SettingsDialogSession::Outcome::UpdateSubscriptions) {
        const OperationResult saveResult =
            subscriptionService_->saveSubscriptions(config_, sessionResult.config.collection().subscriptions);
        appendResult(saveResult);
        if (saveResult.success) {
            syncWindow();
            updateSelectedSubscriptions(sessionResult.selectedSubscriptionRows);
        }
        return;
    }

    applySettingsDialogResult(sessionResult.config);
}

void AppBootstrap::applySettingsDialogResult(const Config& updatedConfig)
{
    const Config previousConfig = config_;

    QSet<QString> previousSubIds;
    for (const SubItem& item : previousConfig.collection().subscriptions) {
        const QString id = item.id.trimmed();
        if (!id.isEmpty()) {
            previousSubIds.insert(id);
        }
    }

    const SettingsDialogApplyPlan plan = evaluateSettingsDialogApplyPlan(
        previousConfig,
        updatedConfig,
        isWindowsPlatform(),
        isProcessElevated(),
        isCoreRunning());

    if (!plan.dirty) {
        syncWindow();
        return;
    }

    config_ = plan.tunSaveBehavior.configToPersist;
    SubscriptionService::normalizeSubscriptionIds(config_.collection().subscriptions);

    QStringList newSubIds;
    for (const SubItem& item : config_.collection().subscriptions) {
        if (item.enabled) {
            const QString id = item.id.trimmed();
            if (!id.isEmpty() && !previousSubIds.contains(id)) {
                newSubIds.append(id);
            }
        }
    }
    if (!serverService_->save(config_)) {
        config_ = previousConfig;
        appendResult(OperationResult::fail(
            QCoreApplication::translate("AppBootstrap", "Failed to save settings.")));
        syncWindow();
        return;
    }

    appendResult(OperationResult::ok(QCoreApplication::translate("AppBootstrap", "Settings saved.")));
    if (plan.tunSaveBehavior.shouldPromptForAdminRestart) {
        appendResult(OperationResult::fail(tunAdminRequiredSaveMessage()));
    }
    syncWindow();
    if (mainWindow_ != nullptr && !newSubIds.isEmpty()) {
        mainWindow_->selectSubscriptionTab(newSubIds.constLast());
    }

    if (!newSubIds.isEmpty()) {
        updateSubscriptionsByIds(
            newSubIds,
            false,
            QCoreApplication::translate("AppBootstrap", "Updating new subscriptions in the background..."));
    }

    if (plan.autoRunChanged && autoRunService_ != nullptr) {
        const bool success = autoRunService_->setEnabled(updatedConfig.ui().autoRunEnabled);
        if (!success) {
            appendResult(OperationResult::fail(QStringLiteral("Failed to update auto run setting.")));
        }
    }

    if (plan.systemProxySettingsChanged
        && systemProxyService_ != nullptr
        && isCoreReady()) {
        const bool proxyUpdated = updateSystemProxyMode(normalizeSystemProxyMode(config_.sysProxyType));
        if (!proxyUpdated) {
            appendResult(OperationResult::fail(
                QCoreApplication::translate("AppBootstrap", "Failed to reapply system proxy settings.")));
        } else {
            proxySession_->adoptManagedSystemProxy(
                expectedSystemProxyEnabled(normalizeSystemProxyMode(config_.sysProxyType)));
        }
    }

    if (plan.runtimeSettingsChanged) {
        restartCoreIfRunning(QCoreApplication::translate(
            "AppBootstrap",
            "Reloading core after applying settings changes."),
            true);
    } else {
        syncStatusIndicators();
    }

    const bool adminRestartInitiated =
        plan.tunSaveBehavior.shouldPromptForAdminRestart && promptRestartAsAdministratorForTun();

    if (shouldPromptForLanguageRestartAfterSettingsSave(plan.languageChanged, adminRestartInitiated)) {
        promptRestartForLanguageChange();
    }
}

void AppBootstrap::openAboutDialog()
{
    if (mainWindow_ == nullptr) {
        return;
    }

    AboutDialog dialog(mainWindow_.get());
    dialog.setRepoUrl(songbirdProjectPageUrl());
    dialog.setVersion(QCoreApplication::applicationVersion());
    dialog.setReleaseDate(QString::fromLatin1(AppReleaseDate));
    dialog.setConfigPath(QDir::toNativeSeparators(resolveConfigPath()));
    dialog.setProxyActive(isCoreReady());
    dialog.exec();
}

void AppBootstrap::openUwpLoopbackDialog()
{
    if (mainWindow_ == nullptr) {
        return;
    }

    if (uwpLoopbackDialog_.isNull()) {
        uwpLoopbackDialog_ = new UwpLoopbackDialog(mainWindow_.get());
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
    if (mainWindow_ == nullptr || !isWindowsPlatform() || isProcessElevated()) {
        return false;
    }

    return DialogUtils::askYesNoQuestion(
            mainWindow_.get(),
            tunAdminRestartPromptTitle(),
            tunAdminRestartPromptMessage(),
            QMessageBox::Yes)
        == QMessageBox::Yes;
}

bool AppBootstrap::promptRestartAsAdministratorForTun()
{
    if (!askRestartAsAdministratorForTun()) {
        return false;
    }

    persistUiState();
    if (!restartApplication(true)) {
        appendResult(OperationResult::fail(tunAdminRestartFailureMessage()));
        return false;
    }

    return true;
}

void AppBootstrap::promptRestartForLanguageChange()
{
    if (mainWindow_ == nullptr) {
        return;
    }

    const auto title = QCoreApplication::translate("AppBootstrap", "Restart Required");
    const auto message =
        QCoreApplication::translate("AppBootstrap", "Language changes take effect after restart. Restart now?");
    if (DialogUtils::askYesNoQuestion(
            mainWindow_.get(),
            title,
            message,
            QMessageBox::Yes)
        != QMessageBox::Yes) {
        return;
    }

    persistUiState();
    const bool requiresAdminRestart =
        isWindowsPlatform() && !isProcessElevated() && config_.tun().tunModeItem.enableTun;
    if (!restartApplication(requiresAdminRestart)) {
        appendResult(OperationResult::fail(
            QCoreApplication::translate("AppBootstrap", "Failed to restart the application.")));
    }
}

bool AppBootstrap::promptRestartForDownloadedAppUpdate(const QString& newExecutablePath, QWidget* parent)
{
    if (newExecutablePath.trimmed().isEmpty()) {
        return false;
    }

    const QString message = QCoreApplication::translate(
        "AppBootstrap",
        "Downloaded update package: %1\n\nRestart SongBird now and apply the update automatically?")
            .arg(QDir::toNativeSeparators(newExecutablePath));
    if (DialogUtils::askYesNoQuestion(
            parent,
            QCoreApplication::translate("AppBootstrap", "Update Downloaded"),
            message,
            QMessageBox::Yes)
        != QMessageBox::Yes) {
        return false;
    }

    if (appUpdateInstallService_ == nullptr || !appUpdateInstallService_->launchUpdateScript(newExecutablePath)) {
        appendResult(OperationResult::fail(
            QCoreApplication::translate("AppBootstrap", "Failed to start the update script.")));
        DialogUtils::showWarning(
            parent,
            QCoreApplication::translate("AppBootstrap", "Update Failed"),
            QCoreApplication::translate("AppBootstrap", "Failed to start the update script."));
        return false;
    }

    shutdownUiStatePersisted_ = true;
    cleanupRuntimeForExit(false);
    SingleInstanceBootstrap::releaseCurrentInstance();
    if (mainWindow_ != nullptr) {
        mainWindow_->setAllowClose(true);
    }
    QCoreApplication::quit();
    return true;
}

bool AppBootstrap::restartApplication(bool requireAdministrator)
{
    const QStringList arguments = startupRelaunchArgumentsForRunningInstance(
        QCoreApplication::arguments(),
        requireAdministrator,
        QCoreApplication::applicationPid());

    SingleInstanceBootstrap::releaseCurrentInstance();

    bool restarted = false;
    if (requireAdministrator) {
        restarted = restartProcessAsAdministrator(QCoreApplication::applicationFilePath(), arguments);
    } else {
        restarted = QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments);
    }

    if (!restarted) {
        SingleInstanceBootstrap::reacquireCurrentInstance();
        return false;
    }

    shutdownUiStatePersisted_ = true;
    cleanupRuntimeForExit(false);
    if (mainWindow_ != nullptr) {
        mainWindow_->setAllowClose(true);
    }
    QCoreApplication::quit();
    return true;
}

void AppBootstrap::startSpeedTest(const QStringList& indexIds)
{
    if (speedTestService_ == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("Speed test service is unavailable.")));
        return;
    }

    backgroundTasks_->resetSpeedTestProgress();
    BackgroundTaskCoordinator::Token token = backgroundTasks_->tryBeginUserTask(BackgroundTaskCoordinator::Kind::SpeedTest);
    if (!token.isValid()) {
        return;
    }
    speedTestTaskToken_ = token;

    QStringList uniqueIds;
    for (const QString& indexId : indexIds) {
        const QString trimmed = indexId.trimmed();
        if (!trimmed.isEmpty() && !uniqueIds.contains(trimmed)) {
            uniqueIds.append(trimmed);
        }
    }

    QList<SpeedTestRequestItem> items;
    items.reserve(uniqueIds.size());
    for (const QString& indexId : uniqueIds) {
        const VmessItem* server = findServerById(indexId);
        if (server == nullptr) {
            continue;
        }

        const CoreType launchCore = resolveLaunchCoreType(*server);
        const VmessItem runtimeServer = runtimeServerForLaunchCore(*server, launchCore);

        items.append(SpeedTestRequestItem{
            *server,
            runtimeServer,
            resolveCoreInfo(runtimeServer)});
    }

    const OperationResult startResult = speedTestService_->start(config_, items);
    if (!startResult.success) {
        appendResult(startResult);
    }

    if (!startResult.success) {
        backgroundTasks_->resetSpeedTestProgress();
        backgroundTasks_->finish(speedTestTaskToken_);
        speedTestTaskToken_ = {};
    }

    if (startResult.success) {
        backgroundTasks_->setSpeedTestTotalCount(items.size());
        backgroundTasks_->syncState();
        static const QString pending = QStringLiteral("...");
        QStringList pendingIds;
        for (const auto& item : items) {
            auto it = std::find_if(config_.collection().servers.begin(), config_.collection().servers.end(),
                [&item](const VmessItem& s) { return s.indexId == item.server.indexId; });
            if (it != config_.collection().servers.end()) {
                it->testResult = pending;
                pendingIds.append(it->indexId);
            }
        }
        if (mainWindow_ != nullptr) {
            mainWindow_->updateServerTestResults(pendingIds, pending);
        }
        if (trayController_ != nullptr) {
            trayController_->setServers(
                config_.collection().servers,
                config_.currentIndexId);
        }
    }
}

void AppBootstrap::trackBackgroundThread(QThread* thread)
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

    backgroundThreads_.append(thread);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
}

void AppBootstrap::waitForBackgroundThreads()
{
    // Workers cooperatively check QThread::isInterruptionRequested() at their
    // iteration boundaries (subscription update, geo update) or via service-level
    // cancel flags. Keep waiting during destruction: these workers capture
    // AppBootstrap members, so continuing teardown while one is still running
    // would leave a use-after-free behind.
    constexpr unsigned long kShutdownWaitMs = 15000;
    const QList<QPointer<QThread>> threads = backgroundThreads_;
    for (const QPointer<QThread>& threadGuard : threads) {
        QThread* thread = threadGuard.data();
        if (thread == nullptr) {
            continue;
        }

        thread->requestInterruption();
        while (!thread->wait(kShutdownWaitMs)) {
            qWarning("AppBootstrap: background thread did not honor interruption within %lums; still waiting",
                kShutdownWaitMs);
            thread->requestInterruption();
        }
    }
    backgroundThreads_.clear();
}

bool AppBootstrap::isCoreRunning() const
{
    return proxySession_ != nullptr && proxySession_->isCoreRunning();
}

bool AppBootstrap::isCoreReady() const
{
    return proxySession_ != nullptr && proxySession_->isActive();
}

QString AppBootstrap::resolveConfigPath() const
{
    return configPath_;
}

const VmessItem* AppBootstrap::resolveActiveServer() const
{
    const QString currentIndexId = config_.currentIndexId.trimmed();
    if (currentIndexId.isEmpty()) {
        return nullptr;
    }

    for (const VmessItem& item : config_.collection().servers) {
        if (item.indexId == currentIndexId) {
            return &item;
        }
    }

    return nullptr;
}

std::optional<VmessItem> AppBootstrap::resolveActiveServerSnapshot() const
{
    const VmessItem* server = resolveActiveServer();
    if (server == nullptr) {
        return std::nullopt;
    }

    return *server;
}

const VmessItem* AppBootstrap::findServerById(const QString& indexId) const
{
    if (indexId.trimmed().isEmpty()) {
        return nullptr;
    }

    for (const VmessItem& item : config_.collection().servers) {
        if (item.indexId == indexId) {
            return &item;
        }
    }

    return nullptr;
}

std::optional<VmessItem> AppBootstrap::findServerSnapshotById(const QString& indexId) const
{
    const VmessItem* server = findServerById(indexId);
    if (server == nullptr) {
        return std::nullopt;
    }

    return *server;
}

QString AppBootstrap::resolveCustomConfigDirectory() const
{
    return QFileInfo(resolveConfigPath()).dir().filePath(QStringLiteral("guiConfigs"));
}

QString AppBootstrap::resolveCustomConfigPath(const QString& address) const
{
    const QString trimmed = address.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (QFileInfo::exists(trimmed)) {
        return QFileInfo(trimmed).absoluteFilePath();
    }

    const QString managedPath = QDir(resolveCustomConfigDirectory()).filePath(trimmed);
    if (QFileInfo::exists(managedPath)) {
        return QFileInfo(managedPath).absoluteFilePath();
    }

    return trimmed;
}

QString AppBootstrap::resolveRuntimeConfigPath(const VmessItem& server) const
{
    QString runtimeDirectory = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("runtime"));

    if (!QDir().mkpath(runtimeDirectory)) {
        return {};
    }

    QString extension = QStringLiteral("json");
    if (server.configType == ConfigType::Custom) {
        const QString sourcePath = resolveCustomConfigPath(server.address);
        const QString sourceExtension = QFileInfo(sourcePath).suffix().trimmed();
        if (!sourceExtension.isEmpty()) {
            extension = sourceExtension;
        }
    }
    const CoreType runtimeCore = resolveEffectiveCoreType(server);
    return QDir(runtimeDirectory).filePath(QStringLiteral("config.generated.%1").arg(extension));
}

QString AppBootstrap::buildRoutingSummaryText() const
{
    if (!config_.collection().enableRoutingAdvanced || config_.collection().routingItems.isEmpty()) {
        return QCoreApplication::translate("AppBootstrap", "Basic");
    }

    if (config_.collection().routingIndex >= 0
        && config_.collection().routingIndex < config_.collection().routingItems.size()) {
        const QString remarks = config_.collection().routingItems.at(config_.collection().routingIndex).remarks.trimmed();
        if (!remarks.isEmpty()) {
            return QCoreApplication::translate("AppBootstrap", "%1 (Advanced)").arg(remarks);
        }

        return QCoreApplication::translate("AppBootstrap", "Routing %1 (Advanced)")
            .arg(config_.collection().routingIndex + 1);
    }

    return QCoreApplication::translate("AppBootstrap", "Advanced");
}

QString AppBootstrap::buildListenSummaryText() const
{
    if (config_.localPort <= 0) {
        return QCoreApplication::translate("AppBootstrap", "Unavailable");
    }

    return QCoreApplication::translate("AppBootstrap", "SOCKS %1 | HTTP %2")
        .arg(config_.localPort)
        .arg(config_.localPort + 1);
}

QString AppBootstrap::buildSystemProxyExceptions() const
{
    QStringList entries = splitProxyExceptions(
        config_.defaults().defIeProxyExceptions.trimmed().isEmpty()
            ? QString::fromUtf8(DefaultIeProxyExceptions)
            : config_.defaults().defIeProxyExceptions.trimmed());

    for (const QString& item : collectRouteDerivedProxyExceptions(config_)) {
        appendUniqueProxyException(entries, item);
    }

    return entries.join(QStringLiteral(";"));
}

bool AppBootstrap::updateSystemProxyMode(SystemProxyMode mode) const
{
    return systemProxyService_ == nullptr
        || systemProxyService_->update(
            mode,
            config_.localPort + 1,
            config_.localPort,
            buildSystemProxyExceptions(),
            config_.systemProxyAdvancedProtocol);
}

CoreType AppBootstrap::resolveEffectiveCoreType(const VmessItem& server) const
{
    return resolveSelectedCoreType(config_, server, existingCoreTypes_);
}

CoreType AppBootstrap::resolveLaunchCoreType(const VmessItem& server) const
{
    return resolveEffectiveCoreType(server);
}

CoreInfo AppBootstrap::resolveCoreInfo(const VmessItem& server) const
{
    CoreInfo info;
    const CoreType launchCore = resolveLaunchCoreType(server);
    const ICoreBackend* backend = coreBackend(launchCore);
    if (backend == nullptr) {
        return {};
    }

    info.type = backend->type();
    info.arguments = backend->launchArguments(info.configPlaceholder);
    info.appendConfigArgument = backend->appendConfigArgument();
    const QStringList candidates = resolveCoreCandidates(launchCore);

    const QString program = locateFirstExistingFile(candidates);

    if (program.isEmpty()) {
        return {};
    }

    info.program = program;
    info.workingDirectory = QFileInfo(program).absolutePath();

    return info;
}

QStringList AppBootstrap::resolveCoreCandidates(CoreType coreType) const
{
    return coreDiscoveryService_ != nullptr
        ? coreDiscoveryService_->resolveCoreCandidates(coreType, resolveConfigPath())
        : QStringList{};
}

QString AppBootstrap::locateFirstExistingFile(const QStringList& candidates) const
{
    return coreDiscoveryService_ != nullptr
        ? coreDiscoveryService_->locateFirstExistingFile(candidates)
        : QString{};
}

void AppBootstrap::refreshExistingCoreTypes()
{
    existingCoreTypes_ = detectExistingCoreTypes();
}

QList<CoreType> AppBootstrap::detectExistingCoreTypes() const
{
    QList<CoreType> existingCoreTypes;
    for (const CoreType coreType : availableCoreTypes()) {
        if (!locateFirstExistingFile(resolveCoreCandidates(coreType)).isEmpty()) {
            existingCoreTypes.append(coreType);
        }
    }
    return existingCoreTypes;
}

QString AppBootstrap::detectCoreVersion(CoreType coreType) const
{
    const QString program = locateFirstExistingFile(resolveCoreCandidates(coreType));
    return coreDiscoveryService_ != nullptr
        ? coreDiscoveryService_->detectCoreVersion(coreType, program)
        : QString{};
}

QString AppBootstrap::resolveCoreInstallDirectory(CoreType coreType) const
{
    const QString existingCorePath = locateFirstExistingFile(resolveCoreCandidates(coreType));
    if (!existingCorePath.trimmed().isEmpty()) {
        return QFileInfo(existingCorePath).absolutePath();
    }

    const QString configDirectory = QFileInfo(resolveConfigPath()).dir().absolutePath();
    if (!configDirectory.trimmed().isEmpty()) {
        return configDirectory;
    }

    return QCoreApplication::applicationDirPath();
}

