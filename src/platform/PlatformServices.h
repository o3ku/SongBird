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
#include "platform/windows/WindowsThemePreferenceService.h"

using PlatformAutoRunService = WindowsAutoRunService;
using PlatformSystemProxyService = WindowsSystemProxyService;
using PlatformThemePreferenceService = WindowsThemePreferenceService;

#elif defined(Q_OS_MACOS)

#include "platform/macos/MacAutoRunService.h"
#include "platform/macos/MacSystemProxyService.h"
#include "platform/NullThemePreferenceService.h"

using PlatformAutoRunService = MacAutoRunService;
using PlatformSystemProxyService = MacSystemProxyService;
using PlatformThemePreferenceService = NullThemePreferenceService;

#else

// Every other platform: the interfaces are still satisfied, because the front-ends construct
// these unconditionally. Linux is the reason this branch is no longer only a theme stand-in --
// it builds the full application (see .cnb.yml) while the proxy and auto-run mechanisms here
// are Windows-only, so the two side effects degrade to stand-ins that report "not applied"
// rather than the composition root growing a platform conditional at each construction site.
#include "platform/NullThemePreferenceService.h"
#include "platform/null/NullAutoRunService.h"
#include "platform/null/NullSystemProxyService.h"

using PlatformAutoRunService = NullAutoRunService;
using PlatformSystemProxyService = NullSystemProxyService;
using PlatformThemePreferenceService = NullThemePreferenceService;

#endif
