#include "app/SubscriptionWorkflowCoordinator.h"

#include <utility>

#include <QCoreApplication>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QThread>

#include "persistence/JsonConfigRepository.h"
#include "services/SubscriptionService.h"
#include "services/SubscriptionUpdateService.h"
#include "subscription/SubscriptionImportTextParser.h"
#include "subscription/SubscriptionUrlImportService.h"

SubscriptionWorkflowCoordinator::SubscriptionWorkflowCoordinator(
    BackgroundTaskCoordinator& backgroundTasks,
    Callbacks callbacks,
    QObject* parent)
    : QObject(parent)
    , backgroundTasks_(backgroundTasks)
    , callbacks_(std::move(callbacks))
{
}

void SubscriptionWorkflowCoordinator::importClipboard(const QString& text)
{
    for (const QString& line : SubscriptionImportTextParser::nonEmptyLines(text)) {
        if (SubscriptionImportTextParser::isSubscriptionUrl(line)) {
            importSubscriptionUrls(text);
            return;
        }
    }

    const BackgroundTaskCoordinator::Token token = beginSubscriptionTask(false);
    if (!token.isValid()) {
        return;
    }

    emitStartupMessage(QCoreApplication::translate(
        "AppBootstrap",
        "Importing clipboard content in the background..."));
    if (callbacks_.setSubscriptionUpdateRunning) {
        callbacks_.setSubscriptionUpdateRunning(true);
    }

    const QString configPath = callbacks_.configPath ? callbacks_.configPath() : QString();
    QObject* uiContext = resolvedUiContext();
    const std::weak_ptr<char> lifetimeGuard = resolvedLifetimeGuard();
    const auto importCustomConfigText = callbacks_.importCustomConfigText;
    QPointer<SubscriptionWorkflowCoordinator> self(this);
    QThread* thread = QThread::create([self, text, configPath, uiContext, token, lifetimeGuard, importCustomConfigText]() {
        JsonConfigRepository repository(configPath);
        SubscriptionService subscriptionService(repository);
        QNetworkAccessManager networkAccessManager;
        SubscriptionUpdateService subscriptionUpdateService(repository, subscriptionService, networkAccessManager);
        Config workerConfig = repository.load();

        OperationResult result = subscriptionUpdateService.importFromText(workerConfig, text);
        if (!result.success && importCustomConfigText) {
            const OperationResult customImportResult = importCustomConfigText(text, workerConfig);
            if (customImportResult.success) {
                result = customImportResult;
            }
        }

        Completion completion;
        completion.result = result;
        completion.reloadConfig = result.success;
        completion.syncWindow = result.success;

        QMetaObject::invokeMethod(uiContext, [self, token, lifetimeGuard, completion]() {
            if (self) {
                self->finishOnUiThread(token, lifetimeGuard, completion);
            }
        }, Qt::QueuedConnection);
    });
    if (callbacks_.trackBackgroundThread) {
        callbacks_.trackBackgroundThread(thread);
    } else {
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    }
    thread->start();
}

