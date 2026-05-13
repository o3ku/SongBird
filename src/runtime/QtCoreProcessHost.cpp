#include "runtime/QtCoreProcessHost.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <utility>

#include "common/CorePidFile.h"

namespace {
const QRegularExpression kAnsiEscape(QStringLiteral("\x1b\\[[0-9;]*[a-zA-Z]"));
}

QtCoreProcessHost::QtCoreProcessHost() = default;

QtCoreProcessHost::~QtCoreProcessHost()
{
    stop(true);
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

    process_ = std::make_unique<QProcess>();
    outputReceived_ = std::move(outputReceived);
    startedCallback_ = std::move(started);
    startFailedCallback_ = std::move(startFailed);
    exitedCallback_ = std::move(exited);
    stopRequested_ = false;
    startNotified_ = false;
    lastCoreInfo_ = coreInfo;
    lastConfigFilePath_ = configFilePath;
    standardOutputBuffer_.clear();
    standardErrorBuffer_.clear();

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
    scheduleForcedKill();
    if (immediate) {
        return OperationResult::ok(QStringLiteral("Stopping core process immediately..."));
    }

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
    if (lastCoreInfo_.captureOutput) {
        QObject::connect(process_.get(), &QProcess::readyReadStandardOutput, process_.get(), [this]() {
            emitBufferedOutput(QProcess::StandardOutput);
        });

        QObject::connect(process_.get(), &QProcess::readyReadStandardError, process_.get(), [this]() {
            emitBufferedOutput(QProcess::StandardError);
        });
    }

    QObject::connect(
        process_.get(),
        &QProcess::started,
        process_.get(),
        [this]() {
            recordCorePid(process_->processId());

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
        process_.get(),
        &QProcess::errorOccurred,
        process_.get(),
        [this](QProcess::ProcessError error) {
            if (startNotified_ || error != QProcess::FailedToStart) {
                return;
            }

            startNotified_ = true;
            if (startFailedCallback_) {
                startFailedCallback_(
                    QStringLiteral("Failed to start core process: %1").arg(process_->errorString()));
            }
        });

    QObject::connect(
        process_.get(),
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        process_.get(),
        [this](int exitCode, QProcess::ExitStatus status) {
            removeCorePid(process_->processId());
            if (forcedKillTimer_ != nullptr) {
                forcedKillTimer_->stop();
            }
            flushBufferedOutput(true);
            if (exitedCallback_) {
                exitedCallback_(exitCode, status, stopRequested_);
            }
            resetProcessState();
        });
}

void QtCoreProcessHost::emitBufferedOutput(QProcess::ProcessChannel channel)
{
    if (!process_ || !outputReceived_) {
        return;
    }

    const QByteArray payload = channel == QProcess::StandardOutput
        ? process_->readAllStandardOutput()
        : process_->readAllStandardError();
    if (payload.isEmpty()) {
        return;
    }

    QString& pendingBuffer = channel == QProcess::StandardOutput
        ? standardOutputBuffer_
        : standardErrorBuffer_;
    pendingBuffer.append(QString::fromUtf8(payload));

    qsizetype newlineIndex = pendingBuffer.indexOf(QChar('\n'));
    while (newlineIndex >= 0) {
        QString line = pendingBuffer.left(newlineIndex);
        if (line.endsWith(QChar('\r'))) {
            line.chop(1);
        }

        pendingBuffer.remove(0, newlineIndex + 1);
        line.remove(kAnsiEscape);
        if (!line.isEmpty()) {
            outputReceived_(line);
        }

        newlineIndex = pendingBuffer.indexOf(QChar('\n'));
    }
}

void QtCoreProcessHost::flushBufferedOutput(bool flushPartialLines)
{
    if (!outputReceived_) {
        standardOutputBuffer_.clear();
        standardErrorBuffer_.clear();
        return;
    }

    const auto flushBuffer = [this, flushPartialLines](QString& buffer) {
        if (buffer.isEmpty()) {
            return;
        }

        if (!flushPartialLines && !buffer.contains(QChar('\n'))) {
            return;
        }

        QString remaining = std::move(buffer);
        buffer.clear();
        remaining.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        remaining.replace(QChar('\r'), QChar('\n'));

        const QStringList lines = remaining.split(QChar('\n'), Qt::SkipEmptyParts);
        for (QString line : lines) {
            line.remove(kAnsiEscape);
            if (!line.isEmpty()) {
                outputReceived_(line);
            }
        }
    };

    flushBuffer(standardOutputBuffer_);
    flushBuffer(standardErrorBuffer_);
}

void QtCoreProcessHost::resetProcessState()
{
    if (forcedKillTimer_ != nullptr) {
        forcedKillTimer_->stop();
        forcedKillTimer_->deleteLater();
        forcedKillTimer_ = nullptr;
    }

    process_.reset();
    startedCallback_ = {};
    startFailedCallback_ = {};
    exitedCallback_ = {};
    startNotified_ = false;
    stopRequested_ = false;
    standardOutputBuffer_.clear();
    standardErrorBuffer_.clear();
}

void QtCoreProcessHost::scheduleForcedKill()
{
    if (!process_) {
        return;
    }

    if (forcedKillTimer_ == nullptr) {
        forcedKillTimer_ = new QTimer();
        forcedKillTimer_->setSingleShot(true);
        QObject::connect(forcedKillTimer_, &QTimer::timeout, forcedKillTimer_, [this]() {
            if (process_ && process_->state() != QProcess::NotRunning) {
                process_->kill();
            }
        });
    }

    forcedKillTimer_->start(1500);
}
