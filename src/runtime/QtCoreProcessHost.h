#pragma once

#include <QProcess>

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
        ExitedCallback exited = {}) override;

    OperationResult stop() override;
    OperationResult reload() override;
    bool isRunning() const override;

private:
    QStringList buildArguments(const CoreInfo& coreInfo, const QString& configFilePath) const;
    void bindOutputSignals();
    void emitBufferedOutput(QProcess::ProcessChannel channel);
    void flushBufferedOutput(bool flushPartialLines);

    std::unique_ptr<QProcess> process_;
    CoreInfo lastCoreInfo_;
    QString lastConfigFilePath_;
    std::function<void(const QString&)> outputReceived_;
    ExitedCallback exitedCallback_;
    bool stopRequested_ = false;
    QString standardOutputBuffer_;
    QString standardErrorBuffer_;
};
