#include "auto/SongBirdAutoCoordinator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QMap>
#include <QPair>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QUuid>

#include <algorithm>
#include <utility>

#include "auto/AutoCountrySelection.h"
#include "auto/AutoCountrySupport.h"
#include "auto/AutoCountryInference.h"
#include "auto/AutoNodeEvaluationService.h"
#include "auto/AutoRuntimeDefaults.h"
#include "backends/singbox/SingBoxConfigFragments.h"
#include "common/AppPlatform.h"
#include "common/ServerDisplayName.h"
#include "common/SystemProxyMode.h"
#include "domain/models/RoutingProfiles.h"
#include "runtime/core/CoreCatalog.h"
#include "runtime/ProtocolCoreCompat.h"
#include "services/CoreUpdateService.h"
#include "services/SpeedTestRuntimeRunner.h"
#include "services/SubscriptionUpdateService.h"

namespace {

constexpr int kPeriodicRefreshIntervalMs = 30 * 60 * 1000;
constexpr int kHealthCheckIntervalMs = 60 * 1000;
constexpr int kSubscriptionUpdateCooldownMs = 10 * 60 * 1000;
constexpr int kCountryTestCooldownMs = 5 * 60 * 1000;
constexpr int kLowCountryRefreshCooldownMs = 5 * 60 * 1000;
constexpr int kLowCountryNodeThreshold = 1;

const QString kDefaultIeProxyExceptions = QStringLiteral(
    "localhost;127.*;10.*;172.16.*;172.17.*;172.18.*;172.19.*;172.20.*;172.21.*;172.22.*;"
    "172.23.*;172.24.*;172.25.*;172.26.*;172.27.*;172.28.*;172.29.*;172.30.*;172.31.*;192.168.*");
const QString kAutoStrategyFirstAvailable = QStringLiteral("firstAvailable");
const QString kAutoStrategyLowestLatency = QStringLiteral("lowestLatency");

QString normalizeSubscriptionUrl(QString value)
{
    value = value.trimmed();
    if (value.startsWith(QChar('#'))) {
        return {};
    }
    return value;
}

QString normalizeAutoSelectionStrategy(QString value)
{
    value = value.trimmed();
    return value.compare(kAutoStrategyFirstAvailable, Qt::CaseInsensitive) == 0
        ? kAutoStrategyFirstAvailable
        : kAutoStrategyLowestLatency;
}

QString evaluationStateText(const AutoNodeEvaluation& evaluation)
{
    if (evaluation.available) {
        return QStringLiteral("%1 %2 ms").arg(evaluation.countryDisplay).arg(evaluation.latencyMs);
    }
    return evaluation.error.trimmed().isEmpty() ? QStringLiteral("Failed") : evaluation.error.trimmed();
}

QString firstCountryWithNodesOrFirst(const QList<AutoCountrySummary>& countries)
{
    const auto withNodes = std::find_if(countries.cbegin(), countries.cend(), [](const AutoCountrySummary& country) {
        return country.availableCount > 0;
    });
    return withNodes == countries.cend()
        ? (countries.isEmpty() ? QString() : countries.constFirst().countryCode)
        : withNodes->countryCode;
}

const VmessItem* findServerInConfig(const Config& config, const QString& indexId)
{
    if (indexId.trimmed().isEmpty()) {
        return nullptr;
    }
    for (const VmessItem& server : config.collection().servers) {
        if (server.indexId == indexId) {
            return &server;
        }
    }
    return nullptr;
}

void preserveActiveServerForSubscriptionUpdate(Config& config, const QString& activeServerId)
{
    for (VmessItem& server : config.collection().servers) {
        if (server.indexId != activeServerId || server.subId.trimmed().isEmpty()) {
            continue;
        }
        server.subId.clear();
        if (!server.remarks.trimmed().startsWith(QStringLiteral("Active copy |"))) {
            server.remarks = QStringLiteral("Active copy | %1")
                .arg(server.remarks.trimmed().isEmpty() ? server.address.trimmed() : server.remarks.trimmed());
        }
        return;
    }
}

bool reconcilePreservedActiveServer(Config& config, const QString& activeServerId)
{
    const VmessItem* activeServer = findServerInConfig(config, activeServerId);
    if (activeServer == nullptr || !activeServer->subId.trimmed().isEmpty()) {
        return false;
    }

    const QString activeReuseKey = SubscriptionService::serverReuseKey(*activeServer);
    bool hasDuplicateSubscriptionServer = false;
    for (VmessItem& server : config.collection().servers) {
        if (server.subId.trimmed().isEmpty()) {
            continue;
        }
        if (SubscriptionService::serverReuseKey(server) == activeReuseKey) {
            server.indexId = activeServerId;
            if (server.testResult.trimmed().isEmpty()) {
                server.testResult = activeServer->testResult;
            }
            hasDuplicateSubscriptionServer = true;
            break;
        }
    }

    if (hasDuplicateSubscriptionServer) {
        config.collection().servers.erase(
            std::remove_if(
                config.collection().servers.begin(),
                config.collection().servers.end(),
                [&activeServerId](const VmessItem& server) {
                    return server.indexId == activeServerId && server.subId.trimmed().isEmpty();
                }),
            config.collection().servers.end());
        config.currentIndexId = activeServerId;
        return true;
    }

    if (config.currentIndexId == activeServerId) {
        return false;
    }
    config.currentIndexId = activeServerId;
    return true;
}

} // namespace

SongBirdAutoCoordinator::SongBirdAutoCoordinator(QString configPath, QObject* parent)
    : QObject(parent)
    , configPath_(std::move(configPath))
{
    qRegisterMetaType<AutoNodeEvaluation>();
    qRegisterMetaType<QList<AutoNodeEvaluation>>();
    qRegisterMetaType<AutoCountrySummary>();
    qRegisterMetaType<QList<AutoCountrySummary>>();

    healthCheckTimer_.setSingleShot(true);
    QObject::connect(&healthCheckTimer_, &QTimer::timeout, this, &SongBirdAutoCoordinator::runHealthCheck);
    periodicRefreshTimer_.setSingleShot(true);
    QObject::connect(&periodicRefreshTimer_, &QTimer::timeout, this, [this]() {
        if (running_ && !stopRequested_) {
            updateSubscriptionsAndRetest();
        }
    });
}

SongBirdAutoCoordinator::~SongBirdAutoCoordinator()
{
    cancelWorkers_.store(true);
    if (evaluationCancel_ != nullptr) {
        evaluationCancel_->store(true);
    }
    stopProxySession(true);
    stopTunRuntime(true);
    clearThreads();
}

bool SongBirdAutoCoordinator::initialize()
{
    if (configPath_.trimmed().isEmpty()) {
        configPath_ = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("songbird-auto.json"));
    }

    repository_ = std::make_unique<JsonConfigRepository>(configPath_);
    serverService_ = std::make_unique<ServerService>(*repository_, resolveCustomConfigDirectory());
    subscriptionService_ = std::make_unique<SubscriptionService>(*repository_);
    networkAccessManager_ = std::make_unique<QNetworkAccessManager>();
    coreDiscoveryService_ = std::make_unique<CoreDiscoveryService>();
    coreCleanupService_ = std::make_unique<CoreProcessCleanupService>();
    tunRuntimeService_ = std::make_unique<TunRuntimeService>();
    clientConfigWriter_ = std::make_unique<ClientConfigWriter>(resolveCustomConfigDirectory());
    mainCore_ = std::make_unique<QtCoreProcessHost>();
    auxiliaryCore_ = std::make_unique<QtCoreProcessHost>();
    tunCore_ = std::make_unique<QtCoreProcessHost>();
    locationProbe_ = std::make_unique<OutboundLocationProbeService>();
    backgroundTasks_ = std::make_unique<BackgroundTaskCoordinator>();
    systemProxyService_ = std::make_unique<WindowsSystemProxyService>();

    reloadConfig();
    ensureDefaultSubscriptions();
    refreshExistingCoreTypes();

    runtimeResolver_ = std::make_unique<AppRuntimeResolver>(
        configPath_,
        config_,
        existingCoreTypes_,
        coreDiscoveryService_.get());

    runtimeEnvironment_ = std::make_unique<FunctionRuntimeEnvironment>();
    runtimeEnvironment_->cleanupPortProcessesFn = [this]() {
        if (coreCleanupService_ == nullptr) {
            return;
        }
        const QStringList cleaned = coreCleanupService_->cleanupCoreProcessesUsingConfiguredPorts(
            config_,
            OutboundLocationProbeService::LocationProbePortOffset);
        if (!cleaned.isEmpty()) {
            log(QStringLiteral("Cleaned core processes: %1").arg(cleaned.join(QStringLiteral(", "))));
        }
    };
    runtimeEnvironment_->removeStaleTunAdapterFn = [this]() {
        return tunRuntimeService_ == nullptr
            ? OperationResult::ok(QStringLiteral("TUN cleanup unavailable."))
            : tunRuntimeService_->removeStaleAdapterIfPresent();
    };
    runtimeEnvironment_->skipCoreChecksFn = []() { return false; };
    runtimeEnvironment_->isWindowsPlatformFn = []() { return isWindowsPlatform(); };
    runtimeEnvironment_->isProcessElevatedFn = []() { return isProcessElevated(); };

    activationCoordinator_ = std::make_unique<FunctionProxyActivationCoordinator>();
    activationCoordinator_->cancelBackgroundTasksForStartupFn = [this]() {
        cancelWorkers_.store(true);
    };
    activationCoordinator_->refreshExistingCoreTypesFn = [this]() { refreshExistingCoreTypes(); };
    activationCoordinator_->isSystemProxyEnabledFn = [this]() {
        return systemProxyService_ != nullptr && systemProxyService_->isEnabled();
    };
    activationCoordinator_->updateSystemProxyModeFn = [this](SystemProxyMode mode) {
        return updateSystemProxyMode(mode);
    };

    proxySession_ = std::make_unique<ProxySession>(ProxySession::Dependencies{
        *mainCore_,
        *auxiliaryCore_,
        *clientConfigWriter_,
        *locationProbe_,
        *backgroundTasks_,
        *runtimeResolver_,
        *runtimeEnvironment_,
        *activationCoordinator_});

    QObject::connect(proxySession_.get(), &ProxySession::activated, this, [this](const QString& location) {
        log(QStringLiteral("Proxy active. Location: %1").arg(location));
        const VmessItem* server = findServerById(activeServerId_);
        QString country;
        qint64 latency = -1;
        for (const AutoNodeEvaluation& evaluation : evaluations_) {
            if (evaluation.indexId == activeServerId_) {
                country = evaluation.countryDisplay;
                latency = evaluation.latencyMs;
                break;
            }
        }
        emit activeServerChanged(activeServerId_, server == nullptr ? QString() : serverDisplayName(*server), country, location, latency);
        taskSummary(QString());
        setRunning(true);
        setBusy(false);
        scheduleHealthCheck(kHealthCheckIntervalMs);
        if (refreshAfterFailover_) {
            refreshAfterFailover_ = false;
            maybeRefreshWhenCountryLow();
        }
    });
    QObject::connect(proxySession_.get(), &ProxySession::failed, this, [this](const QString& reason) {
        log(QStringLiteral("Proxy start failed: %1").arg(reason));
        setBusy(false);
        if (running_ && !stopRequested_) {
            handleUnavailableActiveServer(reason);
        } else {
            setRunning(false);
        }
    });
    QObject::connect(proxySession_.get(), &ProxySession::stopped, this, [this]() {
        if (pendingStartAfterStop_ && !stopRequested_) {
            pendingStartAfterStop_ = false;
            startProxySession();
            return;
        }
        if (stopRequested_) {
            finishStopped();
        }
    });
    QObject::connect(proxySession_.get(), &ProxySession::coreOutput, this, [this](const QString& line) {
        log(QStringLiteral("core | %1").arg(line));
    });
    QObject::connect(proxySession_.get(), &ProxySession::auxiliaryCoreOutput, this, [this](const QString& line) {
        log(QStringLiteral("tun-compat | %1").arg(line));
    });
    QObject::connect(proxySession_.get(), &ProxySession::logMessage, this, &SongBirdAutoCoordinator::logMessage);

    log(QStringLiteral("SongBirdAuto config: %1").arg(QDir::toNativeSeparators(configPath_)));
    countrySummaries_ = buildCountrySummaries(evaluations_);
    emit countrySummariesChanged(countrySummaries_);
    normalizeSelectedCountryAfterRetest();
    emit subscriptionUrlsTextChanged(subscriptionUrlsText());
    emit tunEnabledChanged(config_.tun().tunModeItem.enableTun);
    emit autoSelectionStrategyChanged(autoSelectionStrategy());
    syncTunRuntimeForCurrentState();
    return true;
}

