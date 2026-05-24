#pragma once

#include <functional>
#include <memory>

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include "app/BackgroundTaskCoordinator.h"
#include "common/OperationResult.h"
#include "domain/models/Config.h"

class QThread;

class SubscriptionWorkflowCoordinator final : public QObject {
    Q_OBJECT

public:
    struct Completion {
        OperationResult result;
        QStringList subscriptionIds;
        QString activeSubscriptionId;
        QString lastSubscriptionId;
        bool reloadConfig = false;
        bool syncWindow = false;
        bool restartActiveSubscription = false;
        bool selectLastSubscription = false;
    };

    struct Callbacks {
        std::function<QString()> configPath;
        std::function<Config()> currentConfig;
        std::function<QString()> activeSubscriptionId;
        std::function<QObject*()> uiContext;
        std::function<std::weak_ptr<char>()> lifetimeGuard;
        std::function<void(QThread*)> trackBackgroundThread;
        std::function<void(const QString&)> showStartupMessage;
        std::function<void(bool)> setSubscriptionUpdateRunning;
        std::function<void()> clearServerTestResultsAndSync;
        std::function<void(const OperationResult&)> appendResult;
        std::function<void()> reloadConfig;
        std::function<void()> syncWindow;
        std::function<void(const QString&)> selectSubscriptionTab;
        std::function<void(const QString&, bool)> restartCoreIfRunning;
        std::function<OperationResult(const QString&, Config&)> importCustomConfigText;
    };

    SubscriptionWorkflowCoordinator(
        BackgroundTaskCoordinator& backgroundTasks,
        Callbacks callbacks,
        QObject* parent = nullptr);

    void importClipboard(const QString& text);
    void importSubscriptionUrls(const QString& text);
    void updateAll();
    void updateSelected(const QList<int>& rowIndexes);
    void updateByIds(const QStringList& subscriptionIds, bool useProxy, const QString& startupMessage);

private:
    BackgroundTaskCoordinator::Token beginSubscriptionTask(bool reportAlreadyRunning);
    void finishOnUiThread(
        const BackgroundTaskCoordinator::Token& token,
        const std::weak_ptr<char>& lifetimeGuard,
        Completion completion);
    void emitStartupMessage(const QString& message);
    QObject* resolvedUiContext() const;
    std::weak_ptr<char> resolvedLifetimeGuard() const;

    BackgroundTaskCoordinator& backgroundTasks_;
    Callbacks callbacks_;
};
