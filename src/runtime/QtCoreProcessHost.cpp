#include "runtime/QtCoreProcessHost.h"

#include <QDir>
#include <QFileInfo>
#include <QPointer>

#include <utility>

#include "common/CorePidFile.h"

QtCoreProcessHost::QtCoreProcessHost() = default;

QtCoreProcessHost::~QtCoreProcessHost()
{
    stop(true);
    resetProcessState();
    processHandle_.reset();
}

OperationResult QtCoreProcessHost::start(
    const CoreInfo& coreInfo,
    const QString& configFilePath,
    std::function<void(const QString&)> outputReceived,
    StartedCallback started,
    StartFailedCallback startFailed,
    ExitedCallback exited)
{
    if (coreInfo.program.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Core executable path is empty."));
    }

    if (!QFileInfo::exists(coreInfo.program)) {
        return OperationResult::fail(QStringLiteral("Core executable was not found: %1").arg(coreInfo.program));
    }

    OperationResult stopResult = stop(true);
    if (!stopResult.success && isRunning()) {
        return stopResult;
    }

    process_ = new QProcess();
    processHandle_ = std::make_shared<ProcessHandle>();
    processHandle_->process = process_;
    outputReceived_ = std::move(outputReceived);
    startedCallback_ = std::move(started);
    startFailedCallback_ = std::move(startFailed);
    exitedCallback_ = std::move(exited);
    stopRequested_ = false;
    startNotified_ = false;
    lastCoreInfo_ = coreInfo;
    lastConfigFilePath_ = configFilePath;
    outputBuffer_.clear();

    process_->setProgram(coreInfo.program);
    process_->setArguments(buildArguments(coreInfo, configFilePath));
    process_->setWorkingDirectory(
        coreInfo.workingDirectory.trimmed().isEmpty()
            ? QFileInfo(coreInfo.program).absolutePath()
            : coreInfo.workingDirectory);
    process_->setProcessChannelMode(coreInfo.captureOutput
        ? QProcess::SeparateChannels
        : QProcess::ForwardedChannels);

    bindOutputSignals();

    process_->start();
    const QString programName = QFileInfo(coreInfo.program).fileName();
    if (coreInfo.asyncStart) {
        return OperationResult::ok(
            QStringLiteral("Launching core process: %1").arg(programName));
    }

    return OperationResult::ok(
        QStringLiteral("Launching core process: %1").arg(programName));
}

OperationResult QtCoreProcessHost::stop(bool immediate)
{
    if (!process_) {
        return OperationResult::ok(QStringLiteral("Core process is not running."));
    }

    stopRequested_ = true;
    if (process_->state() == QProcess::NotRunning) {
        flushBufferedOutput(true);
        resetProcessState();
        return OperationResult::ok(QStringLiteral("Core process stopped."));
    }

    process_->terminate();
    if (immediate) {
        if (!process_->waitForFinished(1500)) {
            process_->kill();
            if (!process_->waitForFinished(1500)) {
                return OperationResult::fail(QStringLiteral("Timed out while stopping the core process."));
            }
        }
        return OperationResult::ok(QStringLiteral("Stopping core process immediately..."));
    }

    scheduleForcedKill();
    return OperationResult::ok(QStringLiteral("Stopping core process..."));
}

OperationResult QtCoreProcessHost::reload()
{
    if (lastCoreInfo_.program.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("No core process has been started yet."));
    }

    return start(lastCoreInfo_, lastConfigFilePath_, outputReceived_, startedCallback_, startFailedCallback_, exitedCallback_);
}

bool QtCoreProcessHost::isRunning() const
{
    return process_ && process_->state() != QProcess::NotRunning;
}

QStringList QtCoreProcessHost::buildArguments(const CoreInfo& coreInfo, const QString& configFilePath) const
{
    QStringList arguments = coreInfo.arguments;

    if (!configFilePath.trimmed().isEmpty()) {
        bool replacedPlaceholder = false;
        for (QString& argument : arguments) {
            if (argument.contains(coreInfo.configPlaceholder)) {
                argument.replace(coreInfo.configPlaceholder, QDir::toNativeSeparators(configFilePath));
                replacedPlaceholder = true;
            }
        }

        if (!replacedPlaceholder && coreInfo.appendConfigArgument) {
            arguments << QStringLiteral("-config") << QDir::toNativeSeparators(configFilePath);
        }
    }

    return arguments;
}

