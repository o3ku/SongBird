#include "runtime/core/CoreAssetPlatform.h"

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