QString SongBirdAutoCoordinator::configPath() const
{
    return configPath_;
}

Config SongBirdAutoCoordinator::currentConfig() const
{
    return config_;
}

QString SongBirdAutoCoordinator::selectedCountryCode() const
{
    return selectedCountryCode_;
}

bool SongBirdAutoCoordinator::isRunning() const
{
    return running_;
}

bool SongBirdAutoCoordinator::isTunEnabled() const
{
    return config_.tun().tunModeItem.enableTun;
}

QString SongBirdAutoCoordinator::subscriptionUrlsText() const
{
    QStringList urls;
    for (const SubItem& item : config_.collection().subscriptions) {
        if (!item.url.trimmed().isEmpty()) {
            urls.append(item.url.trimmed());
        }
    }
    return urls.join(QChar('\n'));
}

QString SongBirdAutoCoordinator::autoSelectionStrategy() const
{
    return normalizeAutoSelectionStrategy(config_.ui().autoSelectionStrategy);
}

void SongBirdAutoCoordinator::setSelectedCountryCode(const QString& countryCode)
{
    const QString normalized = normalizeAutoCountryCode(countryCode);
    if (selectedCountryCode_ == normalized) {
        return;
    }
    if (pendingStartAfterEvaluation_) {
        pendingStartAfterEvaluation_ = false;
        switchToBestAfterEvaluation_ = false;
        forceCountryTestAfterSubscriptionUpdate_ = false;
        forceSubscriptionUpdateIfNoUsableAfterTest_ = false;
        noUsableRecoveryUpdateAttempted_ = false;
        if (evaluationCancel_ != nullptr) {
            evaluationCancel_->store(true);
            evaluationCancel_.reset();
        }
        ++evaluationGeneration_;
        taskSummary(QString());
        if (running_ && !isProxySessionRunningOrTransitioning()) {
            setRunning(false);
        }
    }
    selectedCountryCode_ = normalized;
    emit selectedCountryChanged(selectedCountryCode_);
    setStatus(selectedCountryCode_.isEmpty()
        ? QStringLiteral("Select a country")
        : QStringLiteral("Selected %1").arg(selectedCountryDisplayName()));
}

void SongBirdAutoCoordinator::switchToCountry(const QString& countryCode)
{
    setSelectedCountryCode(countryCode);
    if (!running_ || stopRequested_ || selectedCountryCode_.isEmpty()) {
        return;
    }

    pendingStartAfterEvaluation_ = true;
    switchToBestAfterEvaluation_ = false;
    forceCountryTestAfterSubscriptionUpdate_ = false;
    forceSubscriptionUpdateIfNoUsableAfterTest_ = false;
    noUsableRecoveryUpdateAttempted_ = false;
    if (busy_) {
        setStatus(QStringLiteral("Switching to %1 after current task").arg(selectedCountryDisplayName()));
        return;
    }

    if (hasUsableSelectedCountryNode()) {
        pendingStartAfterEvaluation_ = false;
        switchToBestAfterEvaluation_ = false;
        startSelectedServerForSelectedCountry();
        return;
    }

    startBackgroundTestForSelectedCountry(true);
}

void SongBirdAutoCoordinator::setTunEnabled(bool enabled)
{
    if (config_.tun().tunModeItem.enableTun == enabled) {
        emit tunEnabledChanged(enabled);
        syncTunRuntimeForCurrentState();
        return;
    }

    if (busy_) {
        log(QStringLiteral("TUN setting is blocked while background work is running."));
        emit tunEnabledChanged(config_.tun().tunModeItem.enableTun);
        return;
    }

    config_.tun().tunModeItem.enableTun = enabled;
    if (!saveConfig()) {
        config_.tun().tunModeItem.enableTun = !enabled;
        emit tunEnabledChanged(config_.tun().tunModeItem.enableTun);
        log(QStringLiteral("Failed to save TUN setting."));
        return;
    }

    emit tunEnabledChanged(enabled);
    log(enabled ? QStringLiteral("TUN enabled.") : QStringLiteral("TUN disabled."));
    if (!syncTunRuntimeForCurrentState()) {
        emit tunEnabledChanged(config_.tun().tunModeItem.enableTun);
    }
}

void SongBirdAutoCoordinator::setAutoSelectionStrategy(const QString& strategy)
{
    const QString normalized = normalizeAutoSelectionStrategy(strategy);
    if (autoSelectionStrategy() == normalized) {
        emit autoSelectionStrategyChanged(normalized);
        return;
    }

    if (busy_) {
        log(QStringLiteral("Auto selection strategy is blocked while background work is running."));
        emit autoSelectionStrategyChanged(autoSelectionStrategy());
        return;
    }

    const QString previous = autoSelectionStrategy();
    config_.ui().autoSelectionStrategy = normalized;
    if (!saveConfig()) {
        config_.ui().autoSelectionStrategy = previous;
        log(QStringLiteral("Failed to save auto selection strategy."));
        emit autoSelectionStrategyChanged(autoSelectionStrategy());
        return;
    }

    emit autoSelectionStrategyChanged(normalized);
    log(normalized == kAutoStrategyFirstAvailable
        ? QStringLiteral("Auto strategy: first available.")
        : QStringLiteral("Auto strategy: lowest latency."));
}

bool SongBirdAutoCoordinator::saveRoutingSettings(
    const QList<RoutingItem>& routingItems,
    const QList<RoutingRule>& routingCustomRules,
    const QString& settingsRoutingRuleTabKey)
{
    if (busy_) {
        log(QStringLiteral("Routing settings are blocked while background work is running."));
        setStatus(QStringLiteral("Routing settings blocked"));
        return false;
    }

    config_.collection().routingModeId = RoutingProfiles::defaultRoutingModeId();
    for (const RoutingItem& item : routingItems) {
        if (item.locked && !item.id.trimmed().isEmpty()) {
            config_.collection().routingModeId = item.id;
            break;
        }
    }
    config_.collection().customRoutingItems = RoutingProfiles::customRoutingItemsFromRuntime(routingItems);
    config_.collection().routingCustomRules = routingCustomRules;
    RoutingProfiles::normalizeRoutingConfig(config_.collection());
    config_.ui().settingsRoutingRuleTabKey = settingsRoutingRuleTabKey;
    applyAutoRuntimeDefaults(config_);

    if (!saveConfig()) {
        log(QStringLiteral("Failed to save routing settings."));
        setStatus(QStringLiteral("Failed to save routing settings"));
        return false;
    }

    log(running_
        ? QStringLiteral("Routing settings saved. Changes apply on the next proxy start.")
        : QStringLiteral("Routing settings saved."));
    setStatus(QStringLiteral("Routing settings saved"));
    return true;
}

