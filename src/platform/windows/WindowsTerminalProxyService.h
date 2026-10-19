#pragma once

#include "platform/ITerminalProxyService.h"

// Windows implementation of ITerminalProxyService.
//
// The variables live in HKEY_CURRENT_USER\Environment as REG_SZ values, which is what a new process
// inherits. Two details make this different from writing the registry and stopping there:
//
//   1. The in-process environment must be updated too (SetEnvironmentVariableW). Otherwise a child
//      process launched by this app inherits the *old* block, because Windows copies the parent's
//      environment at CreateProcess time, not from the registry.
//   2. Already-running programs need a broadcast. SendMessageTimeoutW(HWND_BROADCAST,
//      WM_SETTINGCHANGE, 0, L"Environment") is the documented way to tell them to re-read; without it
//      Explorer's copy stays stale and a terminal opened from it inherits the stale value.
//
// The broadcast uses SendMessageTimeout rather than SendMessage because a hung top-level window would
// otherwise block this call forever. A timeout is treated as success: the message reached the
// message queues, and a window that did not answer is not our problem to wait on.
//
// Testability. The registry path is injectable for the same reason WindowsSystemProxyService's is:
// a ctest run must not touch the user's real environment block, and the real key cannot be made to
// refuse a write. Injecting a scratch key exercises every read and write path against real registry
// storage.
//
// What is NOT injectable is the other two side effects -- SetEnvironmentVariableW and the broadcast --
// and that is deliberate rather than an omission. Both act on this process's own environment and on
// windows owned by other processes; neither can be pointed at a scratch location, and neither is
// reachable from a unit test. The consequence is that a test can verify what gets written to the
// registry but not that the in-process block was updated; that half is covered by the code being
// three lines long and by the process-level smoke test, not by a unit test pretending otherwise.
class WindowsTerminalProxyService : public ITerminalProxyService {
public:
    // `settingsPath` defaults to the real environment key. Pass a scratch key to test against
    // isolated storage; the registry read and write paths are identical either way.
    explicit WindowsTerminalProxyService(QString settingsPath = {});

    bool enable(const QString& proxyUrl, const QString& noProxy) const override;
    bool disable() const override;
    bool isEnabledFor(const QString& proxyUrl) const override;
    TerminalProxySnapshot capture() const override;
    bool restore(const TerminalProxySnapshot& snapshot) const override;

private:
    QString settingsPath_;
};
