#pragma once

#include "platform/IAutoRunService.h"

// Stand-in for platforms with no "start at login" mechanism wired up.
//
// isEnabled() reports false and setEnabled() reports failure, rather than pretending the
// change stuck. The tray toggle and the settings page both read the answer back through
// syncStatusIndicators(), so a claimed success would leave a checkbox ticked for a setting
// that was never written anywhere.
class NullAutoRunService : public IAutoRunService {
public:
    bool isEnabled() const override { return false; }
    bool setEnabled(bool enabled) const override
    {
        Q_UNUSED(enabled);
        return false;
    }
};
