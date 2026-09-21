#pragma once

#include "platform/IAutoRunService.h"

class WindowsAutoRunService : public IAutoRunService {
public:
    bool isEnabled() const override;
    bool setEnabled(bool enabled) const override;

private:
    static constexpr const char* AutoRunRegistryPath = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static constexpr const char* AutoRunValueName = "SongBird";
};