void SongBirdAutoCoordinator::saveSubscriptionUrlsText(const QString& text)
{
    QStringList urls;
    for (const QString& line : text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts)) {
        const QString url = normalizeSubscriptionUrl(line);
        if (!url.isEmpty() && !urls.contains(url, Qt::CaseInsensitive)) {
            urls.append(url);
        }
    }

    preserveActiveServerForSubscriptionUpdate(config_, activeServerId_);
    const OperationResult result = replaceSubscriptionsFromUrls(urls);
    const bool reconciled = reconcilePreservedActiveServer(config_, activeServerId_);
    if (reconciled) {
        saveConfig();
    }
    log(result.message);
    if (result.success) {
        lastSubscriptionUpdateAt_ = QDateTime();
        lastCountryTestAt_.clear();
        emit subscriptionUrlsTextChanged(subscriptionUrlsText());
        setStatus(QStringLiteral("Subscription URLs saved"));
    }
}

void SongBirdAutoCoordinator::start()
{
    stopRequested_ = false;
    if (selectedCountryCode_.isEmpty()) {
        if (!countrySummaries_.isEmpty()) {
            setSelectedCountryCode(firstCountryWithNodesOrFirst(countrySummaries_));
        } else {
            countrySummaries_ = buildCountrySummaries({});
            emit countrySummariesChanged(countrySummaries_);
            normalizeSelectedCountryAfterRetest();
        }
    }
    setRunning(true);
    pendingStartAfterEvaluation_ = true;
    switchToBestAfterEvaluation_ = false;
    forceCountryTestAfterSubscriptionUpdate_ = false;
    forceSubscriptionUpdateIfNoUsableAfterTest_ = false;
    noUsableRecoveryUpdateAttempted_ = false;
    setStatus(QStringLiteral("Preparing start"));
    prepareStartFromCurrentConfig();
}

void SongBirdAutoCoordinator::stop()
{
    const bool waitForCoreStop = proxySession_ != nullptr && proxySession_->isCoreRunning();
    invalidateOperations();
    stopRequested_ = true;
    pendingStartAfterStop_ = false;
    pendingStartAfterEvaluation_ = false;
    switchToBestAfterEvaluation_ = false;
    forceCountryTestAfterSubscriptionUpdate_ = false;
    forceSubscriptionUpdateIfNoUsableAfterTest_ = false;
    noUsableRecoveryUpdateAttempted_ = false;
    refreshQueuedAfterBusy_ = false;
    refreshQueuedAfterBusyForce_ = false;
    refreshAfterFailover_ = false;
    healthCheckInProgress_ = false;
    healthCheckTimer_.stop();
    periodicRefreshTimer_.stop();
    cancelWorkers_.store(true);
    if (evaluationCancel_ != nullptr) {
        evaluationCancel_->store(true);
    }
    for (QThread* thread : std::as_const(workerThreads_)) {
        if (thread != nullptr) {
            thread->requestInterruption();
        }
    }
    stopProxySession(false);
    updateSystemProxyMode(SystemProxyMode::ForcedClear);
    if (waitForCoreStop) {
        setBusy(false);
        setStatus(QStringLiteral("Stopping"));
    } else {
        finishStopped();
    }
}

void SongBirdAutoCoordinator::updateSubscriptionsAndRetest()
{
    updateSubscriptionsAndRetest(false);
}

void SongBirdAutoCoordinator::updateSubscriptionsAndRetest(bool forceUpdate)
{
    if (!running_ || stopRequested_) {
        return;
    }

    if (busy_) {
        if (running_ && !stopRequested_) {
            refreshQueuedAfterBusy_ = true;
            refreshQueuedAfterBusyForce_ = refreshQueuedAfterBusyForce_ || forceUpdate;
        }
        return;
    }

    if (!forceUpdate && subscriptionUpdateRecentlyCompleted()) {
        log(QStringLiteral("Skipping subscription update; last update was less than 10 minutes ago."));
        if (pendingStartAfterEvaluation_) {
            continueStartWithCurrentData(false, false, true);
        }
        schedulePeriodicRefresh();
        return;
    }

    setBusy(true);
    cancelWorkers_.store(false);
    if (evaluationCancel_ != nullptr) {
        evaluationCancel_->store(true);
    }
    setStatus(QStringLiteral("Updating subscriptions"));
    const quint64 operationId = nextOperationId();
    const QString activeServerIdBeforeUpdate = activeServerId_;
    int subscriptionTotal = 0;
    for (const SubItem& item : config_.collection().subscriptions) {
        if (item.enabled && !item.url.trimmed().isEmpty()) {
            ++subscriptionTotal;
        }
    }
    taskSummary(QStringLiteral("Updating subscriptions 0/%1").arg(subscriptionTotal));

    const QString configPath = configPath_;
    const bool updateViaActiveProxy = running_ && proxySession_ != nullptr && proxySession_->isCoreRunning();
    QPointer<SongBirdAutoCoordinator> self(this);
    QThread* thread = QThread::create([self, configPath, updateViaActiveProxy, operationId, activeServerIdBeforeUpdate]() {
        JsonConfigRepository repository(configPath);
        SubscriptionService subscriptionService(repository);
        QNetworkAccessManager networkAccessManager;
        SubscriptionUpdateService updateService(repository, subscriptionService, networkAccessManager);
        Config workerConfig = repository.load();
        applyAutoRuntimeDefaults(workerConfig);
        preserveActiveServerForSubscriptionUpdate(workerConfig, activeServerIdBeforeUpdate);
        const OperationResult updateResult = updateService.updateAllWithProgress(
            workerConfig,
            updateViaActiveProxy,
            [self, operationId](int current, int total, const QString&) {
                if (!self) {
                    return;
                }
                QMetaObject::invokeMethod(self, [self, current, total, operationId]() {
                    if (self && self->isCurrentOperation(operationId)) {
                        self->taskSummary(QStringLiteral("Updating subscriptions %1/%2").arg(current).arg(total));
                    }
                }, Qt::QueuedConnection);
            });
        const bool reconciled = reconcilePreservedActiveServer(workerConfig, activeServerIdBeforeUpdate);
        if (reconciled) {
            repository.save(workerConfig);
        }

        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, updateResult, operationId, activeServerIdBeforeUpdate]() {
            if (!self || !self->isCurrentOperation(operationId)) {
                return;
            }
            self->lastSubscriptionUpdateAt_ = QDateTime::currentDateTimeUtc();
            self->lastCountryTestAt_.clear();
            self->log(updateResult.message);
            self->taskSummary(QString());
            self->reloadConfig();
            self->refreshExistingCoreTypes();
            self->continueAfterSubscriptionUpdate(updateResult, operationId, true, false, false, false);
        }, Qt::QueuedConnection);
    });
    trackThread(thread);
    thread->start();
}

void SongBirdAutoCoordinator::reloadConfig()
{
    config_ = repository_ == nullptr ? Config() : repository_->load();
    applyAutoRuntimeDefaults(config_);
}

bool SongBirdAutoCoordinator::saveConfig()
{
    return repository_ != nullptr && repository_->save(config_);
}

QString SongBirdAutoCoordinator::resolveCustomConfigDirectory() const
{
    return QFileInfo(configPath_).dir().filePath(QStringLiteral("guiConfigs"));
}

