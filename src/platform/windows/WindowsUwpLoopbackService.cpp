#include "platform/windows/WindowsUwpLoopbackService.h"

#include <algorithm>

#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QObject>
#include <QIODevice>
#include <QProcess>
#include <QThread>

#include "platform/windows/WindowsUwpLoopbackSupport.h"

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

constexpr int kProcessTimeoutMs = 30000;
constexpr int kElevatedScriptWaitMs = 180000;

QString processText(const QByteArray& data)
{
    return QString::fromUtf8(data);
}

} // namespace

bool WindowsUwpLoopbackService::isAvailable() const
{
#if defined(Q_OS_WIN)
    return QFileInfo::exists(checkNetIsolationPath());
#else
    return false;
#endif
}

QList<WindowsUwpPackageInfo> WindowsUwpLoopbackService::listPackages(OperationResult* result) const
{
#if defined(Q_OS_WIN)
    QString output;
    const QString command = QStringLiteral(
        "[Console]::OutputEncoding=[System.Text.Encoding]::UTF8; "
        "Get-AppxPackage | "
        "Select-Object Name,PackageFamilyName,PackageFullName,Publisher,InstallLocation | "
        "ConvertTo-Json -Compress");
    const OperationResult processResult = runProcess(
        QStringLiteral("powershell.exe"),
        {
            QStringLiteral("-NoProfile"),
            QStringLiteral("-ExecutionPolicy"),
            QStringLiteral("Bypass"),
            QStringLiteral("-Command"),
            command
        },
        &output);
    if (!processResult.success) {
        if (result != nullptr) {
            *result = processResult;
        }
        return {};
    }

    OperationResult parseResult;
    QList<WindowsUwpPackageInfo> packages =
        WindowsUwpLoopbackSupport::parsePackageListJson(output, &parseResult);
    if (!parseResult.success) {
        if (result != nullptr) {
            *result = parseResult;
        }
        return {};
    }

    OperationResult exemptionsResult;
    const QSet<QString> exemptPackages = listExemptPackageFamilyNames(&exemptionsResult);
    if (!exemptionsResult.success) {
        if (result != nullptr) {
            *result = exemptionsResult;
        }
        return {};
    }

    for (WindowsUwpPackageInfo& package : packages) {
        package.loopbackEnabled = exemptPackages.contains(package.packageFamilyName);
    }

    std::sort(packages.begin(), packages.end(), [](const auto& lhs, const auto& rhs) {
        const QString lhsName = lhs.name.isEmpty() ? lhs.packageFamilyName : lhs.name;
        const QString rhsName = rhs.name.isEmpty() ? rhs.packageFamilyName : rhs.name;
        return QString::localeAwareCompare(lhsName, rhsName) < 0;
    });

    if (result != nullptr) {
        *result = OperationResult::ok();
    }
    return packages;
#else
    if (result != nullptr) {
        *result = OperationResult::fail(QObject::tr("UWP loopback exemptions are only available on Windows."));
    }
    return {};
#endif
}

QSet<QString> WindowsUwpLoopbackService::listExemptPackageFamilyNames(OperationResult* result) const
{
#if defined(Q_OS_WIN)
    if (!isAvailable()) {
        if (result != nullptr) {
            *result = OperationResult::fail(QObject::tr("CheckNetIsolation.exe was not found."));
        }
        return {};
    }

    QString output;
    const OperationResult processResult = runProcess(
        checkNetIsolationPath(),
        {QStringLiteral("LoopbackExempt"), QStringLiteral("-s")},
        &output);
    if (!processResult.success) {
        if (result != nullptr) {
            *result = processResult;
        }
        return {};
    }

    const QSet<QString> packageNames = WindowsUwpLoopbackSupport::parseExemptPackageFamilyNames(output);

    if (result != nullptr) {
        *result = OperationResult::ok();
    }
    return packageNames;
#else
    if (result != nullptr) {
        *result = OperationResult::fail(QObject::tr("UWP loopback exemptions are only available on Windows."));
    }
    return {};
#endif
}

OperationResult WindowsUwpLoopbackService::setLoopbackEnabled(const QString& packageFamilyName, bool enabled) const
{
#if defined(Q_OS_WIN)
    if (!isAvailable()) {
        return OperationResult::fail(QObject::tr("CheckNetIsolation.exe was not found."));
    }

    const QString trimmedPackageName = packageFamilyName.trimmed();
    if (trimmedPackageName.isEmpty()) {
        return OperationResult::fail(QObject::tr("Package family name is empty."));
    }

    QString output;
    return runProcess(
        checkNetIsolationPath(),
        {
            QStringLiteral("LoopbackExempt"),
            enabled ? QStringLiteral("-a") : QStringLiteral("-d"),
            QStringLiteral("-n=%1").arg(trimmedPackageName)
        },
        &output);
#else
    Q_UNUSED(packageFamilyName);
    Q_UNUSED(enabled);
    return OperationResult::fail(QObject::tr("UWP loopback exemptions are only available on Windows."));
#endif
}

