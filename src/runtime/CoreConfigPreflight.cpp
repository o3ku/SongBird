#include "runtime/CoreConfigPreflight.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

#include "common/ProcessRunner.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/ICoreBackend.h"

namespace {

constexpr int kStartTimeoutMs = 3000;
constexpr int kStopTimeoutMs = 1500;
constexpr int kMaxOutputLength = 2400;

QString normalizeProcessOutput(QString output)
{
    static const QRegularExpression ansiEscape(QStringLiteral("\x1b\\[[0-9;]*[a-zA-Z]"));

    output.remove(ansiEscape);
    output.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    output.replace(QChar('\r'), QChar('\n'));
    output = output.trimmed();

    if (output.size() > kMaxOutputLength) {
        output = output.left(kMaxOutputLength).trimmed()
            + QStringLiteral("\n...");
    }

    return output;
}

} // namespace

QStringList buildCoreConfigPreflightArguments(const CoreInfo& coreInfo, const QString& configFilePath)
{
    const QString fileName = QFileInfo(coreInfo.program).fileName().toLower();
    if (fileName.contains(QStringLiteral("v2ray"))) {
        return {
            QStringLiteral("-test"),
            QStringLiteral("-config"),
            QDir::toNativeSeparators(configFilePath)
        };
    }

    const ICoreBackend* backend = coreBackend(coreInfo.type);
    if (backend == nullptr) {
        backend = coreBackendForExecutableName(coreInfo.program);
    }
    if (backend != nullptr) {
        return backend->configPreflightArguments(configFilePath);
    }

    return {};
}

OperationResult validateCoreConfigBeforeStart(
    const CoreInfo& coreInfo,
    const QString& configFilePath,
    int timeoutMs)
{
    if (coreInfo.program.trimmed().isEmpty()) {
        return OperationResult::fail(QCoreApplication::translate("CoreConfigPreflight", "Core config preflight failed: core executable path is empty."));
    }

    if (!QFileInfo::exists(coreInfo.program)) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreConfigPreflight", "Core config preflight failed: core executable was not found: %1")
                .arg(coreInfo.program));
    }

    if (configFilePath.trimmed().isEmpty()) {
        return OperationResult::fail(QCoreApplication::translate("CoreConfigPreflight", "Core config preflight failed: config path is empty."));
    }

    if (!QFileInfo::exists(configFilePath)) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreConfigPreflight", "Core config preflight failed: config file was not found: %1")
                .arg(configFilePath));
    }

    const QStringList arguments = buildCoreConfigPreflightArguments(coreInfo, configFilePath);
    if (arguments.isEmpty()) {
        return OperationResult::ok(
            QCoreApplication::translate("CoreConfigPreflight", "Core config preflight skipped for unsupported core: %1")
                .arg(QFileInfo(coreInfo.program).fileName()));
    }

    ProcessRunner::Request request;
    request.program = coreInfo.program;
    request.arguments = arguments;
    request.workingDirectory = coreInfo.workingDirectory;
    request.startTimeoutMs = kStartTimeoutMs;
    request.finishTimeoutMs = timeoutMs;
    request.stopTimeoutMs = kStopTimeoutMs;

    const ProcessRunner::Outcome outcome = ProcessRunner::runToCompletion(request);

    if (outcome.status == ProcessRunner::Status::FailedToStart) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreConfigPreflight", "Core config preflight failed to start: %1")
                .arg(outcome.errorText));
    }

    const QString output = normalizeProcessOutput(outcome.output);

    if (outcome.status == ProcessRunner::Status::TimedOut) {
        QString message = QStringLiteral("Core config preflight timed out after %1 ms.").arg(timeoutMs);
        if (!output.isEmpty()) {
            message += QStringLiteral("\n%1").arg(output);
        }
        return OperationResult::fail(message);
    }

    if (!outcome.completedNormally() || outcome.exitCode != 0) {
        QString message = QStringLiteral("Core config preflight failed");
        if (outcome.exitStatus == QProcess::CrashExit) {
            message += QStringLiteral(" because the check process crashed");
        } else {
            message += QStringLiteral(" with exit code %1").arg(outcome.exitCode);
        }
        message += QStringLiteral(".");
        if (!output.isEmpty()) {
            message += QStringLiteral("\n%1").arg(output);
        }
        return OperationResult::fail(message);
    }

    const QString coreName = QFileInfo(coreInfo.program).fileName();
    if (output.isEmpty()) {
        return OperationResult::ok(
            QCoreApplication::translate("CoreConfigPreflight", "Core config preflight passed: %1").arg(coreName));
    }

    return OperationResult::ok(
        QCoreApplication::translate("CoreConfigPreflight", "Core config preflight passed: %1\n%2")
            .arg(coreName, output));
}