OperationResult SongBirdAutoCoordinator::replaceSubscriptionsFromUrls(const QStringList& urls)
{
    if (subscriptionService_ == nullptr) {
        return OperationResult::fail(QStringLiteral("Subscription service is unavailable."));
    }

    QMap<QString, SubItem> existingByUrl;
    for (const SubItem& item : config_.collection().subscriptions) {
        const QString normalizedUrl = item.url.trimmed().toLower();
        if (!normalizedUrl.isEmpty() && !existingByUrl.contains(normalizedUrl)) {
            existingByUrl.insert(normalizedUrl, item);
        }
    }

    QList<SubItem> items;
    items.reserve(urls.size());
    QSet<QString> keptSubscriptionIds;
    int counter = 1;
    for (const QString& url : urls) {
        const QString normalizedUrl = url.trimmed().toLower();
        SubItem item = existingByUrl.value(normalizedUrl);
        if (item.id.trimmed().isEmpty()) {
            item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        item.remarks = item.remarks.trimmed().isEmpty()
            ? QStringLiteral("Auto Subscription %1").arg(counter)
            : item.remarks.trimmed();
        item.url = url;
        item.enabled = true;
        items.append(item);
        keptSubscriptionIds.insert(item.id.trimmed());
        ++counter;
    }

    auto serverEnd = std::remove_if(
        config_.collection().servers.begin(),
        config_.collection().servers.end(),
        [&keptSubscriptionIds](const VmessItem& server) {
            const QString subId = server.subId.trimmed();
            return !subId.isEmpty() && !keptSubscriptionIds.contains(subId);
        });
    config_.collection().servers.erase(serverEnd, config_.collection().servers.end());

    if (!config_.currentIndexId.trimmed().isEmpty()
        && findServerById(config_.currentIndexId) == nullptr) {
        config_.currentIndexId = config_.collection().servers.isEmpty()
            ? QString()
            : config_.collection().servers.constFirst().indexId;
    }
    if (!keptSubscriptionIds.contains(config_.ui().mainSelectedSubId.trimmed())) {
        config_.ui().mainSelectedSubId.clear();
    }

    return subscriptionService_->saveSubscriptions(config_, items);
}

void SongBirdAutoCoordinator::ensureDefaultSubscriptions()
{
    QStringList urls = bundledSubscriptionUrls();
    if (urls.isEmpty()) {
        return;
    }

    QList<SubItem> items = config_.collection().subscriptions;
    bool changed = false;
    int counter = 1;
    for (const QString& url : urls) {
        const auto exists = std::any_of(items.cbegin(), items.cend(), [&url](const SubItem& item) {
            return item.url.trimmed().compare(url, Qt::CaseInsensitive) == 0;
        });
        if (exists) {
            continue;
        }
        SubItem item;
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.remarks = QStringLiteral("Auto Subscription %1").arg(counter++);
        item.url = url;
        item.enabled = true;
        items.append(item);
        changed = true;
    }

    if (changed && subscriptionService_ != nullptr) {
        const OperationResult result = subscriptionService_->saveSubscriptions(config_, items);
        log(result.message);
        if (result.success) {
            emit subscriptionUrlsTextChanged(subscriptionUrlsText());
        }
    }
}

QStringList SongBirdAutoCoordinator::bundledSubscriptionUrls() const
{
    QStringList urls;
    QFile file(QStringLiteral(":/auto/urls.txt"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return urls;
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString url = normalizeSubscriptionUrl(stream.readLine());
        if (!url.isEmpty() && !urls.contains(url, Qt::CaseInsensitive)) {
            urls.append(url);
        }
    }
    return urls;
}

void SongBirdAutoCoordinator::refreshExistingCoreTypes()
{
    existingCoreTypes_.clear();
    if (coreDiscoveryService_ == nullptr) {
        return;
    }
    for (const CoreType coreType : availableCoreTypes()) {
        if (!coreDiscoveryService_->locateFirstExistingFile(
                coreDiscoveryService_->resolveCoreCandidates(coreType, configPath_)).isEmpty()) {
            existingCoreTypes_.append(coreType);
        }
    }
}

QList<CoreType> SongBirdAutoCoordinator::missingCoreTypesForEvaluation() const
{
    QList<CoreType> missingCoreTypes;
    if (coreDiscoveryService_ == nullptr) {
        return missingCoreTypes;
    }

    for (const VmessItem& server : config_.collection().servers) {
        if (server.configType == ConfigType::Custom) {
            continue;
        }

        const CoreType launchCore = resolveLaunchCoreType(server);
        if (launchCore == CoreType::Unknown || missingCoreTypes.contains(launchCore)) {
            continue;
        }

        const QString program = coreDiscoveryService_->locateFirstExistingFile(
            coreDiscoveryService_->resolveCoreCandidates(launchCore, configPath_));
        if (program.trimmed().isEmpty()) {
            missingCoreTypes.append(launchCore);
        }
    }

    if (config_.tun().tunModeItem.enableTun && !missingCoreTypes.contains(CoreType::SingBox)) {
        const QString singBoxProgram = coreDiscoveryService_->locateFirstExistingFile(
            coreDiscoveryService_->resolveCoreCandidates(CoreType::SingBox, configPath_));
        if (singBoxProgram.trimmed().isEmpty()) {
            missingCoreTypes.append(CoreType::SingBox);
        }
    }

    return missingCoreTypes;
}

QString SongBirdAutoCoordinator::resolveCoreInstallDirectory(CoreType coreType) const
{
    if (runtimeResolver_ != nullptr) {
        return runtimeResolver_->resolveCoreInstallDirectory(coreType);
    }

    const QString configDirectory = QFileInfo(configPath_).dir().absolutePath();
    return configDirectory.trimmed().isEmpty()
        ? QCoreApplication::applicationDirPath()
        : configDirectory;
}

void SongBirdAutoCoordinator::downloadMissingCoresThenContinue(
    const QList<CoreType>& coreTypes,
    const OperationResult& updateResult,
    quint64 operationId,
    bool allowSubscriptionUpdateBeforeTesting,
    bool preserveExistingEvaluations,
    bool allowNoUsableRecoveryUpdate)
{
    if (coreTypes.isEmpty()) {
        continueAfterSubscriptionUpdate(
            updateResult,
            operationId,
            false,
            allowSubscriptionUpdateBeforeTesting,
            preserveExistingEvaluations,
            allowNoUsableRecoveryUpdate);
        return;
    }

    QStringList displayNames;
    displayNames.reserve(coreTypes.size());
    for (const CoreType coreType : coreTypes) {
        displayNames.append(coreTypeDisplayName(coreType));
    }
    QList<QPair<CoreType, QString>> downloads;
    downloads.reserve(coreTypes.size());
    for (const CoreType coreType : coreTypes) {
        downloads.append(qMakePair(coreType, resolveCoreInstallDirectory(coreType)));
    }

    setStatus(QStringLiteral("Downloading %1").arg(displayNames.join(QStringLiteral(", "))));
    taskSummary(QStringLiteral("Downloading core 0/%1").arg(coreTypes.size()));

    const bool checkPreReleaseUpdate = config_.checkPreReleaseUpdate;
    const bool ignoreGeoUpdateCore = config_.ignoreGeoUpdateCore;
    QPointer<SongBirdAutoCoordinator> self(this);
    QThread* thread = QThread::create([
        self,
        downloads,
        updateResult,
        operationId,
        checkPreReleaseUpdate,
        ignoreGeoUpdateCore,
        allowSubscriptionUpdateBeforeTesting,
        preserveExistingEvaluations,
        allowNoUsableRecoveryUpdate]() {
        QList<OperationResult> results;
        results.reserve(downloads.size());

        for (int i = 0; i < downloads.size(); ++i) {
            if (!self) {
                return;
            }

            const CoreType coreType = downloads.at(i).first;
            const QString installDirectory = downloads.at(i).second;
            QMetaObject::invokeMethod(self, [self, coreType, installDirectory, i, total = downloads.size(), operationId]() {
                if (!self || !self->isCurrentOperation(operationId)) {
                    return;
                }
                self->taskSummary(QStringLiteral("Downloading core %1/%2").arg(i + 1).arg(total));
                self->log(QStringLiteral("Missing %1 core detected. Downloading to %2.")
                              .arg(coreTypeDisplayName(coreType), QDir::toNativeSeparators(installDirectory)));
            }, Qt::QueuedConnection);

            CoreUpdateService updateService;
            CoreUpdateService::UpdateOptions options;
            options.progressHandler = [self, operationId](const QString& message) {
                if (!self) {
                    return;
                }
                QMetaObject::invokeMethod(self, [self, message, operationId]() {
                    if (self && self->isCurrentOperation(operationId)) {
                        self->taskSummary(message);
                        self->log(message);
                    }
                }, Qt::QueuedConnection);
            };
            options.cancelCheck = [self, operationId]() {
                QThread* currentThread = QThread::currentThread();
                return !self
                    || self->cancelWorkers_.load()
                    || (currentThread != nullptr && currentThread->isInterruptionRequested());
            };
            options.skipLocalVersionCheck = true;

            const OperationResult result = updateService.update(
                coreType,
                CoreUpdateConfig{checkPreReleaseUpdate, ignoreGeoUpdateCore},
                installDirectory,
                options);
            results.append(result);
            if (!result.success) {
                break;
            }
        }

        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [
            self,
            results,
            updateResult,
            operationId,
            allowSubscriptionUpdateBeforeTesting,
            preserveExistingEvaluations,
            allowNoUsableRecoveryUpdate]() {
            if (!self || !self->isCurrentOperation(operationId)) {
                return;
            }
            for (const OperationResult& result : results) {
                self->log(result.message);
                if (!result.success) {
                    self->taskSummary(QString());
                    self->setBusy(false);
                    self->pendingStartAfterEvaluation_ = false;
                    self->switchToBestAfterEvaluation_ = false;
                    self->forceCountryTestAfterSubscriptionUpdate_ = false;
                    self->forceSubscriptionUpdateIfNoUsableAfterTest_ = false;
                    self->setRunning(false);
                    self->setStatus(result.cancelled
                        ? QStringLiteral("Core download canceled")
                        : QStringLiteral("Core download failed"));
                    return;
                }
            }

            self->taskSummary(QString());
            self->refreshExistingCoreTypes();
            self->continueAfterSubscriptionUpdate(
                updateResult,
                operationId,
                false,
                allowSubscriptionUpdateBeforeTesting,
                preserveExistingEvaluations,
                allowNoUsableRecoveryUpdate);
        }, Qt::QueuedConnection);
    });
    trackThread(thread);
    thread->start();
}

void SongBirdAutoCoordinator::continueAfterSubscriptionUpdate(
    const OperationResult& updateResult,
    quint64 operationId,
    bool allowCoreDownload,
    bool allowSubscriptionUpdateBeforeTesting,
    bool preserveExistingEvaluations,
    bool allowNoUsableRecoveryUpdate)
{
    if (!isCurrentOperation(operationId) || !running_ || stopRequested_) {
        return;
    }

    const QList<CoreType> missingCoreTypes = missingCoreTypesForEvaluation();
    if (allowCoreDownload && !missingCoreTypes.isEmpty()) {
        downloadMissingCoresThenContinue(
            missingCoreTypes,
            updateResult,
            operationId,
            allowSubscriptionUpdateBeforeTesting,
            preserveExistingEvaluations,
            allowNoUsableRecoveryUpdate);
        return;
    }

    const QList<SpeedTestRequestItem> items = buildEvaluationItems();
    if (!updateResult.success && !items.isEmpty()) {
        log(QStringLiteral("Subscription update failed; using existing nodes for startup."));
    }
    if (items.isEmpty()) {
        if (pendingStartAfterEvaluation_
            && allowNoUsableRecoveryUpdate
            && !noUsableRecoveryUpdateAttempted_) {
            noUsableRecoveryUpdateAttempted_ = true;
            forceCountryTestAfterSubscriptionUpdate_ = true;
            forceSubscriptionUpdateIfNoUsableAfterTest_ = false;
            log(QStringLiteral("No inferred nodes for %1. Updating subscriptions.")
                    .arg(selectedCountryDisplayName()));
            setBusy(false);
            setStatus(QStringLiteral("Updating subscriptions after no usable nodes"));
            updateSubscriptionsAndRetest(true);
            return;
        }
        handleNoCompatibleServers();
        return;
    }

    refreshInferredCountrySummaries(items, preserveExistingEvaluations);
    setBusy(false);
    setStatus(QStringLiteral("Ready: %1 node(s) classified").arg(items.size()));
    if (running_ && pendingStartAfterEvaluation_) {
        const bool forceCountryTest = forceCountryTestAfterSubscriptionUpdate_;
        forceCountryTestAfterSubscriptionUpdate_ = false;
        continueStartWithCurrentData(
            allowSubscriptionUpdateBeforeTesting,
            forceCountryTest,
            allowNoUsableRecoveryUpdate);
    }
    schedulePeriodicRefresh();
}

QList<AutoCountrySummary> SongBirdAutoCoordinator::buildCountrySummaries(
    const QList<AutoNodeEvaluation>& evaluations) const
{
    return buildAutoCountrySummaries(evaluations);
}

QList<SpeedTestRequestItem> SongBirdAutoCoordinator::buildEvaluationItems() const
{
    QList<SpeedTestRequestItem> items;
    for (const VmessItem& server : config_.collection().servers) {
        if (server.configType == ConfigType::Custom) {
            continue;
        }
        const CoreType launchCore = resolveLaunchCoreType(server);
        if (launchCore == CoreType::Unknown) {
            continue;
        }
        VmessItem runtimeServer = server;
        runtimeServer.coreType = launchCore;
        const CoreInfo coreInfo = resolveCoreInfo(runtimeServer);
        if (coreInfo.program.trimmed().isEmpty()) {
            continue;
        }
        items.append(SpeedTestRequestItem{
            server.indexId,
            serverDisplayName(server),
            server.configType,
            runtimeServer,
            coreInfo});
    }
    return items;
}

QList<SpeedTestRequestItem> SongBirdAutoCoordinator::buildEvaluationItemsForCountry(const QString& countryCode) const
{
    const QString normalizedCountryCode = normalizeAutoCountryCode(countryCode);
    if (normalizedCountryCode.isEmpty()) {
        return {};
    }

    QList<SpeedTestRequestItem> result;
    const QList<SpeedTestRequestItem> items = buildEvaluationItems();
    for (const SpeedTestRequestItem& item : items) {
        if (inferAutoCountryCodeFromNodeName(item.displayName) == normalizedCountryCode) {
            result.append(item);
        }
    }
    return result;
}

void SongBirdAutoCoordinator::refreshInferredCountrySummaries(
    const QList<SpeedTestRequestItem>& items,
    bool preserveExistingResults)
{
    if (!preserveExistingResults) {
        evaluations_ = inferredAutoEvaluationsFromItems(items);
    } else {
        QMap<QString, AutoNodeEvaluation> existingById;
        for (const AutoNodeEvaluation& evaluation : std::as_const(evaluations_)) {
            existingById.insert(evaluation.indexId, evaluation);
        }

        QList<AutoNodeEvaluation> mergedEvaluations;
        mergedEvaluations.reserve(items.size());
        for (const SpeedTestRequestItem& item : items) {
            const AutoNodeEvaluation inferred = inferredAutoEvaluationFromItem(item);
            if (!existingById.contains(item.indexId)) {
                mergedEvaluations.append(inferred);
                continue;
            }

            AutoNodeEvaluation merged = existingById.value(item.indexId);
            merged.indexId = inferred.indexId;
            merged.displayName = inferred.displayName;
            merged.inferredCountryCode = inferred.inferredCountryCode;
            if (merged.countryCode.trimmed().isEmpty()) {
                merged.countryCode = inferred.countryCode;
            }
            if (merged.countryName.trimmed().isEmpty()) {
                merged.countryName = inferred.countryName;
            }
            if (merged.countryDisplay.trimmed().isEmpty()) {
                merged.countryDisplay = inferred.countryDisplay;
            }
            if (!merged.tested) {
                merged.countryCode = inferred.countryCode;
                merged.countryName = inferred.countryName;
                merged.countryDisplay = inferred.countryDisplay;
                merged.available = true;
                merged.error = inferred.error;
            }
            mergedEvaluations.append(merged);
        }
        evaluations_ = mergedEvaluations;
    }
    countrySummaries_ = buildCountrySummaries(evaluations_);
    normalizeSelectedCountryAfterRetest();
    emit nodeEvaluationsChanged(evaluations_);
    emit countrySummariesChanged(countrySummaries_);
}

void SongBirdAutoCoordinator::prepareStartFromCurrentConfig()
{
    if (!running_ || stopRequested_) {
        return;
    }
    if (busy_) {
        refreshQueuedAfterBusy_ = true;
        return;
    }

    setBusy(true);
    cancelWorkers_.store(false);
    if (evaluationCancel_ != nullptr) {
        evaluationCancel_->store(true);
    }
    const quint64 operationId = nextOperationId();
    continueAfterSubscriptionUpdate(
        OperationResult::ok(QStringLiteral("Using existing subscriptions.")),
        operationId,
        true,
        true,
        true,
        true);
}

void SongBirdAutoCoordinator::continueStartWithCurrentData(
    bool allowSubscriptionUpdateBeforeTesting,
    bool forceCountryTest,
    bool allowNoUsableRecoveryUpdate)
{
    if (!running_ || stopRequested_ || !pendingStartAfterEvaluation_) {
        return;
    }

    if (!hasUsableSelectedCountryNode()) {
        forceSubscriptionUpdateIfNoUsableAfterTest_ = allowNoUsableRecoveryUpdate;
        if (allowNoUsableRecoveryUpdate) {
            log(QStringLiteral("No usable node for %1. Retesting before updating subscriptions.")
                    .arg(selectedCountryDisplayName()));
        }
        startBackgroundTestForSelectedCountry(true);
        return;
    }

    forceSubscriptionUpdateIfNoUsableAfterTest_ = false;
    if (allowSubscriptionUpdateBeforeTesting) {
        if (!subscriptionUpdateRecentlyCompleted()) {
            setStatus(QStringLiteral("Updating subscriptions before start"));
            updateSubscriptionsAndRetest(false);
            return;
        }
        log(QStringLiteral("Skipping subscription update; last update was less than 10 minutes ago."));
    }

    startBackgroundTestForSelectedCountry(forceCountryTest);
}

void SongBirdAutoCoordinator::startBackgroundTestForSelectedCountry(bool forceTest)
{
    if (selectedCountryCode_.isEmpty() || busy_) {
        return;
    }

    if (!forceTest && selectedCountryTestRecentlyCompleted() && !currentCountryBestServerId().isEmpty()) {
        log(QStringLiteral("Skipping %1 test; last test was less than 5 minutes ago.")
                .arg(selectedCountryDisplayName()));
        taskSummary(QString());
        setStatus(QStringLiteral("Using recent test results for %1").arg(selectedCountryDisplayName()));
        if (pendingStartAfterEvaluation_) {
            pendingStartAfterEvaluation_ = false;
            switchToBestAfterEvaluation_ = false;
            startSelectedServerForSelectedCountry();
        }
        return;
    }

    if (evaluationCancel_ != nullptr) {
        evaluationCancel_->store(true);
    }
    const quint64 evaluationId = ++evaluationGeneration_;
    auto cancelFlag = std::make_shared<std::atomic_bool>(false);
    evaluationCancel_ = cancelFlag;

    const QString testedCountryCode = selectedCountryCode_;
    const QList<SpeedTestRequestItem> items = buildEvaluationItemsForCountry(testedCountryCode);
    if (items.isEmpty()) {
        if (pendingStartAfterEvaluation_
            && forceSubscriptionUpdateIfNoUsableAfterTest_
            && !noUsableRecoveryUpdateAttempted_) {
            noUsableRecoveryUpdateAttempted_ = true;
            forceCountryTestAfterSubscriptionUpdate_ = true;
            forceSubscriptionUpdateIfNoUsableAfterTest_ = false;
            log(QStringLiteral("No inferred nodes for %1. Updating subscriptions.")
                    .arg(selectedCountryDisplayName()));
            setStatus(QStringLiteral("Updating subscriptions after no usable nodes"));
            taskSummary(QString());
            updateSubscriptionsAndRetest(true);
            return;
        }
        setStatus(QStringLiteral("No inferred nodes for %1").arg(selectedCountryDisplayName()));
        pendingStartAfterEvaluation_ = false;
        switchToBestAfterEvaluation_ = false;
        taskSummary(QString());
        if (running_ && !isProxySessionRunningOrTransitioning()) {
            setRunning(false);
        }
        return;
    }

    setStatus(QStringLiteral("Testing %1 %2 node(s) in background")
                  .arg(items.size())
                  .arg(selectedCountryDisplayName()));
    taskSummary(QStringLiteral("Checking outbound location 0/%1").arg(items.size()));

    AutoNodeEvaluationService::Request request;
    request.config = config_;
    request.items = items;
    request.customConfigDirectory = resolveCustomConfigDirectory();
    request.urlTestUrl = SpeedTestRuntimeRunner::defaultUrlTestUrl(config_);
    request.maxConcurrency = 8;

    const int totalCount = request.items.size();
    auto completedCount = std::make_shared<std::atomic_int>(0);
    QPointer<SongBirdAutoCoordinator> self(this);
    QThread* evalThread = QThread::create([
        self,
        request,
        cancelFlag,
        evaluationId,
        completedCount,
        totalCount,
        testedCountryCode]() {
        if (!self) {
            return;
        }
        const QList<AutoNodeEvaluation> results = AutoNodeEvaluationService::evaluate(
            request,
            *cancelFlag,
            [self, evaluationId](const QString& message) {
                if (!self) {
                    return;
                }
                QMetaObject::invokeMethod(self, [self, message, evaluationId]() {
                    if (self && self->evaluationGeneration_ == evaluationId) {
                        self->log(message);
                    }
                }, Qt::QueuedConnection);
            },
            [self, evaluationId, completedCount, totalCount](const AutoNodeEvaluation& evaluation) {
                if (!self) {
                    return;
                }
                QMetaObject::invokeMethod(self, [self, evaluation, evaluationId, completedCount, totalCount]() {
                    if (!self || self->evaluationGeneration_ != evaluationId) {
                        return;
                    }
                    self->log(QStringLiteral("Test result | %1 -> %2")
                                  .arg(evaluation.displayName, evaluationStateText(evaluation)));
                    const int completed = completedCount->fetch_add(1) + 1;
                    self->taskSummary(QStringLiteral("Checking outbound location %1/%2")
                                          .arg(completed)
                                          .arg(totalCount));
                    self->mergeEvaluationResult(evaluation);
                    self->maybeStartFirstAvailableNode(evaluation);
                }, Qt::QueuedConnection);
            });

        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, results, cancelFlag, evaluationId, testedCountryCode]() {
            if (!self || self->evaluationGeneration_ != evaluationId) {
                return;
            }
            if (self->evaluationCancel_ == cancelFlag) {
                self->evaluationCancel_.reset();
            }
            self->lastCountryTestAt_[testedCountryCode] = QDateTime::currentDateTimeUtc();
            self->mergeEvaluationResults(results);
            self->taskSummary(QString());
            self->setStatus(QStringLiteral("Ready: tested %1 %2 node(s)")
                                .arg(results.size())
                                .arg(self->selectedCountryDisplayName()));
            if (self->pendingStartAfterEvaluation_
                && !self->hasUsableSelectedCountryNode()
                && self->forceSubscriptionUpdateIfNoUsableAfterTest_
                && !self->noUsableRecoveryUpdateAttempted_) {
                self->noUsableRecoveryUpdateAttempted_ = true;
                self->forceCountryTestAfterSubscriptionUpdate_ = true;
                self->forceSubscriptionUpdateIfNoUsableAfterTest_ = false;
                self->log(QStringLiteral("No usable node after retesting %1. Updating subscriptions.")
                              .arg(self->selectedCountryDisplayName()));
                self->setStatus(QStringLiteral("Updating subscriptions after no usable nodes"));
                self->updateSubscriptionsAndRetest(true);
                return;
            }
            if (self->pendingStartAfterEvaluation_) {
                self->pendingStartAfterEvaluation_ = false;
                self->switchToBestAfterEvaluation_ = false;
                self->forceSubscriptionUpdateIfNoUsableAfterTest_ = false;
                self->startSelectedServerForSelectedCountry();
            } else if (self->switchToBestAfterEvaluation_) {
                self->maybeSwitchToBestServerAfterEvaluation();
            }
            if (self->isSelectedCountryLow()) {
                self->maybeRefreshWhenCountryLow();
            }
        }, Qt::QueuedConnection);
    });
    trackThread(evalThread);
    evalThread->start();
}