void QtCoreProcessHost::bindOutputSignals()
{
    QProcess* process = process_.data();
    std::weak_ptr<ProcessHandle> processHandle = processHandle_;
    if (lastCoreInfo_.captureOutput) {
        QObject::connect(process, &QProcess::readyReadStandardOutput, process, [this, process, processHandle]() {
            const std::shared_ptr<ProcessHandle> handle = processHandle.lock();
            if (!handle || handle->process != process) {
                return;
            }
            emitBufferedOutput(QProcess::StandardOutput);
        });

        QObject::connect(process, &QProcess::readyReadStandardError, process, [this, process, processHandle]() {
            const std::shared_ptr<ProcessHandle> handle = processHandle.lock();
            if (!handle || handle->process != process) {
                return;
            }
            emitBufferedOutput(QProcess::StandardError);
        });
    }

    QObject::connect(
        process,
        &QProcess::started,
        process,
        [this, process, processHandle]() {
            const std::shared_ptr<ProcessHandle> handle = processHandle.lock();
            if (!handle || handle->process != process) {
                return;
            }
            recordCorePid(process->processId());

            if (startNotified_) {
                return;
            }

            startNotified_ = true;
            if (startedCallback_) {
                startedCallback_(QStringLiteral("Core process started: %1")
                                     .arg(QFileInfo(lastCoreInfo_.program).fileName()));
            }
        });

    QObject::connect(
        process,
        &QProcess::errorOccurred,
        process,
        [this, process, processHandle](QProcess::ProcessError error) {
            const std::shared_ptr<ProcessHandle> handle = processHandle.lock();
            if (!handle || handle->process != process) {
                return;
            }
            if (startNotified_ || error != QProcess::FailedToStart) {
                return;
            }

            startNotified_ = true;
            if (startFailedCallback_) {
                startFailedCallback_(
                    QStringLiteral("Failed to start core process: %1").arg(process->errorString()));
            }
        });

    QObject::connect(
        process,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        process,
        [this, process, processHandle](int exitCode, QProcess::ExitStatus status) {
            const std::shared_ptr<ProcessHandle> handle = processHandle.lock();
            if (!handle || handle->process != process) {
                return;
            }
            removeCorePid(process->processId());
            if (forcedKillTimer_ != nullptr) {
                forcedKillTimer_->stop();
            }
            flushBufferedOutput(true);
            const ExitedCallback exitedCallback = std::move(exitedCallback_);
            const bool stopRequested = stopRequested_;
            resetProcessState(ProcessCleanupMode::DeleteLater);
            if (exitedCallback) {
                exitedCallback(exitCode, status, stopRequested);
            }
        });
}

void QtCoreProcessHost::emitBufferedOutput(QProcess::ProcessChannel channel)
{
    if (!process_ || !outputReceived_) {
        return;
    }

    outputBuffer_.append(
        channel == QProcess::StandardOutput
            ? CoreProcessOutputBuffer::Channel::StandardOutput
            : CoreProcessOutputBuffer::Channel::StandardError,
        channel == QProcess::StandardOutput
            ? process_->readAllStandardOutput()
            : process_->readAllStandardError(),
        outputReceived_);
}

void QtCoreProcessHost::flushBufferedOutput(bool flushPartialLines)
{
    outputBuffer_.flush(flushPartialLines, outputReceived_);
}

void QtCoreProcessHost::resetProcessState(ProcessCleanupMode cleanupMode)
{
    if (forcedKillTimer_ != nullptr) {
        forcedKillTimer_->stop();
        forcedKillTimer_->deleteLater();
        forcedKillTimer_ = nullptr;
    }

    if (cleanupMode == ProcessCleanupMode::DeleteLater && process_) {
        processHandle_->process.clear();
        process_->deleteLater();
        process_.clear();
    } else {
        processHandle_->process.clear();
        delete process_.data();
        process_.clear();
    }
    startedCallback_ = {};
    startFailedCallback_ = {};
    exitedCallback_ = {};
    startNotified_ = false;
    stopRequested_ = false;
    outputBuffer_.clear();
}

void QtCoreProcessHost::scheduleForcedKill()
{
    if (!process_) {
        return;
    }

    if (forcedKillTimer_ == nullptr) {
        forcedKillTimer_ = new QTimer(process_.data());
        forcedKillTimer_->setSingleShot(true);
        QProcess* process = process_.data();
        QObject::connect(forcedKillTimer_, &QTimer::timeout, process, [process]() {
            if (process->state() != QProcess::NotRunning) {
                process->kill();
            }
        });
    }

    forcedKillTimer_->start(1500);
}
