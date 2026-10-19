#include "platform/windows/WindowsJobObject.h"

#include <limits>
#include <mutex>

#include <QtGlobal>

#include <windows.h>

namespace {

HANDLE g_appJob = nullptr;
std::mutex g_appJobMutex;

} // namespace

namespace WindowsJobObject {

bool ensureAppJobInitialized()
{
    std::lock_guard<std::mutex> lock(g_appJobMutex);
    if (g_appJob != nullptr) {
        return true;
    }

    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job == nullptr) {
        return false;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(
            job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        CloseHandle(job);
        return false;
    }

    g_appJob = job;
    return true;
}

bool assignProcessToAppJob(qint64 pid)
{
    // QProcess::processId() is a PID on Windows, which fits in a DWORD. Anything else is a
    // caller bug or a platform where this file does not compile -- either way, refusing is the
    // only safe answer.
    if (pid <= 0 || static_cast<quint64>(pid) > static_cast<quint64>((std::numeric_limits<DWORD>::max)())) {
        return false;
    }
    if (!ensureAppJobInitialized()) {
        return false;
    }

    HANDLE processHandle = OpenProcess(
        PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (processHandle == nullptr) {
        return false;
    }

    const BOOL assigned = AssignProcessToJobObject(g_appJob, processHandle);
    CloseHandle(processHandle);
    return assigned != FALSE;
}

#ifdef Q_OS_WIN
void* appJobHandleForTest()
{
    std::lock_guard<std::mutex> lock(g_appJobMutex);
    return static_cast<void*>(g_appJob);
}
#endif

} // namespace WindowsJobObject