void SongBirdAutoCoordinator::mergeEvaluationResults(const QList<AutoNodeEvaluation>& results)
{
    QMap<QString, AutoNodeEvaluation> byId;
    for (const AutoNodeEvaluation& evaluation : std::as_const(evaluations_)) {
        byId.insert(evaluation.indexId, evaluation);
    }
    for (const AutoNodeEvaluation& result : results) {
        AutoNodeEvaluation merged = result;
        if (byId.contains(result.indexId)) {
            const AutoNodeEvaluation previous = byId.value(result.indexId);
            merged.inferredCountryCode = previous.inferredCountryCode.trimmed().isEmpty()
                ? previous.countryCode
                : previous.inferredCountryCode;
            if (merged.countryCode.trimmed().isEmpty()) {
                merged.countryCode = previous.countryCode;
                merged.countryDisplay = previous.countryDisplay;
            }
        }
        byId.insert(result.indexId, merged);
    }

    for (AutoNodeEvaluation& evaluation : evaluations_) {
        if (byId.contains(evaluation.indexId)) {
            evaluation = byId.value(evaluation.indexId);
        }
    }
    for (const AutoNodeEvaluation& result : results) {
        const auto existing = std::find_if(evaluations_.cbegin(), evaluations_.cend(), [&result](const AutoNodeEvaluation& evaluation) {
            return evaluation.indexId == result.indexId;
        });
        if (existing == evaluations_.cend() && byId.contains(result.indexId)) {
            evaluations_.append(byId.value(result.indexId));
        }
    }
    countrySummaries_ = buildCountrySummaries(evaluations_);
    normalizeSelectedCountryAfterRetest();
    emit nodeEvaluationsChanged(evaluations_);
    emit countrySummariesChanged(countrySummaries_);
}

