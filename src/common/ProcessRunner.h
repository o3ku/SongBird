#pragma once

#include <QProcess>
#include <QString>
#include <QStringList>

// Runs a short-lived child process to completion (version probes, config
// preflight, netsh-style helpers) and reports how it ended.
//
// This is deliberately NOT the abstraction for the long-running core process --
// that is ICoreProcessHost / QtCoreProcessHost. Nor does it cover the core
// update pipeline, which polls a cancellation handler and performs per-step
// cleanup (removing a staging file, for example) when a step is cancelled; those
// call sites keep their own loops.
namespace ProcessRunner {

enum class Status {
    Completed,     // Exited on its own; the exit code may still be non-zero.
    FailedToStart, // The process could not be launched at all.
    TimedOut,      // Did not finish in time and was killed.
};

struct Request {
    QString program;
    QStringList arguments;
    // Empty means "the program's own directory", which is what the core
    // executables expect. Set inheritWorkingDirectory instead when the child
    // must keep the parent's current directory (PATH tools such as netsh).
    QString workingDirectory;
    bool inheritWorkingDirectory = false;
    // Merged by default: these are diagnostic CLI probes whose stderr belongs in
    // the message we surface alongside stdout.
    bool mergeChannels = true;
    int startTimeoutMs = 1500;
    int finishTimeoutMs = 5000;
    // Grace period after kill() before we stop waiting.
    int stopTimeoutMs = 1500;
};

struct Outcome {
    Status status = Status::FailedToStart;
    // Raw merged UTF-8 output. Callers apply their own trimming, ANSI stripping,
    // and truncation so each surface keeps its existing formatting.
    QString output;
    int exitCode = -1;
    QProcess::ExitStatus exitStatus = QProcess::NormalExit;
    // QProcess::errorString() captured when status is FailedToStart.
    QString errorText;

    bool completedNormally() const
    {
        return status == Status::Completed && exitStatus == QProcess::NormalExit;
    }

    bool succeeded() const
    {
        return completedNormally() && exitCode == 0;
    }
};

Outcome runToCompletion(const Request& request);

} // namespace ProcessRunner
