#include "common/ProcessRunner.h"

#include <QFileInfo>

namespace ProcessRunner {

Outcome runToCompletion(const Request& request)
{
    Outcome outcome;

    QProcess process;
    process.setProgram(request.program);
    process.setArguments(request.arguments);
    if (!request.inheritWorkingDirectory) {
        process.setWorkingDirectory(
            request.workingDirectory.trimmed().isEmpty()
                ? QFileInfo(request.program).absolutePath()
                : request.workingDirectory);
    }
    process.setProcessChannelMode(
        request.mergeChannels ? QProcess::MergedChannels : QProcess::SeparateChannels);
    process.start();

    if (!process.waitForStarted(request.startTimeoutMs)) {
        // errorString() has to be captured while the QProcess is still alive.
        outcome.status = Status::FailedToStart;
        outcome.errorText = process.errorString();
        return outcome;
    }

    if (!process.waitForFinished(request.finishTimeoutMs)) {
        process.kill();
        process.waitForFinished(request.stopTimeoutMs);
        // Read whatever was produced before the kill so the caller can include
        // it in the timeout message.
        outcome.status = Status::TimedOut;
        outcome.output = QString::fromUtf8(process.readAll());
        outcome.exitStatus = process.exitStatus();
        outcome.exitCode = process.exitCode();
        return outcome;
    }

    outcome.status = Status::Completed;
    outcome.output = QString::fromUtf8(process.readAll());
    outcome.exitStatus = process.exitStatus();
    outcome.exitCode = process.exitCode();
    return outcome;
}

} // namespace ProcessRunner
