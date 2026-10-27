find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test Widgets Network Svg)

link_libraries(songbird_core_descriptors)

# Shared production code is compiled once into songbird_shared_objs (defined in
# src/CMakeLists.txt) and linked by every test target below, so the per-test
# source lists only need test-only files and the production files that are not
# shared. RoutingProfiles.cpp used to be added to every target explicitly; it
# belongs to the shared library now.

find_program(SONGBIRD_POWERSHELL_EXECUTABLE NAMES pwsh powershell)
if(SONGBIRD_POWERSHELL_EXECUTABLE)
    add_test(
        NAME backend-boundaries
        COMMAND ${SONGBIRD_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check-backend-boundaries.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    add_test(
        NAME appbootstrap-member-order
        COMMAND ${SONGBIRD_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check-appbootstrap-member-order.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    # Q_OS_*/Q_PROCESSOR_* come from Qt's headers, not from the compiler, so a file that tests one
    # without including any Qt header compiles cleanly and takes the #else path in silence. That is
    # how CoreAssetPlatform.cpp reported Os::Other everywhere, which made a macOS build download the
    # Windows core -- Windows was unaffected only because Other falls through to its branch. The
    # behavioural half of this is CoreUpdateServiceTests::currentAssetPlatformMatchesTheBuildHost;
    # this half covers every other file that reaches for the same macros.
    add_test(
        NAME q-os-include-scope
        COMMAND ${SONGBIRD_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check-q-os-include-scope.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}/../src
            -TestRoot ${CMAKE_CURRENT_SOURCE_DIR}
    )
    # lupdate's "0 unfinished" is not a coverage metric: strings never wrapped in
    # tr()/translate() never reach the .ts, and strings that are wrapped but not added to
    # the hand-maintained .ts render in English. This check covers both gaps.
    add_test(
        NAME localization-coverage
        COMMAND ${SONGBIRD_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check-localization-coverage.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}/../src
            -TranslationFile ${CMAKE_CURRENT_SOURCE_DIR}/../translations/SongBird_zh_CN.ts
    )
    # The .qm files embedded into SongBird.exe come from translations/compiled/, which is committed,
    # because the Qt CI builds against (vcpkg's static qt5-base) has no lrelease. Nothing else in the
    # build notices when that committed copy falls behind the .ts, so check it explicitly.
    add_test(
        NAME translations-fresh
        COMMAND ${SONGBIRD_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check-translations-fresh.ps1
            -TranslationFile ${CMAKE_CURRENT_SOURCE_DIR}/../translations/SongBird_zh_CN.ts
            -CompiledFile ${CMAKE_CURRENT_SOURCE_DIR}/../translations/compiled/SongBird_zh_CN.qm
    )
    # Up to v2.4.3 the .qm embedding step was skipped without a message whenever lrelease was
    # missing, so every released binary was English-only while the build stayed green. Check the
    # produced artifacts rather than the configuration, so the same regression cannot pass again.
    add_test(
        NAME translations-embedded
        COMMAND ${SONGBIRD_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check-translations-embedded.ps1
            -BuildDirectory ${CMAKE_BINARY_DIR}/src
            -Executable $<TARGET_FILE:songbird>
    )
    # The version has one declaration (project(VERSION) in CMakeLists.txt) and several consumers    # (the compile definition, the macOS bundle, the release workflow's tag). Nothing compared them,
    # so a hardcoded second literal or a tag that disagrees with the declaration published a binary
    # whose About dialog, update check and release page all claimed different numbers. The release
    # workflow passes -Tag to this same script; here it runs the in-tree half.
    add_test(
        NAME version-consistency
        COMMAND ${SONGBIRD_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check-version-consistency.ps1
    )
    # The README is the project's front page and nothing compiles it, so a moved or renamed
    # screenshot leaves a broken image icon there with no build failure. This only checks that every
    # referenced path exists and is non-empty; whether an image still shows the current UI needs a
    # human, and that rule is written down in the script rather than pretended to be automated.
    add_test(
        NAME readme-assets
        COMMAND ${SONGBIRD_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check-readme-assets.ps1
    )
    # These four do not read the source tree; they exercise the vcpkg cache steps in
    # .github/workflows/release.yml. Each one extracts the step's real run: block from the workflow,
    # substitutes the ${{ }} expressions, and runs it under pwsh 7 against fixtures -- so a step that
    # stops parsing or starts computing the wrong verdict fails here instead of burning a CI run.
    # They are the only checks that spawn child processes, and they take noticeably longer than the
    # text-only ones; a failure names the individual case that broke.
    foreach(harness vcpkg-guard vcpkg-skip vcpkg-image-namespace vcpkg-evict)
        add_test(
            NAME ${harness}
            COMMAND ${SONGBIRD_POWERSHELL_EXECUTABLE}
                -NoProfile
                -File ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/verify-${harness}.ps1
        )
    endforeach()

    # A worker thread that keeps a raw pointer to a QObject the GUI thread can destroy is a
    # use-after-free that surfaces later, in unrelated code. This guard was written after six GUI
    # suites went red at once for exactly this. The rule: a continuation that captures `this` must
    # either post to `this` or test a lifetime token.
    add_test(
        NAME async-owners
        COMMAND ${SONGBIRD_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check-async-owners.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    # A guard that can never fail is worse than no guard: it produces the appearance of coverage.
    # This self-test mutates a temporary copy of the tree once per rule, asserts each guard goes
    # red, and asserts they all go green on an unmodified copy -- so a guard that fails everything
    # unconditionally cannot pass it either.
    add_test(
        NAME guard-selftest
        COMMAND ${SONGBIRD_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/selftest-guards.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    set_tests_properties(guard-selftest PROPERTIES LABELS "guard" TIMEOUT 600)

    # Colours live in a palette file and nowhere else. A hex in a stylesheet branch that a runtime
    # probe never builds is exactly the case no test can observe, so this is the only check that
    # can catch it. src/auto/AutoWindowPalette.cpp is the second palette: that window deliberately
    # has its own blue accent instead of the main app's green.
    add_test(
        NAME hardcoded-colors
        COMMAND ${SONGBIRD_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check-hardcoded-colors.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
endif()

# Qt's platform plugin is named per platform, and the tests are told which one to use by name:
# the Windows plugin does not exist on macOS, and a test that cannot create its QApplication
# dies before it runs a single assertion. cocoa rather than offscreen on macOS because those
# runners do have a window server -- .github/workflows/macos-verify.yml starts the real
# application with it -- and offscreen would be an environment nothing else here exercises.
if(WIN32)
    set(SONGBIRD_TEST_QPA_PLATFORM "windows")
elseif(APPLE)
    set(SONGBIRD_TEST_QPA_PLATFORM "cocoa")
else()
    set(SONGBIRD_TEST_QPA_PLATFORM "offscreen")
endif()

# Registered here rather than beside the other PowerShell guards because it needs the platform
# plugin name above. It re-runs every test binary to count their cases, so it carries its own
# label and stays out of the default `ctest` invocation.
if(SONGBIRD_POWERSHELL_EXECUTABLE)
    add_test(
        NAME test-suites-nonempty
        COMMAND ${SONGBIRD_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check-test-suites.ps1
            -BuildDirectory ${CMAKE_BINARY_DIR}
    )
    # The build directory is passed rather than a hand-built path, so the check follows a
    # reconfigure into a new preset instead of silently scanning a stale tree.
    set_tests_properties(test-suites-nonempty PROPERTIES
        LABELS "suite"
        TIMEOUT 900
        ENVIRONMENT "QT_QPA_PLATFORM=${SONGBIRD_TEST_QPA_PLATFORM}"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
    )
endif()

function(songbird_add_qt_test target_name test_name)
    cmake_parse_arguments(
        SONGBIRD_TEST
        ""
        "TEST_ENVIRONMENT;TEST_LABELS;TEST_TIMEOUT"
        "SOURCES;LIBRARIES"
        ${ARGN}
    )

    if(QT_VERSION_MAJOR GREATER_EQUAL 6)
        qt_add_executable(${target_name}
            MANUAL_FINALIZATION
            ${SONGBIRD_TEST_SOURCES}
        )
    else()
        add_executable(${target_name} ${SONGBIRD_TEST_SOURCES})
    endif()

    target_include_directories(${target_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_link_libraries(${target_name}
        PRIVATE
            songbird_shared_objs
            ${SONGBIRD_TEST_LIBRARIES}
    )

    add_test(NAME ${test_name} COMMAND ${target_name})
    set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=${SONGBIRD_TEST_QPA_PLATFORM}"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
    )
    if(SONGBIRD_TEST_TEST_LABELS)
        set_tests_properties(${test_name} PROPERTIES
            LABELS "${SONGBIRD_TEST_TEST_LABELS}"
        )
    endif()
    if(SONGBIRD_TEST_TEST_TIMEOUT)
        set_tests_properties(${test_name} PROPERTIES
            TIMEOUT "${SONGBIRD_TEST_TEST_TIMEOUT}"
        )
    endif()
    if(SONGBIRD_TEST_TEST_ENVIRONMENT)
        set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT "${SONGBIRD_TEST_TEST_ENVIRONMENT}"
        )
    endif()

    if(QT_VERSION_MAJOR GREATER_EQUAL 6)
        qt_finalize_executable(${target_name})
    endif()
endfunction()

set(SONGBIRD_SHARE_URL_TEST_SOURCES
    ShareUrlTests.cpp
    ../src/subscription/ShareUrlBuilder.h
    ../src/subscription/ShareUrlBuilder.cpp
    ../src/subscription/ShareUrlProtocolBuilders.h
    ../src/subscription/ShareUrlProtocolBuilders.cpp
    ../src/subscription/ShareUrlBuilderSupport.h
    ../src/subscription/ShareUrlBuilderSupport.cpp
)

songbird_add_qt_test(songbird_share_url_tests share-url-transports
    SOURCES
        ${SONGBIRD_SHARE_URL_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

set(SONGBIRD_ADD_SERVER_DIALOG_TEST_SOURCES
    AddServerDialogTests.cpp
    ../src/ui/dialogs/AddServerDialog.h
    ../src/ui/dialogs/AddServerDialog.cpp
    ../src/ui/dialogs/AddServerDialogServerMapping.cpp
    ../src/ui/dialogs/AddServerDialogUi.cpp
    ../src/ui/dialogs/AddServerDialogOptions.h
    ../src/ui/dialogs/AddServerDialogOptions.cpp
)

songbird_add_qt_test(songbird_add_server_dialog_tests add-server-dialog-roundtrip
    SOURCES
        ${SONGBIRD_ADD_SERVER_DIALOG_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

# The UWP loopback dialog is the one place in the app that drives CheckNetIsolation.exe, and it
# had no coverage at all until it gained a seam: the dialog constructed the service itself, so a
# test could only ever have run against the machine's real package list. The target drives it
# through a stand-in IUwpLoopbackService, which is the only way to reach its failure branches and
# the only way to assert which of the two apply paths (one elevated batch, or one call per
# package) a given elevation state selects.
set(SONGBIRD_UWP_LOOPBACK_DIALOG_TEST_SOURCES
    UwpLoopbackDialogTests.cpp
    ../src/common/DialogUtils.h
    ../src/common/UiLanguage.h
    ../src/platform/IUwpLoopbackService.h
    ../src/platform/UwpPackageInfo.h
    ../src/ui/dialogs/UwpLoopbackDialog.h
    ../src/ui/dialogs/UwpLoopbackDialog.cpp
    ../src/ui/dialogs/UwpLoopbackDialogSupport.h
    ../src/ui/dialogs/UwpLoopbackDialogSupport.cpp
)

songbird_add_qt_test(songbird_uwp_loopback_dialog_tests uwp-loopback-dialog-apply
    SOURCES
        ${SONGBIRD_UWP_LOOPBACK_DIALOG_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_SETTINGS_DIALOG_TEST_SOURCES
    SettingsDialogTests.cpp
    ../src/ui/dialogs/CoreSettingsPageWidget.h
    ../src/ui/dialogs/CoreSettingsPageWidget.cpp
    ../src/ui/dialogs/CoreSettingsPageWidgetUi.cpp
    ../src/ui/dialogs/CoreSettingsPageSupport.h
    ../src/ui/dialogs/CoreSettingsPageSupport.cpp
    ../src/ui/dialogs/DnsSettingsPageWidget.h
    ../src/ui/dialogs/DnsSettingsPageWidget.cpp
    ../src/ui/dialogs/GeneralSettingsPageWidget.h
    ../src/ui/dialogs/GeneralSettingsPageWidget.cpp
    ../src/ui/dialogs/SubscriptionSettingsPageWidget.h
    ../src/ui/dialogs/SubscriptionSettingsPageWidget.cpp
    ../src/ui/dialogs/SettingsDialog.h
    ../src/ui/dialogs/SettingsDialog.cpp
    ../src/ui/dialogs/TunSettingsPageWidget.h
    ../src/ui/dialogs/TunSettingsPageWidget.cpp
)

songbird_add_qt_test(songbird_settings_dialog_tests settings-dialog-download
    SOURCES
        ${SONGBIRD_SETTINGS_DIALOG_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Svg
)

set(SONGBIRD_UI_PRESENTATION_LOGIC_TEST_SOURCES
    UiPresentationLogicTests.cpp
    # CoreSettingsPageSupport lives in SONGBIRD_SOURCES, not the shared object
    # library, so the one production file it needs is compiled in here -- the same
    # pattern SettingsDialogTests uses for the rest of the settings pages.
    ../src/ui/dialogs/CoreSettingsPageSupport.h
    ../src/ui/dialogs/CoreSettingsPageSupport.cpp
)

# ServerTableResultStyle.h and AppTheme's theme resolution are pure, I/O-free
# helpers, so they get their own target instead of dragging in a dialog. Those two
# are already in songbird_shared_objs; only the test file and the one
# non-shared production file are listed here.
songbird_add_qt_test(songbird_ui_presentation_logic_tests ui-presentation-logic
    SOURCES
        ${SONGBIRD_UI_PRESENTATION_LOGIC_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

# The SongBirdAuto window has a palette of its own (blue accent) rather than the main window's
# (green), and this pins that the separation is deliberate. AutoWindowPalette.cpp lives in
# SONGBIRD_AUTO_SOURCES, not songbird_shared_objs, so it is compiled into this target explicitly.
songbird_add_qt_test(songbird_auto_window_palette_tests auto-window-palette
    SOURCES
        AutoWindowPaletteTests.cpp
        ../src/auto/AutoWindowPalette.h
        ../src/auto/AutoWindowPalette.cpp
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Gui
)

set(SONGBIRD_TUN_SETTINGS_APPLY_DECISION_TEST_SOURCES
    TunSettingsApplyDecisionTests.cpp
)

songbird_add_qt_test(songbird_tun_settings_apply_decision_tests tun-settings-apply-decision
    SOURCES
        ${SONGBIRD_TUN_SETTINGS_APPLY_DECISION_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

# The one rule for "which port would this config actually use". It is a pure header, so this target
# needs nothing but its own file -- and it exists because the fallback used to be re-derived at
# every call site, which is how a port comparison and the spin box ended up disagreeing.
songbird_add_qt_test(songbird_local_port_option_tests local-port-option
    SOURCES
        LocalPortOptionTests.cpp
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

set(SONGBIRD_SETTINGS_DIALOG_APPLY_PLAN_TEST_SOURCES
    SettingsDialogApplyPlanTests.cpp
)

songbird_add_qt_test(songbird_settings_dialog_apply_plan_tests settings-dialog-apply-plan
    SOURCES
        ${SONGBIRD_SETTINGS_DIALOG_APPLY_PLAN_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_SETTINGS_APPLY_COORDINATOR_TEST_SOURCES
    SettingsApplyCoordinatorTests.cpp
    ../src/app/SettingsApplyCoordinator.h
    ../src/app/SettingsApplyCoordinator.cpp
    ../src/app/SettingsDialogApplyPlan.h
    ../src/app/TunAdminMessages.h
    ../src/platform/IAutoRunService.h
)

songbird_add_qt_test(songbird_settings_apply_coordinator_tests settings-apply-coordinator
    SOURCES
        ${SONGBIRD_SETTINGS_APPLY_COORDINATOR_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_SERVER_COLLECTION_COORDINATOR_TEST_SOURCES
    ServerCollectionCoordinatorTests.cpp
    ../src/app/ServerCollectionCoordinator.h
    ../src/app/ServerCollectionCoordinator.cpp
)

# Pins the coordinator's reaction to a removal that was applied but leaked a managed config file:
# ServerService::removeServers() reports that combination as a failure, and gating the core work on
# that failure used to leave the core running a server the user had just deleted.
songbird_add_qt_test(songbird_server_collection_coordinator_tests server-collection-coordinator
    SOURCES
        ${SONGBIRD_SERVER_COLLECTION_COORDINATOR_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

# isSingBoxRuleSetPublishedUpstream() is the list of rule-set categories the vendor does not
# publish, so a rule naming one of them can never match. It is pure data with no I/O, so it gets
# a target of its own rather than dragging in the whole sing-box config chain.
set(SONGBIRD_SINGBOX_RULE_SET_AVAILABILITY_TEST_SOURCES
    SingBoxRuleSetAvailabilityTests.cpp
    ../src/backends/singbox/SingBoxRuleSetAvailability.h
)

songbird_add_qt_test(songbird_singbox_rule_set_availability_tests singbox-rule-set-availability
    SOURCES
        ${SONGBIRD_SINGBOX_RULE_SET_AVAILABILITY_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_STARTUP_ADMIN_ELEVATION_TEST_SOURCES
    StartupAdminElevationTests.cpp
)

songbird_add_qt_test(songbird_startup_admin_elevation_tests startup-admin-elevation
    SOURCES
        ${SONGBIRD_STARTUP_ADMIN_ELEVATION_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_APP_BOOTSTRAP_TUN_RUNTIME_TEST_SOURCES
    AppBootstrapTunRuntimeTests.cpp
    TestSupport.h
    ../src/backends/singbox/SingBoxCoreDescriptor.h
    ../src/backends/xray/XrayCoreDescriptor.h
)

songbird_add_qt_test(songbird_app_bootstrap_tun_runtime_tests app-bootstrap-tun-runtime
    SOURCES
        ${SONGBIRD_APP_BOOTSTRAP_TUN_RUNTIME_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

# Any target that compiles ProxySession.cpp needs the runtime/backend chain below. It lives in a
# variable of its own so the system-proxy coordinator tests can reuse it rather than copying ~115
# lines -- a copy would drift the moment either side gains a file.
set(SONGBIRD_PROXY_RUNTIME_TEST_DEPENDENCIES
    ../src/backends/singbox/SingBoxCoreDescriptor.h
    ../src/backends/xray/XrayCoreDescriptor.h
)

set(SONGBIRD_PROXY_SESSION_TEST_SOURCES
    ProxySessionTests.cpp
    ${SONGBIRD_PROXY_RUNTIME_TEST_DEPENDENCIES}
)

songbird_add_qt_test(songbird_proxy_session_tests proxy-session-state
    SOURCES
        ${SONGBIRD_PROXY_SESSION_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

# The only suite that exercises the real QProcess-backed host: the behaviour it covers is how Qt
# drives a child process on Windows, which no stand-in can reproduce.
set(SONGBIRD_CORE_PROCESS_HOST_TEST_SOURCES
    CoreProcessHostTests.cpp
    ../src/common/CorePidFile.h
)

songbird_add_qt_test(songbird_core_process_host_tests core-process-host
    SOURCES
        ${SONGBIRD_CORE_PROCESS_HOST_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

# Guards the tab bar's scroll-button arrows. They are the one place in the app where a QToolButton is
# only 16px wide, which is exactly the width at which the unscoped QToolButton padding in
# AppTheme.qss.in wipes out the arrow's contents rect, so the buttons silently degrade to empty boxes.
set(SONGBIRD_TAB_BAR_SCROLL_BUTTON_TEST_SOURCES
    TabBarScrollButtonTests.cpp
)

songbird_add_qt_test(songbird_tab_bar_scroll_button_tests tab-bar-scroll-buttons
    SOURCES
        ${SONGBIRD_TAB_BAR_SCROLL_BUTTON_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Svg
)

# The bypass list both front ends write through ISystemProxyService::update(). Shared because the
# two copies drifted apart; this target is what keeps the shared resolver honest on its own, without
# standing up a coordinator or a fake service.
#
# No production sources are listed here. `SystemProxyExceptions::resolve()` does read the routing
# rules -- which is what pulls in RoutingConfigFragments and, through it, the sing-box DNS builders --
# but that whole chain already lives in SONGBIRD_SHARED_SOURCES, and songbird_add_qt_test() links
# songbird_shared_objs. Listing it again compiles a second copy of each symbol into the executable:
# ld reports "multiple definition of RoutingConfigFragments::locationProbeTag()", and MSVC reports
# LNK2005. The dependency is real; the source list is what has to stay empty.
songbird_add_qt_test(songbird_system_proxy_exceptions_tests system-proxy-exceptions
    SOURCES
        SystemProxyExceptionsTests.cpp
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

# Drives SystemProxyCoordinator through a stand-in ISystemProxyService, which is the only way to
# reach its failure branch. It needs ProxySession.cpp because the coordinator also calls into the
# session on the exit paths, even though the tests under test here (updateMode) do not.
set(SONGBIRD_SYSTEM_PROXY_COORDINATOR_TEST_SOURCES
    SystemProxyCoordinatorTests.cpp
    ../src/app/SystemProxyCoordinator.h
    ../src/app/SystemProxyCoordinator.cpp
    ${SONGBIRD_PROXY_RUNTIME_TEST_DEPENDENCIES}
)

songbird_add_qt_test(songbird_system_proxy_coordinator_tests system-proxy-coordinator
    SOURCES
        ${SONGBIRD_SYSTEM_PROXY_COORDINATOR_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

# The concrete WindowsSystemProxyService, driven against a scratch registry key. It needs its own
# target because it cannot share the coordinator's stand-in: the point here is the real registry
# write, including the result it reports when that write fails. The service takes the key as a
# constructor argument for exactly this reason -- the real Internet Settings key may not be
# mutated by a test run, and pointing the service at a non-registry path is the only way to reach
# its failure branch.
set(SONGBIRD_SYSTEM_PROXY_SERVICE_TEST_SOURCES
    SystemProxyServiceTests.cpp
    ../src/platform/windows/WindowsSystemProxyService.h
)

# Windows-only, like the class under test: it drives the real registry write and asserts on
# the value it reports when that write fails. The macOS implementation is covered through
# MacSystemProxyService instead, which is a different target because it has no registry to
# point at a scratch key.
if(WIN32)
    songbird_add_qt_test(songbird_system_proxy_service_tests system-proxy-service
        SOURCES
            ${SONGBIRD_SYSTEM_PROXY_SERVICE_TEST_SOURCES}
        LIBRARIES
            Qt${QT_VERSION_MAJOR}::Test
    )
endif()

# The two macOS platform implementations, compiled and driven on every platform.
#
# They are added to the application build only under APPLE (see src/CMakeLists.txt), and the
# release workflow's macOS job builds with BUILD_TEST=OFF, so this target is what keeps them
# compiling everywhere -- on Windows it is the only thing that compiles them at all. The test
# drives only the free functions they expose -- the services themselves write the real HOME
# directory and shell out to networksetup/osascript/launchctl.
#
# Which file this target owns is the exact complement of where the application build takes it
# from, because on macOS adding it here as well would define its symbols twice:
#   MacSystemProxyService -> songbird_shared_objs (linked by every test target) under APPLE
#   MacAutoRunService     -> the songbird executable's own sources under APPLE
set(SONGBIRD_MACOS_PLATFORM_SERVICES_TEST_SOURCES
    MacOSPlatformServicesTests.cpp
)

if(APPLE)
    list(APPEND SONGBIRD_MACOS_PLATFORM_SERVICES_TEST_SOURCES
        ../src/platform/macos/MacAutoRunService.h
        ../src/platform/macos/MacAutoRunService.cpp
    )
else()
    list(APPEND SONGBIRD_MACOS_PLATFORM_SERVICES_TEST_SOURCES
        ../src/platform/macos/MacSystemProxyService.h
        ../src/platform/macos/MacSystemProxyService.cpp
        ../src/platform/macos/MacAutoRunService.h
        ../src/platform/macos/MacAutoRunService.cpp
    )
endif()

songbird_add_qt_test(songbird_macos_platform_services_tests macos-platform-services
    SOURCES
        ${SONGBIRD_MACOS_PLATFORM_SERVICES_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

# The bounded shutdown wait, plus the tracker that uses it. Both live in songbird_shared_objs, so
# this target needs only its own file. The budgets are injected rather than waited out -- the
# production hard cap is 30 s, which no test can afford -- and one case re-runs this executable as
# a subprocess, because the give-up path ends the process and cannot be observed from inside it.
songbird_add_qt_test(songbird_thread_shutdown_tests thread-shutdown
    SOURCES
        ThreadShutdownTests.cpp
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

# The outbound location probe. Its timing constants are injectable for the same reason the
# shutdown budgets are: the production values add up to twelve seconds before the deadline branch
# is reached. The case that matters is the failing one -- a proxy that answers the plain http
# probes with a body the parser rejects while holding the CONNECT tunnels open used to report only
# "timed out", throwing away every reason the hosts had already given.
songbird_add_qt_test(songbird_outbound_location_probe_tests outbound-location-probe
    SOURCES
        OutboundLocationProbeServiceTests.cpp
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Network
)

# The startup gate. The case that matters is the one the old single-probe design got wrong: every
# https host failing must not sink the verdict while a plain http host still answers, because the
# verdict is a status code and never a body.
songbird_add_qt_test(songbird_outbound_liveness_probe_tests outbound-liveness-probe
    SOURCES
        OutboundLivenessProbeServiceTests.cpp
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Network
)

# The shape a failed outbound leaves behind: certificate mismatch, an unreachable node, or a tunnel
# that carried nothing. It is pure text logic over the wording the cores and Qt already print, so it
# gets a target of its own rather than being asserted through a live probe.
songbird_add_qt_test(songbird_outbound_error_classifier_tests outbound-error-classifier
    SOURCES
        OutboundErrorClassifierTests.cpp
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

# The Active rule itself. It lives in appcore/ as a free function precisely so it can be tested:
# RuntimeStateSnapshotBuilder is in app/ and only compiled into the app targets, so a rule that
# stayed there would never be reachable from a test.
songbird_add_qt_test(songbird_proxy_ui_state_decision_tests proxy-ui-state-decision
    SOURCES
        ProxyUiStateDecisionTests.cpp
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

set(SONGBIRD_CORE_UPDATE_COORDINATOR_TEST_SOURCES
    CoreUpdateCoordinatorTests.cpp
    ../src/app/CoreUpdateCoordinator.h
    ../src/app/CoreUpdateCoordinator.cpp
    ../src/app/CoreUpdatePendingState.h
    ../src/app/CoreUpdatePendingState.cpp
    ../src/backends/singbox/SingBoxCoreDescriptor.h
    ../src/backends/xray/XrayCoreDescriptor.h
)

songbird_add_qt_test(songbird_core_update_coordinator_tests core-update-coordinator
    SOURCES
        ${SONGBIRD_CORE_UPDATE_COORDINATOR_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

set(SONGBIRD_RUNTIME_STATE_TEST_SOURCES
    RuntimeStateTests.cpp
)

songbird_add_qt_test(songbird_runtime_state_tests runtime-state
    SOURCES
        ${SONGBIRD_RUNTIME_STATE_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

set(SONGBIRD_MAIN_WINDOW_TEST_SOURCES
    MainWindowTests.cpp
    TestSupport.h
    ../src/subscription/ShareUrlBuilder.h
    ../src/subscription/ShareUrlBuilder.cpp
    ../src/subscription/ShareUrlProtocolBuilders.h
    ../src/subscription/ShareUrlProtocolBuilders.cpp
    ../src/subscription/ShareUrlBuilderSupport.h
    ../src/subscription/ShareUrlBuilderSupport.cpp
    ../src/third_party/qrcodegen/qrcodegen.hpp
    ../src/third_party/qrcodegen/qrcodegen.cpp
    ../src/ui/mainwindow/MainWindow.h
    ../src/ui/mainwindow/MainWindow.cpp
    ../src/ui/mainwindow/LogPanelWidget.h
    ../src/ui/mainwindow/LogPanelWidget.cpp
    ../src/ui/mainwindow/LogPanelSupport.h
    ../src/ui/mainwindow/LogPanelSupport.cpp
    ../src/ui/mainwindow/BackgroundTaskActionsController.h
    ../src/ui/mainwindow/BackgroundTaskActionsController.cpp
    ../src/ui/mainwindow/RoutingModeController.h
    ../src/ui/mainwindow/RoutingModeController.cpp
    ../src/ui/mainwindow/SubscriptionViewSupport.h
    ../src/ui/mainwindow/SubscriptionViewSupport.cpp
    ../src/ui/mainwindow/SubscriptionViewController.h
    ../src/ui/mainwindow/SubscriptionViewController.cpp
    ../src/ui/mainwindow/ServerWorkspaceWidget.h
    ../src/ui/mainwindow/ServerWorkspaceWidget.cpp
    ../src/ui/mainwindow/ServerEmptyStatePolicy.h
    ../src/ui/mainwindow/ServerTableEmptyStateWidget.h
    ../src/ui/mainwindow/ServerTableEmptyStateWidget.cpp
    ../src/ui/mainwindow/MainWindowShortcutController.h
    ../src/ui/mainwindow/MainWindowShortcutController.cpp
    ../src/ui/mainwindow/MainWindowTitleSupport.h
    ../src/ui/mainwindow/MainWindowTitleSupport.cpp
    ../src/ui/mainwindow/MainWindowToolbarActions.h
    ../src/ui/mainwindow/MainWindowToolbarActions.cpp
    ../src/ui/dialogs/ShortcutsCheatSheetDialog.h
    ../src/ui/dialogs/ShortcutsCheatSheetDialog.cpp
    ../src/ui/mainwindow/MainWindowConfigState.h
    ../src/ui/mainwindow/MainWindowConfigState.cpp
    ../src/ui/mainwindow/MainWindowServerSelectionSupport.h
    ../src/ui/mainwindow/MainWindowServerSelectionSupport.cpp
    ../src/ui/mainwindow/WindowLayoutStateSupport.h
    ../src/ui/mainwindow/WindowLayoutStateSupport.cpp
    ../src/ui/mainwindow/WindowLayoutStateController.h
    ../src/ui/mainwindow/WindowLayoutStateController.cpp
    ../src/ui/mainwindow/ServerListController.h
    ../src/ui/mainwindow/ServerListController.cpp
    ../src/ui/mainwindow/SharePanelWidget.h
    ../src/ui/mainwindow/SharePanelWidget.cpp
    ../src/ui/mainwindow/StartupOverlayWidget.h
    ../src/ui/mainwindow/StartupOverlayWidget.cpp
    ../src/ui/mainwindow/ProxyToolbarController.h
    ../src/ui/mainwindow/ProxyToolbarController.cpp
    ../src/ui/mainwindow/ServerSelectionController.h
    ../src/ui/mainwindow/ServerSelectionController.cpp
    ../src/ui/mainwindow/ServerActionsController.h
    ../src/ui/mainwindow/ServerActionsController.cpp
    ../src/ui/mainwindow/StatusBarSupport.h
    ../src/ui/mainwindow/StatusBarSupport.cpp
    ../src/ui/mainwindow/StatusBarController.h
    ../src/ui/mainwindow/StatusBarController.cpp
    ../src/ui/mainwindow/ToolbarWidgets.h
    ../src/ui/mainwindow/ToolbarWidgets.cpp
    ../src/ui/mainwindow/LogItemDelegate.h
    ../src/ui/mainwindow/LogItemDelegate.cpp
    ../src/ui/mainwindow/ServerTableView.h
    ../src/ui/mainwindow/ServerTableView.cpp
    ../src/ui/tray/TrayMenuSupport.h
    ../src/ui/tray/TrayMenuSupport.cpp
    ../src/ui/models/LogFilterProxyModel.h
    ../src/ui/models/LogFilterProxyModel.cpp
    ../src/ui/models/LogListModel.h
    ../src/ui/models/LogListModel.cpp
    ../src/ui/models/ServerFilterProxyModel.h
    ../src/ui/models/ServerFilterProxyModel.cpp
    ../src/ui/models/ServerTableModel.h
    ../src/ui/models/ServerTableModel.cpp
    ../src/ui/models/ServerTableResultStyle.h
    ../src/ui/qr/QrCodeRenderer.h
    ../src/ui/qr/QrCodeRenderer.cpp
    ../src/resources/toolbar_icons.qrc
)

songbird_add_qt_test(songbird_main_window_tests main-window-log-scroll
    SOURCES
        ${SONGBIRD_MAIN_WINDOW_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Svg
)

set(SONGBIRD_BACKEND_STACK_SOURCES

)

set(SONGBIRD_CLIENT_CONFIG_WRITER_TEST_SOURCES
    ClientConfigWriterTests.cpp
    ${SONGBIRD_BACKEND_STACK_SOURCES}
)

set(SONGBIRD_BACKEND_CONTRACT_TEST_SOURCES
    BackendContractTests.cpp
    ${SONGBIRD_BACKEND_STACK_SOURCES}
)

songbird_add_qt_test(songbird_client_config_writer_tests client-config-writer-tun-compat
    SOURCES
        ${SONGBIRD_CLIENT_CONFIG_WRITER_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

songbird_add_qt_test(songbird_xray_tls_compatibility_tests xray-tls-compatibility
    SOURCES XrayTlsCompatibilityTests.cpp
    LIBRARIES Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network
)

songbird_add_qt_test(songbird_backend_contract_tests backend-contract
    SOURCES
        ${SONGBIRD_BACKEND_CONTRACT_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_TUN_COMPAT_CORE_REQUIREMENT_TEST_SOURCES
    TunCompatCoreRequirementTests.cpp
)

songbird_add_qt_test(songbird_tun_compat_core_requirement_tests tun-compat-core-requirement
    SOURCES
        ${SONGBIRD_TUN_COMPAT_CORE_REQUIREMENT_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_JSON_CONFIG_REPOSITORY_TEST_SOURCES
    JsonConfigRepositoryTests.cpp
)

songbird_add_qt_test(songbird_json_config_repository_tests json-config-repository-defaults
    SOURCES
        ${SONGBIRD_JSON_CONFIG_REPOSITORY_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_CONFIG_BACKUP_STATE_DOCUMENT_TEST_SOURCES
    ConfigBackupStateDocumentTests.cpp
    ../src/services/ConfigBackupStateDocument.h
    ../src/services/ConfigBackupStateDocument.cpp
)

songbird_add_qt_test(songbird_config_backup_state_document_tests config-backup-state-document
    SOURCES
        ${SONGBIRD_CONFIG_BACKUP_STATE_DOCUMENT_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_PROXY_AVAILABILITY_CHECK_TEST_SOURCES
    ProxyAvailabilityCheckServiceTests.cpp
)

songbird_add_qt_test(songbird_proxy_availability_check_tests proxy-availability-check
    SOURCES
        ${SONGBIRD_PROXY_AVAILABILITY_CHECK_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

# The port policy is a header-only pure function set, so this target needs nothing beyond the
# shared object library every test already links.
songbird_add_qt_test(songbird_local_inbound_ports_tests local-inbound-ports
    SOURCES
        LocalInboundPortsTests.cpp
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

set(SONGBIRD_SPEED_TEST_SERVICE_INTERNAL_TEST_SOURCES
    SpeedTestServiceInternalTests.cpp
)

songbird_add_qt_test(songbird_speed_test_service_internal_tests speed-test-service-internal
    SOURCES
        ${SONGBIRD_SPEED_TEST_SERVICE_INTERNAL_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

set(SONGBIRD_SPEED_TEST_COORDINATOR_LOGIC_TEST_SOURCES
    SpeedTestCoordinatorLogicTests.cpp
    ../src/app/SpeedTestCoordinatorLogic.h
)

songbird_add_qt_test(songbird_speed_test_coordinator_logic_tests speed-test-coordinator-logic
    SOURCES
        ${SONGBIRD_SPEED_TEST_COORDINATOR_LOGIC_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_SUBSCRIPTION_SERVICE_TEST_SOURCES
    SubscriptionServiceTests.cpp
)

songbird_add_qt_test(songbird_subscription_service_tests subscription-service
    SOURCES
        ${SONGBIRD_SUBSCRIPTION_SERVICE_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

# Subscription order is presentation state that happens to live in the config file, so it is tested
# twice over: the pure move rules (which is where the edge cases are -- a selection already at an
# edge, a scattered selection, every row selected) and the service that persists the result. The
# service half is only reachable here because the repository is an interface.
set(SONGBIRD_SUBSCRIPTION_ORDER_TEST_SOURCES
    SubscriptionOrderTests.cpp
)

songbird_add_qt_test(songbird_subscription_order_tests subscription-order
    SOURCES
        ${SONGBIRD_SUBSCRIPTION_ORDER_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

# Tests NodeFingerprint dedup logic, ClashYamlBuilder output, ExportService
# URI/YAML/dedup operations, and round-trips through ClashProxyItemParser.
# ExportService and ShareUrlBuilder* live in SONGBIRD_SOURCES (not shared_objs),
# so list them explicitly here alongside the test file.
set(SONGBIRD_EXPORT_SERVICE_TEST_SOURCES
    ExportServiceTests.cpp
    ../src/services/ExportService.h
    ../src/services/ExportService.cpp
    ../src/subscription/ShareUrlBuilder.h
    ../src/subscription/ShareUrlBuilder.cpp
    ../src/subscription/ShareUrlBuilderSupport.h
    ../src/subscription/ShareUrlBuilderSupport.cpp
    ../src/subscription/ShareUrlProtocolBuilders.h
    ../src/subscription/ShareUrlProtocolBuilders.cpp
)

songbird_add_qt_test(songbird_export_service_tests export-service
    SOURCES
        ${SONGBIRD_EXPORT_SERVICE_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Network
)

# SubscriptionUpdateOrder decides the order the serial update loop walks the batch in: it spreads
# subscriptions sharing a host apart and defers the ones that keep failing. Pure permutation logic,
# so it gets its own small target instead of dragging in the network stack.
set(SONGBIRD_SUBSCRIPTION_UPDATE_ORDER_TEST_SOURCES
    SubscriptionUpdateOrderTests.cpp
)

songbird_add_qt_test(songbird_subscription_update_order_tests subscription-update-order
    SOURCES
        ${SONGBIRD_SUBSCRIPTION_UPDATE_ORDER_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

# TerminalProxyPolicy holds the decisions behind the terminal-proxy environment variables: the URL
# written, the default bypass list, and the "is this value ours?" checks. The registry write and the
# window broadcast cannot be unit tested, which is why those decisions live here instead.
set(SONGBIRD_TERMINAL_PROXY_POLICY_TEST_SOURCES
    TerminalProxyPolicyTests.cpp
)

songbird_add_qt_test(songbird_terminal_proxy_policy_tests terminal-proxy-policy
    SOURCES
        ${SONGBIRD_TERMINAL_PROXY_POLICY_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

# WindowsTerminalProxyService's registry contract. The key is injectable for the same reason
# WindowsSystemProxyService's is: a ctest run must not leave the developer's own terminal pointed at a
# port that is not listening, and the real key cannot be made to refuse a write. The in-process
# environment mirror and the WM_SETTINGCHANGE broadcast are not testable and are documented as such.
if(WIN32)
    set(SONGBIRD_WINDOWS_TERMINAL_PROXY_SERVICE_TEST_SOURCES
        WindowsTerminalProxyServiceTests.cpp
    )

    songbird_add_qt_test(songbird_windows_terminal_proxy_service_tests windows-terminal-proxy-service
        SOURCES
            ${SONGBIRD_WINDOWS_TERMINAL_PROXY_SERVICE_TEST_SOURCES}
        LIBRARIES
            Qt${QT_VERSION_MAJOR}::Test
    )
endif()

# HealthStripPolicy folds probe records into the 24-hour strip shown beside a server. Pure integer
# logic over an explicitly-supplied clock, so it gets its own target and needs no timer.
set(SONGBIRD_HEALTH_STRIP_POLICY_TEST_SOURCES
    HealthStripPolicyTests.cpp
)

songbird_add_qt_test(songbird_health_strip_policy_tests health-strip-policy
    SOURCES
        ${SONGBIRD_HEALTH_STRIP_POLICY_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

# The empty-state decision is pure: it turns "is there a subscription on record" plus the row
# counts into the one state the panel renders. Split out from MainWindowTests because the bug
# it pins (an enabled subscription with no nodes looked like a fresh install) is a decision, not
# a widget, and every branch is cheaper to assert here than through a shown window.
set(SONGBIRD_EMPTY_STATE_POLICY_TEST_SOURCES
    ServerEmptyStatePolicyTests.cpp
)

songbird_add_qt_test(songbird_server_empty_state_policy_tests server-empty-state-policy
    SOURCES
        ${SONGBIRD_EMPTY_STATE_POLICY_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

# Drives SubscriptionUpdateService through a local TCP stub server to cover the
# conditional-GET (ETag/Last-Modified), 304 short-circuit, retry back-off, and
# fail-streak paths without making real network requests.
set(SONGBIRD_SUBSCRIPTION_UPDATE_SERVICE_TEST_SOURCES
    SubscriptionUpdateServiceTests.cpp
)

songbird_add_qt_test(songbird_subscription_update_service_tests subscription-update-service
    SOURCES
        ${SONGBIRD_SUBSCRIPTION_UPDATE_SERVICE_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

set(SONGBIRD_SUBSCRIPTION_URL_IMPORT_SERVICE_TEST_SOURCES
    SubscriptionUrlImportServiceTests.cpp
    ../src/subscription/SubscriptionUrlImportService.h
    ../src/subscription/SubscriptionUrlImportService.cpp
)

songbird_add_qt_test(songbird_subscription_url_import_service_tests subscription-url-import-service
    SOURCES
        ${SONGBIRD_SUBSCRIPTION_URL_IMPORT_SERVICE_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

set(SONGBIRD_ROUTING_SERVICE_TEST_SOURCES
    RoutingServiceTests.cpp
    ../src/services/RoutingService.h
    ../src/services/RoutingService.cpp
)

songbird_add_qt_test(songbird_routing_service_tests routing-service
    SOURCES
        ${SONGBIRD_ROUTING_SERVICE_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_SYSTEM_PROXY_MODE_TEST_SOURCES
    SystemProxyModeTests.cpp
)

songbird_add_qt_test(songbird_system_proxy_mode_tests system-proxy-mode
    SOURCES
        ${SONGBIRD_SYSTEM_PROXY_MODE_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

# The ownership rules -- "did *we* change the system proxy?" -- plus the snapshot's three-state
# contract. Both are pure logic, which is the point: the defect they were extracted from was a
# boolean that had the wrong operator, and it was invisible because it sat inside a teardown
# routine that could only be reached by failing a real startup.
set(SONGBIRD_SYSTEM_PROXY_OWNERSHIP_TEST_SOURCES
    SystemProxyOwnershipPolicyTests.cpp
)

songbird_add_qt_test(songbird_system_proxy_ownership_tests system-proxy-ownership
    SOURCES
        ${SONGBIRD_SYSTEM_PROXY_OWNERSHIP_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

# LogSeverity decides the marker the information panel prints in front of every
# line, and logSeverityFromCoreOutput reads the level a core already printed in
# its own line. Both are pure string logic over three cores' log formats, so they
# get a target of their own instead of dragging in the panel.
#
# Only the header is listed: LogSeverity.cpp is in SONGBIRD_SHARED_SOURCES, and
# every test target already links songbird_shared_objs, so compiling it here as
# well would define logSeverityMarker twice.
set(SONGBIRD_LOG_SEVERITY_TEST_SOURCES
    LogSeverityTests.cpp
    ../src/common/LogSeverity.h
)

songbird_add_qt_test(songbird_log_severity_tests log-severity
    SOURCES
        ${SONGBIRD_LOG_SEVERITY_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

# The file the panel's lines are also appended to. The two rules that matter --
# rotation within a day, and which files retention is allowed to delete -- are pure
# functions over file names, and the writer takes both its directory and its clock
# as parameters, so every case runs against a temporary directory with a clock the
# test moves by hand rather than against the real application data directory.
#
# Only the header is listed, for the same reason as above: LogFileWriter.cpp is in
# SONGBIRD_SHARED_SOURCES, and every test target already links songbird_shared_objs.
set(SONGBIRD_LOG_FILE_WRITER_TEST_SOURCES
    LogFileWriterTests.cpp
    ../src/common/LogFileWriter.h
)

songbird_add_qt_test(songbird_log_file_writer_tests log-file-writer
    SOURCES
        ${SONGBIRD_LOG_FILE_WRITER_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

# StartupSequencer is the abort-on-failure primitive AppBootstrap::run() drives its startup
# tail through, and decideStartupWindowAction is the tray-less fallback branch that used to be
# an untested nested if inside run(). Both are pure and dependency-free, so they get a target of
# their own rather than dragging in the whole AppBootstrap link chain.
set(SONGBIRD_STARTUP_SEQUENCER_TEST_SOURCES
    StartupSequencerTests.cpp
    ../src/common/StartupSequencer.h
    ../src/app/StartupBanner.h
    ../src/app/StartupCorePromptPolicy.h
    ../src/app/StartupWindowPolicy.h
)

songbird_add_qt_test(songbird_startup_sequencer_tests startup-sequencer
    SOURCES
        ${SONGBIRD_STARTUP_SEQUENCER_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

# ProxyCrashRestartPolicy.cpp used to be compiled only as a member of
# SONGBIRD_PROXY_RUNTIME_TEST_DEPENDENCIES, where no test case ever referenced it -- so it
# linked into two binaries and asserted nothing. It is pure decision logic with no I/O, so it
# gets a small target of its own rather than dragging in the ProxySession chain.
set(SONGBIRD_PROXY_CRASH_RESTART_POLICY_TEST_SOURCES
    ProxyCrashRestartPolicyTests.cpp
)

songbird_add_qt_test(songbird_proxy_crash_restart_policy_tests proxy-crash-restart-policy
    SOURCES
        ${SONGBIRD_PROXY_CRASH_RESTART_POLICY_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

# ProxyHealthWatchPolicy is the decision table behind the main app's node health watch, the same
# shape as ProxyCrashRestartPolicy above and split out for the same reason: ProxySession owns the
# timer and the probe, the policy owns only the decision. It is pure, so it gets a small target of
# its own rather than dragging in the ProxySession chain.
set(SONGBIRD_PROXY_HEALTH_WATCH_POLICY_TEST_SOURCES
    ProxyHealthWatchPolicyTests.cpp
)

songbird_add_qt_test(songbird_proxy_health_watch_policy_tests proxy-health-watch-policy
    SOURCES
        ${SONGBIRD_PROXY_HEALTH_WATCH_POLICY_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

# RoutingScorePolicy ranks candidate nodes: EWMA smoothing so one outlier does not reorder the
# list, a circuit breaker so a dead node cannot win on a stale score, and a switch margin so two
# nodes within noise of each other do not trade the active connection on every probe. Pure logic,
# so it gets its own target rather than dragging in the ProxySession chain.
set(SONGBIRD_ROUTING_SCORE_POLICY_TEST_SOURCES
    RoutingScorePolicyTests.cpp
)

songbird_add_qt_test(songbird_routing_score_policy_tests routing-score-policy
    SOURCES
        ${SONGBIRD_ROUTING_SCORE_POLICY_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_AUTO_COUNTRY_SELECTION_TEST_SOURCES
    AutoCountrySelectionTests.cpp
    ../src/auto/AutoCountrySelection.h
    ../src/auto/AutoCountrySelection.cpp
    ../src/auto/AutoCountrySupport.h
    ../src/auto/AutoCountrySupport.cpp
    ../src/auto/AutoTypes.h
)

songbird_add_qt_test(songbird_auto_country_selection_tests auto-country-selection
    SOURCES
        ${SONGBIRD_AUTO_COUNTRY_SELECTION_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_AUTO_COUNTRY_INFERENCE_TEST_SOURCES
    AutoCountryInferenceTests.cpp
    ../src/auto/AutoCountryInference.h
    ../src/auto/AutoCountryInference.cpp
    ../src/auto/AutoCountrySupport.h
    ../src/auto/AutoCountrySupport.cpp
    ../src/auto/AutoTypes.h
)

songbird_add_qt_test(songbird_auto_country_inference_tests auto-country-inference
    SOURCES
        ${SONGBIRD_AUTO_COUNTRY_INFERENCE_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_AUTO_RUNTIME_DEFAULTS_TEST_SOURCES
    AutoRuntimeDefaultsTests.cpp
    ../src/auto/AutoRuntimeDefaults.h
    ../src/auto/AutoRuntimeDefaults.cpp
)

songbird_add_qt_test(songbird_auto_runtime_defaults_tests auto-runtime-defaults
    SOURCES
        ${SONGBIRD_AUTO_RUNTIME_DEFAULTS_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_AUTO_COORDINATOR_LOGIC_TEST_SOURCES
    AutoCoordinatorLogicTests.cpp
    ../src/auto/AutoCoordinatorLogic.h
    ../src/auto/AutoCoordinatorLogic.cpp
    ../src/auto/AutoTypes.h
)

songbird_add_qt_test(songbird_auto_coordinator_logic_tests auto-coordinator-logic
    SOURCES
        ${SONGBIRD_AUTO_COORDINATOR_LOGIC_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

# The coordinator itself, not a pure helper: this suite exists because issue #12's deletion had no
# coverage -- the mutation check (put the ForcedClear back) left every suite green, since
# ProxySessionTests drives a session that cannot see the coordinator. It links the production
# coordinator and injects a recording ISystemProxyService.
set(SONGBIRD_AUTO_COORDINATOR_STOP_TEST_SOURCES
    SongBirdAutoCoordinatorStopTests.cpp
    ../src/auto/SongBirdAutoCoordinator.h
    ../src/auto/SongBirdAutoCoordinator.cpp
    ../src/auto/AutoCoordinatorLogic.h
    ../src/auto/AutoCoordinatorLogic.cpp
    ../src/auto/AutoCountryInference.h
    ../src/auto/AutoCountryInference.cpp
    ../src/auto/AutoCountrySelection.h
    ../src/auto/AutoCountrySelection.cpp
    ../src/auto/AutoCountrySupport.h
    ../src/auto/AutoCountrySupport.cpp
    ../src/auto/AutoNodeEvaluationService.h
    ../src/auto/AutoNodeEvaluationService.cpp
    ../src/auto/AutoRuntimeDefaults.h
    ../src/auto/AutoRuntimeDefaults.cpp
    ../src/auto/AutoTypes.h
)

songbird_add_qt_test(songbird_auto_coordinator_stop_tests auto-coordinator-stop
    SOURCES
        ${SONGBIRD_AUTO_COORDINATOR_STOP_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_USER_AGENT_TEST_SOURCES
    UserAgentTests.cpp
)

songbird_add_qt_test(songbird_user_agent_tests user-agent
    SOURCES
        ${SONGBIRD_USER_AGENT_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_UI_LANGUAGE_TEST_SOURCES
    UiLanguageTests.cpp
    ../src/common/UiLanguage.h
    ../src/common/DialogUtils.h
)

songbird_add_qt_test(songbird_ui_language_tests ui-language
    SOURCES
        ${SONGBIRD_UI_LANGUAGE_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)


set(SONGBIRD_APP_UPDATE_SERVICE_TEST_SOURCES
    AppUpdateServiceTests.cpp
    ../src/services/AppUpdateService.h
    ../src/services/AppUpdateService.cpp
    ../src/services/AppUpdateReleaseMetadata.h
    ../src/services/AppUpdateReleaseMetadata.cpp
    ../src/common/GitHubMirrorHelper.h
    ../src/common/GitHubUrls.h
)

songbird_add_qt_test(songbird_app_update_service_tests app-update-service
    SOURCES
        ${SONGBIRD_APP_UPDATE_SERVICE_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

set(SONGBIRD_APP_UPDATE_CHECK_COORDINATOR_TEST_SOURCES
    AppUpdateCheckCoordinatorTests.cpp
    ../src/app/AppUpdateCheckCoordinator.h
    ../src/app/AppUpdateCheckCoordinator.cpp
    ../src/app/AppUpdateCheckPresentation.h
    ../src/app/AppUpdateCheckPresentation.cpp
    ../src/services/AppUpdateService.h
    ../src/services/AppUpdateService.cpp
    ../src/services/AppUpdateReleaseMetadata.h
    ../src/services/AppUpdateReleaseMetadata.cpp
    ../src/common/GitHubMirrorHelper.h
    ../src/common/GitHubUrls.h
)

songbird_add_qt_test(songbird_app_update_check_coordinator_tests app-update-check-coordinator
    SOURCES
        ${SONGBIRD_APP_UPDATE_CHECK_COORDINATOR_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)



set(SONGBIRD_CORE_UPDATE_SERVICE_TEST_SOURCES
    CoreUpdateServiceTests.cpp
)

songbird_add_qt_test(songbird_core_update_service_tests core-update-service
    SOURCES
        ${SONGBIRD_CORE_UPDATE_SERVICE_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

set(SONGBIRD_GEO_RESOURCE_UPDATE_SERVICE_TEST_SOURCES
    GeoResourceUpdateServiceTests.cpp
)

songbird_add_qt_test(songbird_geo_resource_update_service_tests geo-resource-update-service
    SOURCES
        ${SONGBIRD_GEO_RESOURCE_UPDATE_SERVICE_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

set(SONGBIRD_SUBSCRIPTION_PARSER_TEST_SOURCES
    SubscriptionParserTests.cpp
)

songbird_add_qt_test(songbird_subscription_parser_tests subscription-parser
    SOURCES
        ${SONGBIRD_SUBSCRIPTION_PARSER_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

set(SONGBIRD_SERVER_SERVICE_TEST_SOURCES
    ServerServiceTests.cpp
)

songbird_add_qt_test(songbird_server_service_tests server-service
    SOURCES
        ${SONGBIRD_SERVER_SERVICE_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_PROTOCOL_CORE_COMPAT_TEST_SOURCES
    ProtocolCoreCompatTests.cpp
)

songbird_add_qt_test(songbird_protocol_core_compat_tests protocol-core-compat
    SOURCES
        ${SONGBIRD_PROTOCOL_CORE_COMPAT_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_END_TO_END_SMOKE_TEST_SOURCES
    EndToEndSmokeTests.cpp
    ../src/common/GitHubMirrorHelper.h
    ../src/common/GitHubUrls.h
    ../src/runtime/CoreLifecycleService.h
    ../src/runtime/CoreLifecycleService.cpp
    ../src/runtime/TunCompatCoreRequirement.h
    ../src/services/SpeedTestServiceInternal.h
    ../src/ui/dialogs/SubscriptionSettingsPageWidget.h
    ../src/ui/dialogs/SubscriptionSettingsPageWidget.cpp
)

songbird_add_qt_test(songbird_end_to_end_smoke_tests end-to-end-smoke
    SOURCES
        ${SONGBIRD_END_TO_END_SMOKE_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Svg
    TEST_LABELS "smoke"
    TEST_TIMEOUT 7200
    TEST_ENVIRONMENT "QT_QPA_PLATFORM=${SONGBIRD_TEST_QPA_PLATFORM}\;QTEST_FUNCTION_TIMEOUT=7200000"
)

if(WIN32)
    target_link_libraries(songbird_end_to_end_smoke_tests
        PRIVATE
            wininet
    )
endif()
