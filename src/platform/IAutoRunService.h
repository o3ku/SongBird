#pragma once

// The "start SongBird when Windows starts" side effect, reduced to the operations its callers use.
//
// Same reasoning as ISystemProxyService: the implementation writes the Run key through QSettings
// with NativeFormat, so the "the write was refused" branch cannot be produced on demand in a test.
// Callers take this interface so a stand-in can report the refusal and the caller's handling of it
// becomes verifiable.
class IAutoRunService {
public:
    virtual ~IAutoRunService() = default;

    virtual bool isEnabled() const = 0;
    virtual bool setEnabled(bool enabled) const = 0;
};