void SubscriptionWorkflowCoordinator::importSubscriptionUrls(const QString& text)
{
    const BackgroundTaskCoordinator::Token token = beginSubscriptionTask(false);
    if (!token.isValid()) {
        return;
    }

    const QString activeSubscriptionId = callbacks_.activeSubscriptionId
        ? callbacks_.activeSubscriptionId().trimmed()
        : QString();

    emitStartupMessage(QCoreApplication::translate(
        "AppBootstrap",
        "Importing subscription URLs from the clipboard in the background..."));

    if (callbacks_.clearServerTestResultsAndSync) {
        callbacks_.clearServerTestResultsAndSync();
    }
    if (callbacks_.setSubscriptionUpdateRunning) {
        callbacks_.setSubscriptionUpdateRunning(true);
    }

    const QString configPath = callbacks_.configPath ? callbacks_.configPath() : QString();
    QObject* uiContext = resolvedUiContext();
    const std::weak_ptr<char> lifetimeGuard = resolvedLifetimeGuard();
    QPointer<SubscriptionWorkflowCoordinator> self(this);
    QThread* thread = QThread::create([self, text, configPath, activeSubscriptionId, uiContext, token, lifetimeGuard]() {
        JsonConfigRepository repository(configPath);
        SubscriptionService subscriptionService(repository);
        QNetworkAccessManager networkAccessManager;
        SubscriptionUpdateService subscriptionUpdateService(repository, subscriptionService, networkAccessManager);
        SubscriptionUrlImportService importService(
            repository,
            subscriptionService,
            [&subscriptionUpdateService](Config& config, const QStringList& subscriptionIds) {
                return subscriptionUpdateService.updateByIds(config, subscriptionIds);
            });
        Config workerConfig = repository.load();

        SubscriptionUrlImportService::ImportPlan plan;
        const OperationResult result = importService.importAndUpdate(workerConfig, text, &plan);

        Completion completion;
        completion.result = result;
        completion.subscriptionIds = plan.subscriptionIds;
        completion.activeSubscriptionId = activeSubscriptionId;
        completion.lastSubscriptionId = plan.lastSubscriptionId;
        completion.reloadConfig = true;
        completion.syncWindow = true;
        completion.selectLastSubscription = !plan.lastSubscriptionId.trimmed().isEmpty();
        completion.restartActiveSubscription = result.success
            && !activeSubscriptionId.isEmpty()
            && plan.subscriptionIds.contains(activeSubscriptionId);

        QMetaObject::invokeMethod(uiContext, [self, token, lifetimeGuard, completion]() {
            if (self) {
                self->finishOnUiThread(token, lifetimeGuard, completion);
            }
        }, Qt::QueuedConnection);
    });
    if (callbacks_.trackBackgroundThread) {
        callbacks_.trackBackgroundThread(thread);
    } else {
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    }
    thread->start();
}

void SubscriptionWorkflowCoordinator::updateAll()
{
    if (backgroundTasks_.isKindActive(BackgroundTaskCoordinator::Kind::SubscriptionUpdate)) {
        if (callbacks_.appendResult) {
            callbacks_.appendResult(OperationResult::fail(
                QStringLiteral("A subscription update is already running in the background.")));
        }
        return;
    }

    updateByIds(
        QStringList(),
        false,
        QCoreApplication::translate("AppBootstrap", "Updating subscriptions in the background..."));
}

void SubscriptionWorkflowCoordinator::updateSelected(const QList<int>& rowIndexes)
{
    const Config config = callbacks_.currentConfig ? callbacks_.currentConfig() : Config();
    QStringList subscriptionIds;
    for (int rowIndex : rowIndexes) {
        if (rowIndex < 0 || rowIndex >= config.collection().subscriptions.size()) {
            continue;
        }

        const QString subscriptionId = config.collection().subscriptions.at(rowIndex).id.trimmed();
        if (!subscriptionId.isEmpty() && !subscriptionIds.contains(subscriptionId)) {
            subscriptionIds.append(subscriptionId);
        }
    }

    updateByIds(
        subscriptionIds,
        false,
        QCoreApplication::translate("AppBootstrap", "Updating selected subscriptions in the background..."));
}

