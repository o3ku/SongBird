#pragma once

class WindowsAutoRunService {
public:
    bool isEnabled() const;
    bool setEnabled(bool enabled) const;

private:
    static constexpr const char* AutoRunRegistryPath = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static constexpr const char* AutoRunValueName = "SongBird";
};