void SongBirdAutoCoordinator::mergeEvaluationResult(const AutoNodeEvaluation& result)
{
    mergeEvaluationResults(QList<AutoNodeEvaluation>{result});
}

void SongBirdAutoCoordinator::maybeStartFirstAvailableNode(const AutoNodeEvaluation& evaluation)
{
    if (!pendingStartAfterEvaluation_
        || !running_
        || stopRequested_
        || !evaluation.available
        || isProxySessionRunningOrTransitioning()) {
        return;
    }

    AutoNodeEvaluation mergedEvaluation = evaluation;
    const auto mergedIt = std::find_if(evaluations_.cbegin(), evaluations_.cend(), [&evaluation](const AutoNodeEvaluation& item) {
        return item.indexId == evaluation.indexId;
    });
    if (mergedIt != evaluations_.cend()) {
        mergedEvaluation = *mergedIt;
    }
    if (autoCountryKeyForEvaluation(mergedEvaluation) != normalizeAutoCountryCode(selectedCountryCode_)) {
        return;
    }

    pendingStartAfterEvaluation_ = false;
    switchToBestAfterEvaluation_ = !usesFirstAvailableStrategy();
    forceSubscriptionUpdateIfNoUsableAfterTest_ = false;
    log(QStringLiteral("Starting first available node while tests continue: %1").arg(mergedEvaluation.displayName));
    setStatus(QStringLiteral("Starting first available node"));
    if (!selectServer(mergedEvaluation.indexId)) {
        pendingStartAfterEvaluation_ = true;
        switchToBestAfterEvaluation_ = false;
        return;
    }
    startProxySession();
}

void SongBirdAutoCoordinator::maybeSwitchToBestServerAfterEvaluation()
{
    switchToBestAfterEvaluation_ = false;
    if (!running_ || stopRequested_) {
        return;
    }

    const QString bestServerId = currentCountryBestServerId();
    if (bestServerId.isEmpty()) {
        return;
    }
    if (bestServerId == activeServerId_) {
        log(QStringLiteral("Current node remains the fastest tested node."));
        return;
    }

    log(QStringLiteral("Switching to fastest tested node after background tests completed."));
    startBestServerForSelectedCountry();
}

const VmessItem* SongBirdAutoCoordinator::findServerById(const QString& indexId) const
{
    for (const VmessItem& server : config_.collection().servers) {
        if (server.indexId == indexId) {
            return &server;
        }
    }
    return nullptr;
}

CoreType SongBirdAutoCoordinator::resolveLaunchCoreType(const VmessItem& server) const
{
    return resolveSelectedCoreType(config_, server, existingCoreTypes_);
}

CoreInfo SongBirdAutoCoordinator::resolveCoreInfo(const VmessItem& server) const
{
    if (runtimeResolver_ == nullptr) {
        return {};
    }
    return runtimeResolver_->resolveCoreInfo(server);
}

QString SongBirdAutoCoordinator::currentCountryBestServerId() const
{
    return bestAutoServerIdForCountry(evaluations_, selectedCountryCode_);
}

QString SongBirdAutoCoordinator::currentCountryFirstAvailableServerId() const
{
    const QString normalizedCountryCode = normalizeAutoCountryCode(selectedCountryCode_);
    for (const AutoNodeEvaluation& evaluation : evaluations_) {
        if (evaluation.available && autoCountryKeyForEvaluation(evaluation) == normalizedCountryCode) {
            return evaluation.indexId;
        }
    }
    return {};
}

bool SongBirdAutoCoordinator::hasUsableSelectedCountryNode() const
{
    return !selectedCountryCode_.isEmpty()
        && countAvailableAutoServersForCountry(evaluations_, selectedCountryCode_) > 0;
}

bool SongBirdAutoCoordinator::usesFirstAvailableStrategy() const
{
    return autoSelectionStrategy() == kAutoStrategyFirstAvailable;
}

bool SongBirdAutoCoordinator::isProxySessionRunningOrTransitioning() const
{
    return proxySession_ != nullptr
        && (proxySession_->isCoreRunning() || proxySession_->isTransitioning());
}

bool SongBirdAutoCoordinator::subscriptionUpdateRecentlyCompleted() const
{
    return lastSubscriptionUpdateAt_.isValid()
        && lastSubscriptionUpdateAt_.msecsTo(QDateTime::currentDateTimeUtc()) <= kSubscriptionUpdateCooldownMs;
}

bool SongBirdAutoCoordinator::selectedCountryTestRecentlyCompleted() const
{
    const QDateTime lastTestAt = lastCountryTestAt_.value(selectedCountryCode_);
    return lastTestAt.isValid()
        && lastTestAt.msecsTo(QDateTime::currentDateTimeUtc()) <= kCountryTestCooldownMs;
}

void SongBirdAutoCoordinator::startBestServerForSelectedCountry()
{
    if (selectedCountryCode_.isEmpty()) {
        setStatus(QStringLiteral("Select a country"));
        return;
    }
    const QString bestServerId = currentCountryBestServerId();
    if (bestServerId.isEmpty()) {
        log(QStringLiteral("No usable tested node for %1.").arg(selectedCountryCode_));
        pendingStartAfterEvaluation_ = false;
        switchToBestAfterEvaluation_ = false;
        if (running_ && !isProxySessionRunningOrTransitioning()) {
            setRunning(false);
        }
        setStatus(QStringLiteral("No usable node for %1").arg(selectedCountryDisplayName()));
        return;
    }
    if (!selectServer(bestServerId)) {
        return;
    }
    if (isProxySessionRunningOrTransitioning()) {
        pendingStartAfterStop_ = true;
        setBusy(true);
        setStatus(QStringLiteral("Switching proxy"));
        stopProxySession(false);
        return;
    }
    startProxySession();
}

