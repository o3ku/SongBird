#include "runtime/core/CoreAssetPlatform.h"

// Load-bearing, and the only reason this file has a Qt include at all: Q_OS_WIN, Q_OS_MACOS and
// Q_PROCESSOR_ARM_64 are defined by Qt's own headers, not by the compiler, and no other include here
// brings one in. Without it every branch below is skipped, the function silently returns Os::Other,
// and since Other falls through to the Windows branch of every backend, a macOS build downloads and
// installs the Windows core. Windows keeps working by accident, which is what hides the mistake.
#include <QtGlobal>

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

CoreAssetPlatform currentAssetPlatform()
{
    CoreAssetPlatform platform;

#if defined(Q_OS_WIN)
    platform.os = CoreAssetPlatform::Os::Windows;

    SYSTEM_INFO systemInfo{};
    GetNativeSystemInfo(&systemInfo);
    switch (systemInfo.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
    case PROCESSOR_ARCHITECTURE_ARM64:
        platform.sixtyFourBit = true;
        break;
    default:
        platform.sixtyFourBit = false;
        break;
    }
#elif defined(Q_OS_MACOS)
    platform.os = CoreAssetPlatform::Os::MacOS;
    platform.sixtyFourBit = true;
#if defined(Q_PROCESSOR_ARM_64)
    platform.appleSilicon = true;
#endif
#endif

    return platform;
}
