#pragma once

#include <functional>
#include <memory>

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include "appcore/BackgroundTaskCoordinator.h"
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
        // One reason per subscription id that failed in this batch, empty when the run
        // succeeded. Carried in the completion rather than recomputed on the UI thread,
        // because the message it is derived from describes the worker's config, not the copy
        // the window is about to reload.
        QHash<QString, QString> subscriptionFailures;
        bool reloadConfig = false;
        bool syncWindow = false;
        bool restartActiveSubscription = false;
        bool selectLastSubscription = false;
    };

    struct ConfigCallbacks {
        std::function<QString()> configPath;
        std::function<Config()> currentConfig;
        std::function<QString()> activeSubscriptionId;
        std::function<QString()> customConfigDirectory;
        std::function<void()> reloadConfig;
    };

    struct UiCallbacks {
        std::function<QObject*()> context;
        std::function<std::weak_ptr<char>()> lifetimeGuard;
        std::function<void(const QString&)> showStartupMessage;
        std::function<void(bool)> setSubscriptionUpdateRunning;
        std::function<void()> clearServerTestResultsAndSync;
        std::function<void(const OperationResult&)> appendResult;
        std::function<void()> syncWindow;
        // The last import/update left nothing to list, and why. That reason is the
        // only thing that distinguishes "you have not imported anything yet" from
        // "your import failed", and the empty server panel is where the user is
        // looking. An empty reason clears it, which is what an import that put
        // servers back on the table does; a failure keeps the previous reason
        // until one of those happens.
        std::function<void(const QString&)> reportImportFailure;
        std::function<void(const QString&)> selectSubscriptionTab;
        // Replaces the per-subscription failure reasons the tab hover text shows. Called with the
        // whole map on every batch, so a subscription that succeeded stops naming an old reason.
        std::function<void(const QHash<QString, QString>&)> setSubscriptionUpdateFailures;
    };

    struct ThreadCallbacks {
        std::function<void(QThread*)> trackBackgroundThread;
    };

    struct WorkflowCallbacks {
        std::function<void(const QString&, bool)> restartCoreIfRunning;
    };

    struct Callbacks {
        ConfigCallbacks config;
        UiCallbacks ui;
        ThreadCallbacks thread;
        WorkflowCallbacks workflow;
    };

    struct Dependencies {
        BackgroundTaskCoordinator& backgroundTasks;
        Callbacks callbacks;
    };

    SubscriptionWorkflowCoordinator(
        Dependencies dependencies,
        QObject* parent = nullptr);

    void importClipboard(const QString& text);
    void importSubscriptionUrls(const QString& text);
    void updateAll();
    // Takes ids, not rows: the settings dialog's rows are reorderable, so a row number captured
    // when the button was pressed addresses a different subscription by the time the list is saved.
    void updateSelected(const QStringList& subscriptionIds);
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
    ConfigCallbacks configCallbacks_;
    UiCallbacks uiCallbacks_;
    ThreadCallbacks threadCallbacks_;
    WorkflowCallbacks workflowCallbacks_;
};