void SubscriptionWorkflowCoordinator::updateByIds(
    const QStringList& subscriptionIds,
    bool useProxy,
    const QString& startupMessage)
{
    const BackgroundTaskCoordinator::Token token = beginSubscriptionTask(false);
    if (!token.isValid()) {
        return;
    }

    const QString activeSubscriptionId = callbacks_.activeSubscriptionId
        ? callbacks_.activeSubscriptionId().trimmed()
        : QString();

    emitStartupMessage(startupMessage);

    if (callbacks_.clearServerTestResultsAndSync) {
        callbacks_.clearServerTestResultsAndSync();
    }
    if (callbacks_.setSubscriptionUpdateRunning) {
        callbacks_.setSubscriptionUpdateRunning(true);
    }

    const QString configPath = callbacks_.configPath ? callbacks_.configPath() : QString();
    QObject* uiContext = resolvedUiContext();
    const std::weak_ptr<char> lifetimeGuard = resolvedLifetimeGuard();
    QPointer<SubscriptionWorkflowCoordinator> self(this);
    QThread* thread = QThread::create([self, configPath, subscriptionIds, activeSubscriptionId, uiContext, useProxy, token, lifetimeGuard]() {
        JsonConfigRepository repository(configPath);
        SubscriptionService subscriptionService(repository);
        QNetworkAccessManager networkAccessManager;
        SubscriptionUpdateService subscriptionUpdateService(repository, subscriptionService, networkAccessManager);
        Config workerConfig = repository.load();
        const OperationResult result = subscriptionIds.isEmpty()
            ? subscriptionUpdateService.updateAll(workerConfig, useProxy)
            : subscriptionUpdateService.updateByIds(workerConfig, subscriptionIds, useProxy);

        Completion completion;
        completion.result = result;
        completion.subscriptionIds = subscriptionIds;
        completion.activeSubscriptionId = activeSubscriptionId;
        completion.reloadConfig = true;
        completion.syncWindow = true;
        completion.restartActiveSubscription = result.success
            && !activeSubscriptionId.isEmpty()
            && (subscriptionIds.isEmpty() || subscriptionIds.contains(activeSubscriptionId));

        QMetaObject::invokeMethod(uiContext, [self, token, lifetimeGuard, completion]() {
            if (self) {
                self->finishOnUiThread(token, lifetimeGuard, completion);
            }
        }, Qt::QueuedConnection);
    });
    if (callbacks_.trackBackgroundThread) {
        callbacks_.trackBackgroundThread(thread);
    } else {
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    }
    thread->start();
}

BackgroundTaskCoordinator::Token SubscriptionWorkflowCoordinator::beginSubscriptionTask(bool reportAlreadyRunning)
{
    if (reportAlreadyRunning
        && backgroundTasks_.isKindActive(BackgroundTaskCoordinator::Kind::SubscriptionUpdate)
        && callbacks_.appendResult) {
        callbacks_.appendResult(OperationResult::fail(
            QStringLiteral("A subscription update is already running in the background.")));
        return {};
    }

    const BackgroundTaskCoordinator::Token token =
        backgroundTasks_.tryBeginUserTask(BackgroundTaskCoordinator::Kind::SubscriptionUpdate);
    return token;
}

void SubscriptionWorkflowCoordinator::finishOnUiThread(
    const BackgroundTaskCoordinator::Token& token,
    const std::weak_ptr<char>& lifetimeGuard,
    Completion completion)
{
    if (lifetimeGuard.expired()) {
        return;
    }
    if (!backgroundTasks_.isCurrent(token)) {
        return;
    }

    backgroundTasks_.finish(token);
    if (callbacks_.setSubscriptionUpdateRunning) {
        callbacks_.setSubscriptionUpdateRunning(false);
    }
    if (completion.reloadConfig && callbacks_.reloadConfig) {
        callbacks_.reloadConfig();
    }
    if (callbacks_.appendResult) {
        callbacks_.appendResult(completion.result);
    }
    if (completion.syncWindow && callbacks_.syncWindow) {
        callbacks_.syncWindow();
    }
    if (completion.selectLastSubscription && callbacks_.selectSubscriptionTab) {
        callbacks_.selectSubscriptionTab(completion.lastSubscriptionId);
    }
    if (completion.restartActiveSubscription && callbacks_.restartCoreIfRunning) {
        callbacks_.restartCoreIfRunning(QStringLiteral("Reloading core after updating subscriptions."), true);
    }
}

void SubscriptionWorkflowCoordinator::emitStartupMessage(const QString& message)
{
    if (callbacks_.showStartupMessage) {
        callbacks_.showStartupMessage(message);
    }
}

QObject* SubscriptionWorkflowCoordinator::resolvedUiContext() const
{
    if (callbacks_.uiContext) {
        QObject* context = callbacks_.uiContext();
        if (context != nullptr) {
            return context;
        }
    }
    return QCoreApplication::instance();
}

std::weak_ptr<char> SubscriptionWorkflowCoordinator::resolvedLifetimeGuard() const
{
    if (callbacks_.lifetimeGuard) {
        return callbacks_.lifetimeGuard();
    }

    static const std::shared_ptr<char> fallbackGuard = std::make_shared<char>();
    return fallbackGuard;
}
