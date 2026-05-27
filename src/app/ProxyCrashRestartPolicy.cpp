#include "app/ProxyCrashRestartPolicy.h"

#include <algorithm>

#include <QCoreApplication>

#include "app/TunRuntimeState.h"
#include "runtime/core/CoreCatalog.h"

namespace {

constexpr int kMaxTunAdapterConflictRestarts = 1;
constexpr qint64 kStableRunThresholdMs = 60000;
constexpr int kMaxCrashRestarts = 5;

QString coreLabel(bool auxiliary)
{
    return auxiliary ? QStringLiteral("Auxiliary core") : QStringLiteral("Core");
}

QString exitKind(QProcess::ExitStatus exitStatus)
{
    return exitStatus == QProcess::CrashExit ? QStringLiteral("crash") : QStringLiteral("exit");
}

} // namespace

void ProxyCrashRestartPolicy::resetTunConflictRetries()
{
    coreTunAdapterConflictRetryCount_ = 0;
}

ProxyCrashRestartPolicy::Decision ProxyCrashRestartPolicy::decide(
    int exitCode,
    QProcess::ExitStatus exitStatus,
    bool auxiliary,
    bool runningOnWindows,
    bool tunEnabledAtCoreExit,
    bool tunAdapterConflictDetected,
    qint64 runDurationMs)
{
    if (!auxiliary
        && shouldRetryAfterTunAdapterConflict(
            runningOnWindows,
            tunEnabledAtCoreExit,
            tunAdapterConflictDetected,
            coreTunAdapterConflictRetryCount_,
            kMaxTunAdapterConflictRestarts)) {
        ++coreTunAdapterConflictRetryCount_;
        return Decision{
            Action::ScheduleRestart,
            QStringLiteral("TUN adapter conflict detected (code=%1). Cleaning up and retrying core startup...")
                .arg(exitCode),
            1000,
            false,
            true};
    }

    if (!auxiliary && tunAdapterConflictDetected) {
        return Decision{
            Action::DisableAfterTunConflict,
            QStringLiteral("TUN adapter conflict persisted after cleanup retry. Auto-restart disabled."),
            0,
            false,
            false};
    }

    int& count = crashCount(auxiliary);
    if (runDurationMs > kStableRunThresholdMs) {
        count = 0;
    }
    ++count;

    if (count > kMaxCrashRestarts) {
        const QString message =
            QStringLiteral("%1 %2 detected (code=%3). Auto-restart disabled after %4 consecutive failures.")
                .arg(coreLabel(auxiliary), exitKind(exitStatus))
                .arg(exitCode)
                .arg(kMaxCrashRestarts);
        return Decision{Action::DisableRestart, message, 0, auxiliary, false};
    }

    const int delayMs = std::min(3000 * count, 30000);
    const QString message =
        QStringLiteral("%1 %2 detected (code=%3). Restarting in %4s... (attempt %5/%6)")
            .arg(coreLabel(auxiliary), exitKind(exitStatus))
            .arg(exitCode)
            .arg(delayMs / 1000)
            .arg(count)
            .arg(kMaxCrashRestarts);
    return Decision{Action::ScheduleRestart, message, delayMs, auxiliary, false};
}

int ProxyCrashRestartPolicy::crashCount(bool auxiliary) const
{
    return auxiliary ? auxiliaryCrashRestartCount_ : coreCrashRestartCount_;
}

int& ProxyCrashRestartPolicy::crashCount(bool auxiliary)
{
    return auxiliary ? auxiliaryCrashRestartCount_ : coreCrashRestartCount_;
}
