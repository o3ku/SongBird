#pragma once

// Which operating system and CPU a release-asset decision is being made for.
//
// A value rather than a set of compile-time branches, because the asset names are the one part of
// the update path a test cannot reach by running on the machine under test: a macOS-only branch
// compiled on Windows is never executed by any test, and no CI job runs the suite on macOS at all.
// Passing the platform in is what lets every platform's naming be covered from any host.
struct CoreAssetPlatform {
    enum class Os {
        Windows,
        MacOS,
        Other,
    };

    Os os = Os::Other;

    // Apple Silicon rather than Intel. Only consulted on macOS, where the two cores are separate
    // downloads -- and where the universal bundle means the slice the process is running as, not the
    // machine it is running on, is what decides which one is needed.
    bool appleSilicon = false;

    // The operating system is 64-bit, which selects the primary variant of the vendor's pair. Always
    // true on macOS, which publishes no 32-bit build of anything.
    bool sixtyFourBit = true;

    bool isWindows() const { return os == Os::Windows; }
    bool isMacOS() const { return os == Os::MacOS; }
};

// The platform this build is running as.
//
// Defined out of line because the Windows answer comes from GetNativeSystemInfo, which reports the
// *operating system's* architecture -- a 32-bit process on 64-bit Windows still needs the 64-bit
// core -- and that would drag <windows.h> into a widely included header.
CoreAssetPlatform currentAssetPlatform();
