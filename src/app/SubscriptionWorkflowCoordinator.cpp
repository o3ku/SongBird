#include "app/SubscriptionWorkflowCoordinator.h"

#include <utility>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>

#include "persistence/JsonConfigRepository.h"
#include "services/ServerService.h"
#include "services/SubscriptionService.h"
#include "services/SubscriptionUpdateService.h"
#include "subscription/CustomConfigTextParser.h"
#include "subscription/SubscriptionImportTextParser.h"
#include "subscription/SubscriptionUrlImportService.h"

namespace {

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

} // namespace

SubscriptionWorkflowCoordinator::SubscriptionWorkflowCoordinator(
    Dependencies dependencies,
    QObject* parent)
    : QObject(parent)
    , backgroundTasks_(dependencies.backgroundTasks)
    , configCallbacks_(std::move(dependencies.callbacks.config))
    , uiCallbacks_(std::move(dependencies.callbacks.ui))
    , threadCallbacks_(std::move(dependencies.callbacks.thread))
    , workflowCallbacks_(std::move(dependencies.callbacks.workflow))
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
    if (uiCallbacks_.setSubscriptionUpdateRunning) {
        uiCallbacks_.setSubscriptionUpdateRunning(true);
    }

    const QString configPath = configCallbacks_.configPath ? configCallbacks_.configPath() : QString();
    const QString customConfigDirectory = configCallbacks_.customConfigDirectory
        ? configCallbacks_.customConfigDirectory()
        : QString();
    QObject* uiContext = resolvedUiContext();
    const std::weak_ptr<char> lifetimeGuard = resolvedLifetimeGuard();
    QPointer<SubscriptionWorkflowCoordinator> self(this);
    QThread* thread = QThread::create([self, text, configPath, customConfigDirectory, uiContext, token, lifetimeGuard]() {
        JsonConfigRepository repository(configPath);
        SubscriptionService subscriptionService(repository);
        ServerService serverService(repository, customConfigDirectory);
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
    if (threadCallbacks_.trackBackgroundThread) {
        threadCallbacks_.trackBackgroundThread(thread);
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

    const QString activeSubscriptionId = configCallbacks_.activeSubscriptionId
        ? configCallbacks_.activeSubscriptionId().trimmed()
        : QString();

    emitStartupMessage(QCoreApplication::translate(
        "AppBootstrap",
        "Importing subscription URLs from the clipboard in the background..."));

    if (uiCallbacks_.clearServerTestResultsAndSync) {
        uiCallbacks_.clearServerTestResultsAndSync();
    }
    if (uiCallbacks_.setSubscriptionUpdateRunning) {
        uiCallbacks_.setSubscriptionUpdateRunning(true);
    }

    const QString configPath = configCallbacks_.configPath ? configCallbacks_.configPath() : QString();
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
    if (threadCallbacks_.trackBackgroundThread) {
        threadCallbacks_.trackBackgroundThread(thread);
    } else {
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    }
    thread->start();
}

void SubscriptionWorkflowCoordinator::updateAll()
{
    if (backgroundTasks_.isKindActive(BackgroundTaskCoordinator::Kind::SubscriptionUpdate)) {
        if (uiCallbacks_.appendResult) {
            uiCallbacks_.appendResult(OperationResult::fail(
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
    const Config config = configCallbacks_.currentConfig ? configCallbacks_.currentConfig() : Config();
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

    const QString activeSubscriptionId = configCallbacks_.activeSubscriptionId
        ? configCallbacks_.activeSubscriptionId().trimmed()
        : QString();

    emitStartupMessage(startupMessage);

    if (uiCallbacks_.clearServerTestResultsAndSync) {
        uiCallbacks_.clearServerTestResultsAndSync();
    }
    if (uiCallbacks_.setSubscriptionUpdateRunning) {
        uiCallbacks_.setSubscriptionUpdateRunning(true);
    }

    const QString configPath = configCallbacks_.configPath ? configCallbacks_.configPath() : QString();
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
    if (threadCallbacks_.trackBackgroundThread) {
        threadCallbacks_.trackBackgroundThread(thread);
    } else {
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    }
    thread->start();
}

BackgroundTaskCoordinator::Token SubscriptionWorkflowCoordinator::beginSubscriptionTask(bool reportAlreadyRunning)
{
    if (reportAlreadyRunning
        && backgroundTasks_.isKindActive(BackgroundTaskCoordinator::Kind::SubscriptionUpdate)
        && uiCallbacks_.appendResult) {
        uiCallbacks_.appendResult(OperationResult::fail(
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
    if (uiCallbacks_.setSubscriptionUpdateRunning) {
        uiCallbacks_.setSubscriptionUpdateRunning(false);
    }
    if (completion.reloadConfig && configCallbacks_.reloadConfig) {
        configCallbacks_.reloadConfig();
    }
    if (uiCallbacks_.appendResult) {
        uiCallbacks_.appendResult(completion.result);
    }
    if (completion.syncWindow && uiCallbacks_.syncWindow) {
        uiCallbacks_.syncWindow();
    }
    if (completion.selectLastSubscription && uiCallbacks_.selectSubscriptionTab) {
        uiCallbacks_.selectSubscriptionTab(completion.lastSubscriptionId);
    }
    if (completion.restartActiveSubscription && workflowCallbacks_.restartCoreIfRunning) {
        workflowCallbacks_.restartCoreIfRunning(QStringLiteral("Reloading core after updating subscriptions."), true);
    }
}

void SubscriptionWorkflowCoordinator::emitStartupMessage(const QString& message)
{
    if (uiCallbacks_.showStartupMessage) {
        uiCallbacks_.showStartupMessage(message);
    }
}

QObject* SubscriptionWorkflowCoordinator::resolvedUiContext() const
{
    if (uiCallbacks_.context) {
        QObject* context = uiCallbacks_.context();
        if (context != nullptr) {
            return context;
        }
    }
    return QCoreApplication::instance();
}

std::weak_ptr<char> SubscriptionWorkflowCoordinator::resolvedLifetimeGuard() const
{
    if (uiCallbacks_.lifetimeGuard) {
        return uiCallbacks_.lifetimeGuard();
    }

    static const std::shared_ptr<char> fallbackGuard = std::make_shared<char>();
    return fallbackGuard;
}