void SongBirdAutoCoordinator::startSelectedServerForSelectedCountry()
{
    if (!usesFirstAvailableStrategy()) {
        startBestServerForSelectedCountry();
        return;
    }

    if (selectedCountryCode_.isEmpty()) {
        setStatus(QStringLiteral("Select a country"));
        return;
    }
    const QString serverId = currentCountryFirstAvailableServerId();
    if (serverId.isEmpty()) {
        log(QStringLiteral("No usable tested node for %1.").arg(selectedCountryCode_));
        pendingStartAfterEvaluation_ = false;
        switchToBestAfterEvaluation_ = false;
        if (running_ && !isProxySessionRunningOrTransitioning()) {
            setRunning(false);
        }
        setStatus(QStringLiteral("No usable node for %1").arg(selectedCountryDisplayName()));
        return;
    }
    if (!selectServer(serverId)) {
        return;
    }
    if (isProxySessionRunningOrTransitioning()) {
        pendingStartAfterStop_ = true;
        setBusy(true);
        setStatus(QStringLiteral("Switching proxy"));
        stopProxySession(false);
        return;
    }
    startProxySession();
}

bool SongBirdAutoCoordinator::selectServer(const QString& indexId)
{
    if (serverService_ == nullptr) {
        return false;
    }
    if (config_.currentIndexId == indexId) {
        return true;
    }
    const OperationResult result = serverService_->setDefaultServer(config_, indexId);
    log(result.message);
    return result.success;
}

void SongBirdAutoCoordinator::startProxySession()
{
    if (proxySession_ == nullptr) {
        return;
    }
    if (config_.tun().tunModeItem.enableTun && !startTunRuntime(true)) {
        setBusy(false);
        setRunning(false);
        return;
    }
    activeServerId_ = config_.currentIndexId;
    const VmessItem* server = findServerById(activeServerId_);
    QString country;
    qint64 latency = -1;
    for (const AutoNodeEvaluation& evaluation : evaluations_) {
        if (evaluation.indexId == activeServerId_) {
            country = evaluation.countryDisplay;
            latency = evaluation.latencyMs;
            break;
        }
    }
    emit activeServerChanged(activeServerId_, server == nullptr ? QString() : serverDisplayName(*server), country, QString(), latency);
    setBusy(true);
    setStatus(QStringLiteral("Starting proxy"));
    log(QStringLiteral("Starting best node: %1").arg(server == nullptr ? activeServerId_ : serverDisplayName(*server)));

    ProxySession::StartRequest request;
    request.config = config_;
    request.config.tun().tunModeItem.enableTun = false;
    request.existingCoreTypes = existingCoreTypes_;
    request.skipTunCleanup = false;
    request.showOverlay = false;
    request.warnOnFailure = true;
    proxySession_->start(request);
}

void SongBirdAutoCoordinator::stopProxySession(bool immediate)
{
    if (proxySession_ != nullptr) {
        proxySession_->stop(immediate);
    }
    if (immediate) {
        updateSystemProxyMode(SystemProxyMode::ForcedClear);
    }
}

CoreInfo SongBirdAutoCoordinator::resolveSingBoxCoreInfo() const
{
    CoreInfo info;
    info.type = CoreType::SingBox;
    const QString program = coreDiscoveryService_ == nullptr
        ? QString()
        : coreDiscoveryService_->locateFirstExistingFile(
              coreDiscoveryService_->resolveCoreCandidates(CoreType::SingBox, configPath_));
    if (program.trimmed().isEmpty()) {
        return {};
    }
    info.program = program;
    info.arguments.clear();
    info.arguments << QStringLiteral("run") << QStringLiteral("-c") << info.configPlaceholder;
    info.appendConfigArgument = false;
    info.workingDirectory = QFileInfo(program).absolutePath();
    return info;
}

QString SongBirdAutoCoordinator::writeTunRuntimeConfig(bool relayToProxy) const
{
    const QString runtimeDirectory = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("runtime"));
    if (!QDir().mkpath(runtimeDirectory)) {
        return {};
    }

    QJsonObject root;
    QJsonObject logObject;
    logObject.insert(QStringLiteral("disabled"), false);
    logObject.insert(QStringLiteral("level"), QStringLiteral("warn"));
    root.insert(QStringLiteral("log"), logObject);
    root.insert(QStringLiteral("dns"), SingBoxConfigFragments::buildTunCompatDns());

    QJsonArray inbounds;
    inbounds.append(SingBoxConfigFragments::buildTunInbound(config_));
    root.insert(QStringLiteral("inbounds"), inbounds);

    QJsonArray outbounds;
    if (relayToProxy) {
        outbounds.append(SingBoxConfigFragments::buildTunCompatOutbounds(config_));
    } else {
        outbounds.append(SingBoxConfigFragments::buildDirectOutbound());
        outbounds.append(SingBoxConfigFragments::buildBlockOutbound());
    }
    root.insert(QStringLiteral("outbounds"), outbounds);

    QJsonObject route;
    route.insert(QStringLiteral("auto_detect_interface"), true);
    route.insert(QStringLiteral("final"), relayToProxy ? QStringLiteral("proxy") : QStringLiteral("direct"));
    QJsonArray rules = SingBoxConfigFragments::buildTunCompatRejectRules();
    SingBoxConfigFragments::appendTunCompatProcessRules(rules);
    rules.append(SingBoxConfigFragments::buildTunCompatPrivateAddressDirectRule());
    SingBoxConfigFragments::appendTunIcmpRouteRule(rules, config_.tun().tunModeItem);
    if (relayToProxy) {
        SingBoxConfigFragments::appendSniffRules(rules, config_);
        SingBoxConfigFragments::appendTunUdpRouteRule(rules, config_.tun().tunModeItem);
    }
    route.insert(QStringLiteral("rules"), rules);
    root.insert(QStringLiteral("route"), route);

    const QString configPath = QDir(runtimeDirectory).filePath(QStringLiteral("auto-tun.generated.json"));
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {};
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return configPath;
}

bool SongBirdAutoCoordinator::startTunRuntime(bool relayToProxy)
{
    if (!config_.tun().tunModeItem.enableTun) {
        stopTunRuntime(true);
        return true;
    }
    if (tunCore_ == nullptr) {
        return false;
    }
    if (tunCore_->isRunning() && tunRuntimeRelayToProxy_ == relayToProxy) {
        return true;
    }
    if (isWindowsPlatform() && !isProcessElevated()) {
        log(QStringLiteral("TUN requires administrator privileges."));
        setStatus(QStringLiteral("TUN requires administrator privileges"));
        return false;
    }

    stopTunRuntime(false);
    if (tunRuntimeService_ != nullptr) {
        const OperationResult cleanupResult = tunRuntimeService_->removeStaleAdapterIfPresent();
        log(cleanupResult.message);
        if (!cleanupResult.success) {
            return false;
        }
    }

    const CoreInfo coreInfo = resolveSingBoxCoreInfo();
    if (coreInfo.program.trimmed().isEmpty()) {
        log(QStringLiteral("TUN requires sing-box, but no sing-box executable was found."));
        return false;
    }
    const QString configPath = writeTunRuntimeConfig(relayToProxy);
    if (configPath.trimmed().isEmpty()) {
        log(QStringLiteral("Failed to write TUN runtime config."));
        return false;
    }

    tunRuntimeRelayToProxy_ = relayToProxy;
    const OperationResult startResult = tunCore_->start(
        coreInfo,
        configPath,
        [this](const QString& line) { log(QStringLiteral("tun | %1").arg(line)); },
        [this](const QString& message) { log(QStringLiteral("TUN runtime started. %1").arg(message)); },
        [this](const QString& message) {
            log(QStringLiteral("TUN runtime failed to start: %1").arg(message));
            setStatus(QStringLiteral("TUN failed to start"));
        },
        [this](int exitCode, QProcess::ExitStatus, bool stopRequested) {
            if (!stopRequested && config_.tun().tunModeItem.enableTun) {
                log(QStringLiteral("TUN runtime exited unexpectedly: %1").arg(exitCode));
                setStatus(QStringLiteral("TUN stopped unexpectedly"));
            }
        });
    if (!startResult.success) {
        log(startResult.message);
        return false;
    }
    setStatus(relayToProxy ? QStringLiteral("TUN routing through proxy") : QStringLiteral("TUN device ready"));
    return true;
}

void SongBirdAutoCoordinator::stopTunRuntime(bool cleanupAdapter)
{
    if (tunCore_ != nullptr) {
        tunCore_->stop(true);
    }
    tunRuntimeRelayToProxy_ = false;
    if (cleanupAdapter && tunRuntimeService_ != nullptr) {
        const OperationResult cleanupResult = tunRuntimeService_->removeStaleAdapterIfPresent();
        log(cleanupResult.message);
    }
}

bool SongBirdAutoCoordinator::syncTunRuntimeForCurrentState()
{
    if (!config_.tun().tunModeItem.enableTun) {
        stopTunRuntime(true);
        setStatus(running_ ? QStringLiteral("TUN disabled") : QStringLiteral("Stopped"));
        return true;
    }
    return startTunRuntime(running_ && isProxySessionRunningOrTransitioning());
}

bool SongBirdAutoCoordinator::updateSystemProxyMode(SystemProxyMode mode) const
{
    return systemProxyService_ == nullptr
        || systemProxyService_->update(
            mode,
            config_.localPort + 1,
            config_.localPort,
            proxyExceptions(),
            config_.systemProxyAdvancedProtocol);
}

QString SongBirdAutoCoordinator::proxyExceptions() const
{
    return config_.defaults().defIeProxyExceptions.trimmed().isEmpty()
        ? kDefaultIeProxyExceptions
        : config_.defaults().defIeProxyExceptions.trimmed();
}

QString SongBirdAutoCoordinator::selectedCountryDisplayName() const
{
    for (const AutoCountrySummary& country : countrySummaries_) {
        if (country.countryCode == selectedCountryCode_) {
            return country.displayName.trimmed().isEmpty()
                ? autoCountryDisplayName(selectedCountryCode_)
                : country.displayName.trimmed();
        }
    }
    return autoCountryDisplayName(selectedCountryCode_);
}

void SongBirdAutoCoordinator::scheduleHealthCheck(int delayMs)
{
    if (!running_ || stopRequested_) {
        return;
    }
    healthCheckTimer_.start(delayMs);
}

