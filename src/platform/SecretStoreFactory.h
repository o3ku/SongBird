#pragma once

#include <memory>

#include "platform/ISecretStore.h"

// The secret store for this platform: DPAPI on Windows, Keychain on macOS,
// and the unavailable NullSecretStore elsewhere. Implemented once per
// platform (platform/windows, platform/macos, platform/null) and registered
// in src/CMakeLists.txt next to the other platform services.
std::unique_ptr<ISecretStore> createSecretStore();
