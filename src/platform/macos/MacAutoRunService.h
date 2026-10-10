#pragma once

#include <QString>

#include "platform/IAutoRunService.h"

// The "start SongBird when the user logs in" side effect on macOS, expressed as a
// per-user LaunchAgent.
//
// There is no macOS equivalent of the Windows Run key: the supported mechanism is a
// property list under ~/Library/LaunchAgents that launchd reads at login. Writing it is
// unprivileged -- it is the user's own directory -- which is why this service never has
// to escalate, unlike the system proxy.
//
// The agent is identified by a fixed label rather than by the bundle identifier so that
// an upgrade that changes the bundle identifier cannot leave the old agent behind and
// start the application twice.

// The three pieces of the LaunchAgent, as free functions rather than as private members.
//
// This file is only added to the build on APPLE, and no test ever runs on macOS, so as
// members nothing on any other platform would compile them: the plist the application
// writes would be exercised for the first time in a release build. As free functions the
// test target builds them everywhere and drives them directly. isEnabled()/setEnabled()
// cannot serve as that seam -- they touch the real HOME directory and spawn launchctl.
//
// The program path is a parameter for the same reason: the escaping in the plist is only
// reachable if the caller can choose a path that needs escaping.
QString macLaunchAgentLabel();
QString macLaunchAgentPath();
QString macLaunchAgentContents(const QString& programPath);

class MacAutoRunService : public IAutoRunService {
public:
    bool isEnabled() const override;
    bool setEnabled(bool enabled) const override;
};