void SongBirdAutoCoordinator::schedulePeriodicRefresh()
{
    if (!running_ || stopRequested_) {
        periodicRefreshTimer_.stop();
        return;
    }
    periodicRefreshTimer_.start(kPeriodicRefreshIntervalMs);
}

void SongBirdAutoCoordinator::runHealthCheck()
{
    if (!running_ || stopRequested_ || busy_ || healthCheckInProgress_) {
        scheduleHealthCheck(kHealthCheckIntervalMs);
        return;
    }

    ProxyAvailabilityCheckConfig checkConfig;
    checkConfig.localPort = config_.localPort;
    checkConfig.tunEnabled = false;
    checkConfig.speedPingTestUrl = config_.defaults().speedPingTestUrl;

    healthCheckInProgress_ = true;
    QPointer<SongBirdAutoCoordinator> self(this);
    QThread* thread = QThread::create([self, checkConfig]() {
        ProxyAvailabilityCheckService service;
        const OperationResult result = service.check(checkConfig);
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, result]() {
            if (self) {
                self->finishHealthCheck(result);
            }
        }, Qt::QueuedConnection);
    });
    trackThread(thread);
    thread->start();
}

void SongBirdAutoCoordinator::finishHealthCheck(const OperationResult& result)
{
    healthCheckInProgress_ = false;
    if (!running_ || stopRequested_) {
        return;
    }

    log(result.message);
    if (!result.success) {
        handleUnavailableActiveServer(result.message);
        return;
    }

    scheduleHealthCheck(kHealthCheckIntervalMs);
}

void SongBirdAutoCoordinator::handleUnavailableActiveServer(const QString& reason)
{
    log(QStringLiteral("Active node unavailable: %1").arg(reason));
    if (!switchToNextBestSameCountry(activeServerId_)) {
        scheduleHealthCheck(kHealthCheckIntervalMs);
    }
}

void SongBirdAutoCoordinator::clearEvaluationState()
{
    evaluations_.clear();
    countrySummaries_ = buildCountrySummaries(evaluations_);
    activeServerId_.clear();
    normalizeSelectedCountryAfterRetest();

    emit nodeEvaluationsChanged(evaluations_);
    emit countrySummariesChanged(countrySummaries_);
    emit activeServerChanged(QString(), QString(), QString(), QString(), -1);
}

void SongBirdAutoCoordinator::handleNoCompatibleServers()
{
    healthCheckTimer_.stop();
    clearEvaluationState();
    pendingStartAfterEvaluation_ = false;
    switchToBestAfterEvaluation_ = false;
    log(QStringLiteral("No compatible servers are available for automatic testing."));
    if (running_) {
        if (isProxySessionRunningOrTransitioning()) {
            log(QStringLiteral("Keeping the active proxy while waiting for compatible automatic nodes."));
        } else {
            updateSystemProxyMode(SystemProxyMode::ForcedClear);
        }
        setBusy(false);
        setStatus(QStringLiteral("Waiting for compatible servers"));
    } else {
        setBusy(false);
        setStatus(QStringLiteral("No compatible servers"));
    }
    schedulePeriodicRefresh();
}

void SongBirdAutoCoordinator::normalizeSelectedCountryAfterRetest()
{
    if (countrySummaries_.isEmpty()) {
        if (!selectedCountryCode_.isEmpty()) {
            selectedCountryCode_.clear();
            emit selectedCountryChanged(selectedCountryCode_);
        }
        return;
    }

    const auto selectedIt = std::find_if(
        countrySummaries_.cbegin(),
        countrySummaries_.cend(),
        [this](const AutoCountrySummary& country) {
            return country.countryCode == selectedCountryCode_;
        });
    if (selectedIt != countrySummaries_.cend()) {
        return;
    }

    selectedCountryCode_ = firstCountryWithNodesOrFirst(countrySummaries_);
    emit selectedCountryChanged(selectedCountryCode_);
    setStatus(QStringLiteral("Selected %1").arg(selectedCountryDisplayName()));
}

void SongBirdAutoCoordinator::maybeRefreshWhenCountryLow()
{
    if (!running_ || stopRequested_) {
        return;
    }
    if (!isSelectedCountryLow()) {
        return;
    }
    const int count = countAvailableAutoServersForCountry(evaluations_, selectedCountryCode_);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (lastLowCountryRefreshAt_.isValid()
        && lastLowCountryRefreshAt_.msecsTo(now) < kLowCountryRefreshCooldownMs) {
        return;
    }
    lastLowCountryRefreshAt_ = now;
    log(QStringLiteral("%1 has %2 usable node(s). Refreshing subscriptions.")
            .arg(selectedCountryCode_)
            .arg(count));
    if (busy_) {
        refreshQueuedAfterBusy_ = true;
        return;
    }
    updateSubscriptionsAndRetest();
}

bool SongBirdAutoCoordinator::isSelectedCountryLow() const
{
    return !selectedCountryCode_.isEmpty()
        && countAvailableAutoServersForCountry(evaluations_, selectedCountryCode_) <= kLowCountryNodeThreshold;
}

QString SongBirdAutoCoordinator::nextFailoverServerId(const QString& failedServerId) const
{
    if (!usesFirstAvailableStrategy()) {
        return bestAutoServerIdForCountry(evaluations_, selectedCountryCode_, failedServerId);
    }

    const QString normalizedCountryCode = normalizeAutoCountryCode(selectedCountryCode_);
    for (const AutoNodeEvaluation& evaluation : evaluations_) {
        if (!evaluation.available
            || autoCountryKeyForEvaluation(evaluation) != normalizedCountryCode
            || evaluation.indexId == failedServerId) {
            continue;
        }
        return evaluation.indexId;
    }
    return {};
}

bool SongBirdAutoCoordinator::switchToNextBestSameCountry(const QString& failedServerId)
{
    const int availableCount = countAvailableAutoServersForCountry(evaluations_, selectedCountryCode_);
    const QString nextId = nextFailoverServerId(failedServerId);
    refreshAfterFailover_ = running_ && availableCount <= kLowCountryNodeThreshold;

    if (nextId.isEmpty()) {
        log(QStringLiteral("No alternate node for %1. Retesting after subscription update.").arg(selectedCountryCode_));
        refreshAfterFailover_ = false;
        updateSubscriptionsAndRetest(true);
        return false;
    }

    if (selectServer(nextId)) {
        if (isProxySessionRunningOrTransitioning()) {
            pendingStartAfterStop_ = true;
            setBusy(true);
            setStatus(QStringLiteral("Switching proxy"));
            stopProxySession(false);
            return true;
        }
        startProxySession();
        return true;
    }

    refreshAfterFailover_ = false;
    return false;
}

void SongBirdAutoCoordinator::finishStopped()
{
    activeServerId_.clear();
    emit activeServerChanged(QString(), QString(), QString(), QString(), -1);
    if (config_.tun().tunModeItem.enableTun) {
        startTunRuntime(false);
    }
    setRunning(false);
    setBusy(false);
    setStatus(QStringLiteral("Stopped"));
}

void SongBirdAutoCoordinator::setBusy(bool busy)
{
    if (busy_ == busy) {
        return;
    }
    busy_ = busy;
    emit busyChanged(busy_);
    if (!busy_ && refreshQueuedAfterBusy_) {
        const bool forceUpdate = refreshQueuedAfterBusyForce_;
        refreshQueuedAfterBusy_ = false;
        refreshQueuedAfterBusyForce_ = false;
        QTimer::singleShot(0, this, [this, forceUpdate]() {
            if (!running_ || stopRequested_) {
                return;
            }
            if (busy_) {
                refreshQueuedAfterBusy_ = true;
                refreshQueuedAfterBusyForce_ = refreshQueuedAfterBusyForce_ || forceUpdate;
                return;
            }
            updateSubscriptionsAndRetest(forceUpdate);
        });
    } else if (!busy_ && running_ && pendingStartAfterEvaluation_) {
        QTimer::singleShot(0, this, [this]() {
            if (!running_ || stopRequested_ || busy_ || !pendingStartAfterEvaluation_) {
                return;
            }
            continueStartWithCurrentData(false, false, true);
        });
    }
}

void SongBirdAutoCoordinator::setRunning(bool running)
{
    if (running_ == running) {
        return;
    }
    running_ = running;
    emit runningChanged(running_);
}

void SongBirdAutoCoordinator::setStatus(const QString& status)
{
    emit statusMessageChanged(status);
}

void SongBirdAutoCoordinator::log(const QString& message)
{
    const QString trimmed = message.trimmed();
    if (!trimmed.isEmpty()) {
        emit logMessage(trimmed);
    }
}

void SongBirdAutoCoordinator::taskSummary(const QString& message)
{
    emit taskSummaryMessage(message.trimmed());
}

void SongBirdAutoCoordinator::trackThread(QThread* thread)
{
    if (thread == nullptr) {
        return;
    }
    workerThreads_.append(thread);
    QObject::connect(thread, &QThread::finished, this, [this, thread]() {
        workerThreads_.removeAll(thread);
    });
    QObject::connect(thread, &QThread::finished, thread, [thread]() {
        thread->deleteLater();
    });
}

void SongBirdAutoCoordinator::clearThreads()
{
    for (QThread* thread : std::as_const(workerThreads_)) {
        if (thread == nullptr) {
            continue;
        }
        thread->requestInterruption();
        thread->quit();
        thread->wait();
    }
    workerThreads_.clear();
}

quint64 SongBirdAutoCoordinator::nextOperationId()
{
    ++operationGeneration_;
    if (operationGeneration_ == 0) {
        operationGeneration_ = 1;
    }
    return operationGeneration_;
}

bool SongBirdAutoCoordinator::isCurrentOperation(quint64 operationId) const
{
    return operationId != 0 && operationGeneration_ == operationId;
}

void SongBirdAutoCoordinator::invalidateOperations()
{
    nextOperationId();
}
