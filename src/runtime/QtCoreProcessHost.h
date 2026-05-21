#pragma once

#include <QProcess>
#include <QTimer>

#include <functional>
#include <memory>

#include "runtime/ICoreProcessHost.h"

class QtCoreProcessHost final : public ICoreProcessHost {
public:
    QtCoreProcessHost();
    ~QtCoreProcessHost() override;

    OperationResult start(
        const CoreInfo& coreInfo,
        const QString& configFilePath,
        std::function<void(const QString&)> outputReceived,
        StartedCallback started = {},
        StartFailedCallback startFailed = {},
        ExitedCallback exited = {}) override;

    OperationResult stop(bool immediate = false) override;
    OperationResult reload() override;
    bool isRunning() const override;

private:
    enum class ProcessCleanupMode {
        DeleteNow,
        DeleteLater
    };

    QStringList buildArguments(const CoreInfo& coreInfo, const QString& configFilePath) const;
    void bindOutputSignals();
    void emitBufferedOutput(QProcess::ProcessChannel channel);
    void flushBufferedOutput(bool flushPartialLines);
    void resetProcessState(ProcessCleanupMode cleanupMode = ProcessCleanupMode::DeleteNow);
    void scheduleForcedKill();

    std::unique_ptr<QProcess> process_;
    CoreInfo lastCoreInfo_;
    QString lastConfigFilePath_;
    std::function<void(const QString&)> outputReceived_;
    StartedCallback startedCallback_;
    StartFailedCallback startFailedCallback_;
    ExitedCallback exitedCallback_;
    bool stopRequested_ = false;
    bool startNotified_ = false;
    QString standardOutputBuffer_;
    QString standardErrorBuffer_;
    QTimer* forcedKillTimer_ = nullptr;
};
