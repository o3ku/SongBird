#pragma once

#include <QtGlobal>

// Windows job object for the managed core processes.
//
// Why this exists: when this program dies unexpectedly (task-manager kill, crash), the
// QProcess bookkeeping dies with it and nothing reaps the running core. The old answer was
// CoreProcessCleanupService, which kills by image name on the *next* startup -- it cannot help
// when the orphan still holds the ports *now*, and it matches any process with the same name,
// including a mihomo the user started themselves.
//
// A job object with KILL_ON_JOB_CLOSE fixes both: every core child is assigned to one job whose
// handle lives in this process, so the OS terminates the members when our last handle closes,
// no matter how we died. The image-name cleanup stays as the fallback for orphans that predate
// this (and for children that escaped assignment), not as the primary mechanism.
//
// Lifetime: the handle is created once and deliberately never closed until process exit --
// closing it earlier would kill the cores it holds. Assignment is best-effort and reported back:
// when it fails (a process that is already in an incompatible job, a PID that raced away) the
// caller falls back to the PID file, which is exactly what happened before this existed.
namespace WindowsJobObject {

// Creates the application job object on first call; later calls return the same one.
// False only when creation itself failed.
bool ensureAppJobInitialized();

// Assigns an already-running process to the application job. False when the job could not be
// created, the PID is not usable, or the OS refused the assignment.
bool assignProcessToAppJob(qint64 pid);

#ifdef Q_OS_WIN
// The raw handle, for tests only. Null when ensureAppJobInitialized() was never called or
// failed. Never closed by callers: its lifetime is the process lifetime.
void* appJobHandleForTest();
#endif

} // namespace WindowsJobObject
