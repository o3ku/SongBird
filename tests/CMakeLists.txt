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
        ENVIRONMENT "QT_QPA_PLATFORM=windows"
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

songbird_add_qt_test(songbird_system_proxy_service_tests system-proxy-service
    SOURCES
        ${SONGBIRD_SYSTEM_PROXY_SERVICE_TEST_SOURCES}
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
    ../src/ui/mainwindow/MainWindowShortcutController.h
    ../src/ui/mainwindow/MainWindowShortcutController.cpp
    ../src/ui/mainwindow/MainWindowTitleSupport.h
    ../src/ui/mainwindow/MainWindowTitleSupport.cpp
    ../src/ui/mainwindow/MainWindowToolbarActions.h
    ../src/ui/mainwindow/MainWindowToolbarActions.cpp
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

# StartupSequencer is the abort-on-failure primitive AppBootstrap::run() drives its startup
# tail through, and decideStartupWindowAction is the tray-less fallback branch that used to be
# an untested nested if inside run(). Both are pure and dependency-free, so they get a target of
# their own rather than dragging in the whole AppBootstrap link chain.
set(SONGBIRD_STARTUP_SEQUENCER_TEST_SOURCES
    StartupSequencerTests.cpp
    ../src/common/StartupSequencer.h
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
    TEST_ENVIRONMENT "QT_QPA_PLATFORM=windows\;QTEST_FUNCTION_TIMEOUT=7200000"
)

if(WIN32)
    target_link_libraries(songbird_end_to_end_smoke_tests
        PRIVATE
            wininet
    )
endif()
