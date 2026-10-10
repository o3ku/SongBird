#pragma once

// The concrete platform services, named once.
//
// Three composition roots -- AppBootstrapServiceWiring, AppBootstrap and
// SongBirdAutoCoordinator -- construct the auto-run and system-proxy services, and each of
// them has to name a concrete class that only exists on one platform. Selecting it here
// instead keeps those three sites free of a conditional that would otherwise have to be
// kept in step by hand, and gives the choice a single place to be documented.
//
// The interfaces (IAutoRunService, ISystemProxyService) stay platform-neutral: this header
// is the only thing that knows which implementation backs them, and it is included by
// front-end code, never by the shared services that consume the interfaces.
#if defined(Q_OS_WIN)

#include "platform/windows/WindowsAutoRunService.h"
#include "platform/windows/WindowsSystemProxyService.h"

using PlatformAutoRunService = WindowsAutoRunService;
using PlatformSystemProxyService = WindowsSystemProxyService;

#elif defined(Q_OS_MACOS)

#include "platform/macos/MacAutoRunService.h"
#include "platform/macos/MacSystemProxyService.h"

using PlatformAutoRunService = MacAutoRunService;
using PlatformSystemProxyService = MacSystemProxyService;

#endif
