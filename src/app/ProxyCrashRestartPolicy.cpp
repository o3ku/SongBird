#include "app/ProxyCrashRestartPolicy.h"

#include <algorithm>

#include <QCoreApplication>

#include "app/TunRuntimeState.h"
#include "runtime/core/CoreCatalog.h"

namespace {

constexpr int kMaxTunAdapterConflictRestarts = 1;
constexpr qint64 kStableRunThresholdMs = 60000;
constexpr int kMaxCrashRestarts = 5;

// One complete sentence per (core, exit kind), rather than gluing a subject fragment
// ("Core" / "Auxiliary core") and a verb fragment ("crash" / "exit") into a shared
// "%1 %2 detected" template. Chinese word order puts the parts the other way round, so a
// translator cannot reorder a template's slots -- the same reasoning that made
// JsonConfigRepository::describeWriteFailure spell out one sentence per stage.
//
// The result is then placed in a frame ("%1 Auto-restart disabled after %2 ..."). That is a
// sentence-valued slot, not a fragment one, so the frame stays translatable: the translator
// controls the whole frame and can move the embedded sentence wherever the target language
// needs it.
//
// These strings reach the log panel through ProxySession's `emit logMessage`, so they use that
// context -- same as CoreStartupChecklist.cpp, the other app/ file whose text surfaces there.
QString crashSummary(bool auxiliary, QProcess::ExitStatus exitStatus, int exitCode)
{
    const bool crashed = exitStatus == QProcess::CrashExit;
    if (auxiliary) {
        return crashed
            ? QCoreApplication::translate("ProxySession", "Auxiliary core crash detected (code=%1).")
                  .arg(exitCode)
            : QCoreApplication::translate("ProxySession", "Auxiliary core exited (code=%1).")
                  .arg(exitCode);
    }
    return crashed
        ? QCoreApplication::translate("ProxySession", "Core crash detected (code=%1).").arg(exitCode)
        : QCoreApplication::translate("ProxySession", "Core exited (code=%1).").arg(exitCode);
}

QString restartDisabledMessage(
    bool auxiliary,
    QProcess::ExitStatus exitStatus,
    int exitCode,
    int consecutiveFailures)
{
    // `consecutiveFailures` is the count that just exceeded kMaxCrashRestarts, not the budget
    // itself: this frame is reached on the failure *after* the last allowed restart, so filling the
    // slot with the constant said "after 5 consecutive failures" about the sixth one.
    return QCoreApplication::translate(
               "ProxySession", "%1 Auto-restart disabled after %2 consecutive failures.")
        .arg(crashSummary(auxiliary, exitStatus, exitCode))
        .arg(consecutiveFailures);
}

QString restartingMessage(
    bool auxiliary,
    QProcess::ExitStatus exitStatus,
    int exitCode,
    int delayMs,
    int attempt)
{
    return QCoreApplication::translate("ProxySession", "%1 Restarting in %2s... (attempt %3/%4)")
        .arg(crashSummary(auxiliary, exitStatus, exitCode))
        .arg(delayMs / 1000)
        .arg(attempt)
        .arg(kMaxCrashRestarts);
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
            QCoreApplication::translate(
                "ProxySession",
                "TUN adapter conflict detected (code=%1). Cleaning up and retrying core startup...")
                .arg(exitCode),
            1000,
            false};
    }

    if (!auxiliary && tunAdapterConflictDetected) {
        return Decision{
            Action::DisableAfterTunConflict,
            QCoreApplication::translate(
                "ProxySession",
                "TUN adapter conflict persisted after cleanup retry. Auto-restart disabled."),
            0,
            false};
    }

    int& count = crashCount(auxiliary);
    if (runDurationMs > kStableRunThresholdMs) {
        count = 0;
    }
    ++count;

    if (count > kMaxCrashRestarts) {
        return Decision{
            Action::DisableRestart,
            restartDisabledMessage(auxiliary, exitStatus, exitCode, count),
            0,
            auxiliary};
    }

    const int delayMs = std::min(3000 * count, 30000);
    return Decision{
        Action::ScheduleRestart,
        restartingMessage(auxiliary, exitStatus, exitCode, delayMs, count),
        delayMs,
        auxiliary};
}

int ProxyCrashRestartPolicy::crashCount(bool auxiliary) const
{
    return auxiliary ? auxiliaryCrashRestartCount_ : coreCrashRestartCount_;
}

int& ProxyCrashRestartPolicy::crashCount(bool auxiliary)
{
    return auxiliary ? auxiliaryCrashRestartCount_ : coreCrashRestartCount_;
}
