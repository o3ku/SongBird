#include "runtime/CoreConfigPreflight.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

namespace {

constexpr int kStartTimeoutMs = 3000;
constexpr int kStopTimeoutMs = 1500;
constexpr int kMaxOutputLength = 2400;

QString normalizedCoreFileName(const CoreInfo& coreInfo)
{
    return QFileInfo(coreInfo.program).fileName().toLower();
}

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

QString processOutput(QProcess& process)
{
    return normalizeProcessOutput(QString::fromUtf8(process.readAll()));
}

} // namespace

QStringList buildCoreConfigPreflightArguments(const CoreInfo& coreInfo, const QString& configFilePath)
{
    const QString configPath = QDir::toNativeSeparators(configFilePath);
    const QString fileName = normalizedCoreFileName(coreInfo);

    if (fileName.contains(QStringLiteral("sing-box"))) {
        return {
            QStringLiteral("check"),
            QStringLiteral("-c"),
            configPath
        };
    }

    if (fileName.contains(QStringLiteral("xray"))) {
        return {
            QStringLiteral("run"),
            QStringLiteral("-test"),
            QStringLiteral("-config"),
            configPath
        };
    }

    if (fileName.contains(QStringLiteral("v2ray"))) {
        return {
            QStringLiteral("-test"),
            QStringLiteral("-config"),
            configPath
        };
    }

    return {};
}

OperationResult validateCoreConfigBeforeStart(
    const CoreInfo& coreInfo,
    const QString& configFilePath,
    int timeoutMs)
{
    if (coreInfo.program.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Core config preflight failed: core executable path is empty."));
    }

    if (!QFileInfo::exists(coreInfo.program)) {
        return OperationResult::fail(
            QStringLiteral("Core config preflight failed: core executable was not found: %1")
                .arg(coreInfo.program));
    }

    if (configFilePath.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Core config preflight failed: config path is empty."));
    }

    if (!QFileInfo::exists(configFilePath)) {
        return OperationResult::fail(
            QStringLiteral("Core config preflight failed: config file was not found: %1")
                .arg(configFilePath));
    }

    const QStringList arguments = buildCoreConfigPreflightArguments(coreInfo, configFilePath);
    if (arguments.isEmpty()) {
        return OperationResult::ok(
            QStringLiteral("Core config preflight skipped for unsupported core: %1")
                .arg(QFileInfo(coreInfo.program).fileName()));
    }

    QProcess process;
    process.setProgram(coreInfo.program);
    process.setArguments(arguments);
    process.setWorkingDirectory(
        coreInfo.workingDirectory.trimmed().isEmpty()
            ? QFileInfo(coreInfo.program).absolutePath()
            : coreInfo.workingDirectory);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();

    if (!process.waitForStarted(kStartTimeoutMs)) {
        return OperationResult::fail(
            QStringLiteral("Core config preflight failed to start: %1")
                .arg(process.errorString()));
    }

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(kStopTimeoutMs);
        const QString output = processOutput(process);
        QString message = QStringLiteral("Core config preflight timed out after %1 ms.").arg(timeoutMs);
        if (!output.isEmpty()) {
            message += QStringLiteral("\n%1").arg(output);
        }
        return OperationResult::fail(message);
    }

    const QString output = processOutput(process);
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QString message = QStringLiteral("Core config preflight failed");
        if (process.exitStatus() == QProcess::CrashExit) {
            message += QStringLiteral(" because the check process crashed");
        } else {
            message += QStringLiteral(" with exit code %1").arg(process.exitCode());
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
            QStringLiteral("Core config preflight passed: %1").arg(coreName));
    }

    return OperationResult::ok(
        QStringLiteral("Core config preflight passed: %1\n%2")
            .arg(coreName, output));
}