OperationResult WindowsUwpLoopbackService::setLoopbackEnabledElevated(
    const QHash<QString, bool>& enabledByPackageFamilyName) const
{
#if defined(Q_OS_WIN)
    if (!isAvailable()) {
        return OperationResult::fail(QObject::tr("CheckNetIsolation.exe was not found."));
    }
    if (enabledByPackageFamilyName.isEmpty()) {
        return OperationResult::ok();
    }

    QTemporaryDir tempDir(QDir::temp().filePath(QStringLiteral("songbird-uwp-loopback-XXXXXX")));
    if (!tempDir.isValid()) {
        return OperationResult::fail(QObject::tr("Failed to create temporary update script directory."));
    }
    tempDir.setAutoRemove(false);

    const QString donePath = QDir(tempDir.path()).filePath(QStringLiteral("done.txt"));
    const QString logPath = QDir(tempDir.path()).filePath(QStringLiteral("loopback.log"));
    const QString scriptPath = QDir(tempDir.path()).filePath(QStringLiteral("apply-loopback.ps1"));

    const OperationResult scriptResult = WindowsUwpLoopbackSupport::writeElevatedLoopbackScript(
        scriptPath,
        donePath,
        logPath,
        enabledByPackageFamilyName);
    if (!scriptResult.success) {
        return scriptResult;
    }

    const OperationResult launchResult = runElevatedScript(scriptPath);
    if (!launchResult.success) {
        return launchResult;
    }

    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + kElevatedScriptWaitMs;
    while (QDateTime::currentMSecsSinceEpoch() < deadline) {
        if (QFileInfo::exists(donePath)) {
            QFile doneFile(donePath);
            const bool doneOk = doneFile.open(QIODevice::ReadOnly | QIODevice::Text);
            const QString done = doneOk ? QString::fromLatin1(doneFile.readAll()).trimmed() : QString();

            QFile logFile(logPath);
            const QString log = logFile.open(QIODevice::ReadOnly | QIODevice::Text)
                ? QString::fromUtf8(logFile.readAll()).trimmed()
                : QString();

            QDir(tempDir.path()).removeRecursively();
            return done == QStringLiteral("0")
                ? OperationResult::ok(log)
                : OperationResult::fail(log.isEmpty()
                    ? QObject::tr("Elevated UWP loopback update failed.")
                    : log);
        }
        QThread::msleep(250);
    }

    return OperationResult::fail(QObject::tr("Timed out waiting for elevated UWP loopback update."));
#else
    Q_UNUSED(enabledByPackageFamilyName);
    return OperationResult::fail(QObject::tr("UWP loopback exemptions are only available on Windows."));
#endif
}

OperationResult WindowsUwpLoopbackService::runProcess(
    const QString& program,
    const QStringList& arguments,
    QString* output) const
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);
    if (!process.waitForStarted(kProcessTimeoutMs)) {
        return OperationResult::fail(QObject::tr("Failed to start %1.").arg(program));
    }
    if (!process.waitForFinished(kProcessTimeoutMs)) {
        process.kill();
        process.waitForFinished(3000);
        return OperationResult::fail(QObject::tr("%1 timed out.").arg(program));
    }

    const QString mergedOutput = processText(process.readAll());
    if (output != nullptr) {
        *output = mergedOutput;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString detail = mergedOutput.trimmed();
        return OperationResult::fail(detail.isEmpty()
                ? QObject::tr("%1 failed with exit code %2.").arg(program).arg(process.exitCode())
                : detail);
    }

    return OperationResult::ok(mergedOutput.trimmed());
}

OperationResult WindowsUwpLoopbackService::runElevatedScript(const QString& scriptPath) const
{
#if defined(Q_OS_WIN)
    const QString program = QStringLiteral("powershell.exe");
    QString nativeScriptPath = QDir::toNativeSeparators(scriptPath);
    nativeScriptPath.replace(QChar('"'), QStringLiteral("\\\""));
    const QString parameters = QStringLiteral("-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%1\"")
        .arg(nativeScriptPath);
    const HINSTANCE result = ShellExecuteW(
        nullptr,
        L"runas",
        reinterpret_cast<LPCWSTR>(program.utf16()),
        reinterpret_cast<LPCWSTR>(parameters.utf16()),
        nullptr,
        SW_HIDE);
    return reinterpret_cast<INT_PTR>(result) > 32
        ? OperationResult::ok()
        : OperationResult::fail(QObject::tr("Failed to start elevated UWP loopback update."));
#else
    Q_UNUSED(scriptPath);
    return OperationResult::fail(QObject::tr("UWP loopback exemptions are only available on Windows."));
#endif
}

QString WindowsUwpLoopbackService::checkNetIsolationPath() const
{
#if defined(Q_OS_WIN)
    return WindowsUwpLoopbackSupport::checkNetIsolationPath();
#else
    return {};
#endif
}
