#pragma once

#include <QProcess>
#include <QString>

class ProxyCrashRestartPolicy final
{
public:
    enum class Action {
        ScheduleRestart,
        DisableRestart,
        DisableAfterTunConflict
    };

    struct Decision {
        Action action = Action::ScheduleRestart;
        QString message;
        QString transientStatus;
        int delayMs = 0;
        bool auxiliary = false;
        bool tunConflictRetry = false;
    };

    void resetTunConflictRetries();
    Decision decide(
        int exitCode,
        QProcess::ExitStatus exitStatus,
        bool auxiliary,
        bool runningOnWindows,
        bool tunEnabledAtCoreExit,
        bool tunAdapterConflictDetected,
        qint64 runDurationMs);

private:
    int crashCount(bool auxiliary) const;
    int& crashCount(bool auxiliary);

    int coreTunAdapterConflictRetryCount_ = 0;
    int coreCrashRestartCount_ = 0;
    int auxiliaryCrashRestartCount_ = 0;
};
