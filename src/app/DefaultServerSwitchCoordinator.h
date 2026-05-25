#pragma once

#include <functional>
#include <memory>

#include <QObject>
#include <QString>

#include "common/OperationResult.h"

class DefaultServerSwitchCoordinator final : public QObject {
public:
    struct Callbacks {
        std::function<QString()> currentIndexId;
        std::function<bool()> isCoreRunning;
        std::function<bool()> isTunEnabled;
        std::function<bool()> isShuttingDown;
        std::function<QObject*()> uiContext;
        std::function<std::weak_ptr<char>()> lifetimeGuard;
        std::function<OperationResult(const QString&)> setDefaultServer;
        std::function<void(const OperationResult&)> appendResult;
        std::function<void()> syncWindow;
        std::function<void()> clearServerWarning;
        std::function<void(bool)> setCurrentActivationPending;
        std::function<void(const QString&, bool)> switchRunningCoreToServer;
        std::function<void(const QString&, bool)> restartCoreIfRunning;
        std::function<void(bool)> enableSystemProxy;
        std::function<void(bool)> setTunEnabled;
        std::function<void(bool)> startProxyAfterSwitch;
    };

    explicit DefaultServerSwitchCoordinator(Callbacks callbacks, QObject* parent = nullptr);

    void setDefaultServer(const QString& indexId);
    void setDefaultServerWithTun(const QString& indexId);
    bool requestSwitchAfterCoreStop(const QString& indexId, bool enableTun);
    void scheduleSwitchAfterCoreStopped(const QString& indexId, bool enableTun, bool showStartupOverlay);
    void switchAfterCoreStopped(const QString& indexId, bool enableTun, bool showStartupOverlay);

private:
    QString currentIndexId() const;
    bool isCoreRunning() const;
    bool isTunEnabled() const;
    bool isShuttingDown() const;
    QObject* resolvedUiContext() const;
    std::weak_ptr<char> resolvedLifetimeGuard() const;
    OperationResult setDefaultServerOnConfig(const QString& indexId) const;
    void appendResult(const OperationResult& result) const;
    void syncWindow() const;
    void clearServerWarning() const;
    void setCurrentActivationPending(bool pending) const;

    Callbacks callbacks_;
};
