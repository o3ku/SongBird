find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test Widgets Network Svg)

link_libraries(songbird_core_descriptors)

set(SONGBIRD_COMMON_DOMAIN_TEST_SOURCES
    ../src/domain/models/RoutingProfiles.h
    ../src/domain/models/RoutingProfiles.cpp
)

find_program(SONGBIRD_POWERSHELL_EXECUTABLE NAMES pwsh powershell)
if(SONGBIRD_POWERSHELL_EXECUTABLE)
    add_test(
        NAME backend-boundaries
        COMMAND ${SONGBIRD_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/check-backend-boundaries.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}/../src
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

    target_sources(${target_name}
        PRIVATE
            ${SONGBIRD_COMMON_DOMAIN_TEST_SOURCES}
    )

    target_include_directories(${target_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    target_link_libraries(${target_name}
        PRIVATE
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
    ../src/subscription/ShareUrlParser.h
    ../src/subscription/ShareUrlParser.cpp
    ../src/subscription/ShareUrlProtocolParsers.h
    ../src/subscription/ShareUrlProtocolParsers.cpp
    ../src/subscription/ShareUrlSingBoxProtocolParsers.cpp
    ../src/subscription/ShareUrlVmessParser.h
    ../src/subscription/ShareUrlVmessParser.cpp
    ../src/subscription/ShareUrlParserSupport.h
    ../src/subscription/ShareUrlParserSupport.cpp
    ../src/subscription/SubscriptionImportTextParser.h
    ../src/subscription/SubscriptionImportTextParser.cpp
    ../src/common/PortValidator.h
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
    ../src/ui/dialogs/RoutingBaseRouteCard.h
    ../src/ui/dialogs/RoutingBaseRouteCard.cpp
    ../src/ui/dialogs/RoutingCustomRuleSupport.h
    ../src/ui/dialogs/RoutingCustomRuleSupport.cpp
    ../src/ui/dialogs/RoutingSettingsPageWidget.h
    ../src/ui/dialogs/RoutingSettingsPageWidget.cpp
    ../src/ui/dialogs/RoutingSettingsPageWidgetUi.cpp
    ../src/ui/dialogs/SubscriptionSettingsPageWidget.h
    ../src/ui/dialogs/SubscriptionSettingsPageWidget.cpp
    ../src/ui/dialogs/SettingsDialog.h
    ../src/ui/dialogs/SettingsDialog.cpp
    ../src/ui/dialogs/TunSettingsPageWidget.h
    ../src/ui/dialogs/TunSettingsPageWidget.cpp
    ../src/ui/theme/AppTheme.h
    ../src/ui/theme/AppTheme.cpp
    ../src/ui/theme/AppThemePalette.h
    ../src/ui/theme/AppThemePalette.cpp
    ../src/ui/theme/AppThemeStyle.h
    ../src/ui/theme/AppThemeStyle.cpp
    ../src/ui/theme/ServerTableTheme.h
    ../src/ui/theme/ServerTableTheme.cpp
    ../src/resources/themes.qrc
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
    ../src/app/CoreStartupCheckpoint.h
    ../src/runtime/CoreConfigPreflight.h
    ../src/runtime/CoreConfigPreflight.cpp
    ../src/runtime/DnsConfigFragments.h
    ../src/runtime/DnsConfigFragments.cpp
    ../src/runtime/DnsAddressParser.h
    ../src/runtime/DnsAddressParser.cpp
    ../src/runtime/DnsHosts.h
    ../src/runtime/DnsHosts.cpp
    ../src/runtime/LegacyDnsConfigBuilder.h
    ../src/runtime/LegacyDnsConfigBuilder.cpp
    ../src/runtime/SingBoxFakeIpFilter.h
    ../src/runtime/SingBoxFakeIpFilter.cpp
    ../src/runtime/SingBoxDnsConfigSupport.h
    ../src/runtime/SingBoxDnsConfigSupport.cpp
    ../src/runtime/SingBoxDnsRuleConfig.h
    ../src/runtime/SingBoxDnsRuleConfig.cpp
    ../src/runtime/SingBoxDnsConfigBuilder.h
    ../src/runtime/SingBoxDnsConfigBuilder.cpp
    ../src/runtime/ProtocolConfigMapper.h
    ../src/runtime/ProtocolConfigMapper.cpp
    ../src/runtime/RoutingConfigFragments.h
    ../src/runtime/RoutingConfigFragments.cpp
    ../src/runtime/RoutingRuleJsonMapper.h
    ../src/runtime/RoutingRuleJsonMapper.cpp
    ../src/runtime/RoutingRuleJsonSupport.h
    ../src/runtime/RoutingRuleJsonSupport.cpp
    ../src/runtime/SingBoxRoutingRuleMapper.h
    ../src/runtime/SingBoxRoutingRuleMapper.cpp
    ../src/runtime/core/ICoreBackend.h
    ../src/runtime/core/CoreDescriptor.h
    ../src/runtime/core/CoreDescriptorRegistry.h
    ../src/runtime/core/CoreCatalog.h
    ../src/runtime/core/CoreBackendRegistry.h
    ../src/backends/CoreBackendRegistry.cpp
    ../src/backends/singbox/SingBoxCoreDescriptor.h
    ../src/backends/singbox/SingBoxConfigFragments.h
    ../src/backends/singbox/SingBoxConfigFragments.cpp
    ../src/backends/singbox/SingBoxTunCompatConfigFragments.h
    ../src/backends/singbox/SingBoxTunCompatConfigFragments.cpp
    ../src/backends/singbox/SingBoxOutboundConfigFragments.h
    ../src/backends/singbox/SingBoxOutboundConfigFragments.cpp
    ../src/backends/singbox/SingBoxOutboundConfigSupport.h
    ../src/backends/singbox/SingBoxOutboundConfigSupport.cpp
    ../src/backends/singbox/SingBoxTlsConfigFragments.h
    ../src/backends/singbox/SingBoxTlsConfigFragments.cpp
    ../src/backends/singbox/SingBoxTransportConfigFragments.h
    ../src/backends/singbox/SingBoxTransportConfigFragments.cpp
    ../src/backends/singbox/SingBoxRoutingConfigFragments.h
    ../src/backends/singbox/SingBoxRoutingConfigFragments.cpp
    ../src/backends/singbox/SingBoxCoreBackend.h
    ../src/backends/singbox/SingBoxCoreBackend.cpp
    ../src/backends/xray/XrayCoreDescriptor.h
    ../src/backends/xray/XrayConfigFragments.h
    ../src/backends/xray/XrayConfigFragments.cpp
    ../src/backends/xray/XrayOutboundConfigFragments.h
    ../src/backends/xray/XrayOutboundConfigFragments.cpp
    ../src/backends/xray/XrayStreamConfigFragments.h
    ../src/backends/xray/XrayStreamConfigFragments.cpp
    ../src/backends/xray/XrayTransportConfigFragments.h
    ../src/backends/xray/XrayTransportConfigFragments.cpp
    ../src/backends/xray/XraySecurityConfigFragments.h
    ../src/backends/xray/XraySecurityConfigFragments.cpp
    ../src/backends/xray/XrayCoreBackend.h
    ../src/backends/xray/XrayCoreBackend.cpp
)

songbird_add_qt_test(songbird_app_bootstrap_tun_runtime_tests app-bootstrap-tun-runtime
    SOURCES
        ${SONGBIRD_APP_BOOTSTRAP_TUN_RUNTIME_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

set(SONGBIRD_PROXY_SESSION_TEST_SOURCES
    ProxySessionTests.cpp
    ../src/app/BackgroundTaskCoordinator.h
    ../src/app/BackgroundTaskCoordinator.cpp
    ../src/app/BackgroundThreadTracker.h
    ../src/app/BackgroundThreadTracker.cpp
    ../src/app/OutboundLocationProbeService.h
    ../src/app/OutboundLocationProbeService.cpp
    ../src/app/PostStopAction.h
    ../src/app/CoreStartupChecklist.h
    ../src/app/CoreStartupChecklist.cpp
    ../src/app/ProxyCrashRestartPolicy.h
    ../src/app/ProxyCrashRestartPolicy.cpp
    ../src/app/ProxySession.h
    ../src/app/ProxySession.cpp
    ../src/app/ProxyRuntimeInterfaces.h
    ../src/runtime/ClientConfigWriter.h
    ../src/runtime/ClientConfigWriter.cpp
    ../src/runtime/CoreConfigPreflight.h
    ../src/runtime/CoreConfigPreflight.cpp
    ../src/runtime/DnsConfigFragments.h
    ../src/runtime/DnsConfigFragments.cpp
    ../src/runtime/DnsAddressParser.h
    ../src/runtime/DnsAddressParser.cpp
    ../src/runtime/DnsHosts.h
    ../src/runtime/DnsHosts.cpp
    ../src/runtime/LegacyDnsConfigBuilder.h
    ../src/runtime/LegacyDnsConfigBuilder.cpp
    ../src/runtime/SingBoxFakeIpFilter.h
    ../src/runtime/SingBoxFakeIpFilter.cpp
    ../src/runtime/SingBoxDnsConfigSupport.h
    ../src/runtime/SingBoxDnsConfigSupport.cpp
    ../src/runtime/SingBoxDnsRuleConfig.h
    ../src/runtime/SingBoxDnsRuleConfig.cpp
    ../src/runtime/SingBoxDnsConfigBuilder.h
    ../src/runtime/SingBoxDnsConfigBuilder.cpp
    ../src/runtime/ICoreProcessHost.h
    ../src/runtime/ProtocolConfigMapper.h
    ../src/runtime/ProtocolConfigMapper.cpp
    ../src/runtime/RoutingConfigFragments.h
    ../src/runtime/RoutingConfigFragments.cpp
    ../src/runtime/RoutingRuleJsonMapper.h
    ../src/runtime/RoutingRuleJsonMapper.cpp
    ../src/runtime/RoutingRuleJsonSupport.h
    ../src/runtime/RoutingRuleJsonSupport.cpp
    ../src/runtime/SingBoxRoutingRuleMapper.h
    ../src/runtime/SingBoxRoutingRuleMapper.cpp
    ../src/runtime/core/ICoreBackend.h
    ../src/runtime/core/CoreDescriptor.h
    ../src/runtime/core/CoreDescriptorRegistry.h
    ../src/runtime/core/CoreCatalog.h
    ../src/runtime/core/CoreBackendRegistry.h
    ../src/backends/CoreBackendRegistry.cpp
    ../src/backends/singbox/SingBoxCoreDescriptor.h
    ../src/backends/singbox/SingBoxConfigFragments.h
    ../src/backends/singbox/SingBoxConfigFragments.cpp
    ../src/backends/singbox/SingBoxTunCompatConfigFragments.h
    ../src/backends/singbox/SingBoxTunCompatConfigFragments.cpp
    ../src/backends/singbox/SingBoxOutboundConfigFragments.h
    ../src/backends/singbox/SingBoxOutboundConfigFragments.cpp
    ../src/backends/singbox/SingBoxOutboundConfigSupport.h
    ../src/backends/singbox/SingBoxOutboundConfigSupport.cpp
    ../src/backends/singbox/SingBoxTlsConfigFragments.h
    ../src/backends/singbox/SingBoxTlsConfigFragments.cpp
    ../src/backends/singbox/SingBoxTransportConfigFragments.h
    ../src/backends/singbox/SingBoxTransportConfigFragments.cpp
    ../src/backends/singbox/SingBoxRoutingConfigFragments.h
    ../src/backends/singbox/SingBoxRoutingConfigFragments.cpp
    ../src/backends/singbox/SingBoxCoreBackend.h
    ../src/backends/singbox/SingBoxCoreBackend.cpp
    ../src/backends/xray/XrayCoreDescriptor.h
    ../src/backends/xray/XrayConfigFragments.h
    ../src/backends/xray/XrayConfigFragments.cpp
    ../src/backends/xray/XrayOutboundConfigFragments.h
    ../src/backends/xray/XrayOutboundConfigFragments.cpp
    ../src/backends/xray/XrayStreamConfigFragments.h
    ../src/backends/xray/XrayStreamConfigFragments.cpp
    ../src/backends/xray/XrayTransportConfigFragments.h
    ../src/backends/xray/XrayTransportConfigFragments.cpp
    ../src/backends/xray/XraySecurityConfigFragments.h
    ../src/backends/xray/XraySecurityConfigFragments.cpp
    ../src/backends/xray/XrayCoreBackend.h
    ../src/backends/xray/XrayCoreBackend.cpp
    ../src/services/CoreUpdateService.h
    ../src/services/CoreUpdateService.cpp
    ../src/services/CoreUpdateInstallFiles.h
    ../src/services/CoreUpdateInstallFiles.cpp
    ../src/services/CoreUpdatePackageDownload.h
    ../src/services/CoreUpdatePackageDownload.cpp
    ../src/services/CoreUpdatePackageInstallation.h
    ../src/services/CoreUpdatePackageInstallation.cpp
    ../src/services/CoreUpdateReleaseMetadata.h
    ../src/services/CoreUpdateReleaseMetadata.cpp
    ../src/services/CoreUpdateReleaseResolution.h
    ../src/services/CoreUpdateReleaseResolution.cpp
    ../src/services/CoreUpdateOperations.h
    ../src/services/CoreUpdateOperations.cpp
    ../src/services/CoreUpdateVersion.h
    ../src/services/CoreUpdateVersion.cpp
    ../src/services/GeoResourceUpdateService.h
    ../src/services/GeoResourceUpdateService.cpp
    ../src/services/GeoResourceDownload.h
    ../src/services/GeoResourceDownload.cpp
)

songbird_add_qt_test(songbird_proxy_session_tests proxy-session-state
    SOURCES
        ${SONGBIRD_PROXY_SESSION_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

set(SONGBIRD_CORE_UPDATE_COORDINATOR_TEST_SOURCES
    CoreUpdateCoordinatorTests.cpp
    ../src/app/BackgroundTaskCoordinator.h
    ../src/app/BackgroundTaskCoordinator.cpp
    ../src/app/CoreUpdateCoordinator.h
    ../src/app/CoreUpdateCoordinator.cpp
    ../src/app/CoreUpdatePendingState.h
    ../src/app/CoreUpdatePendingState.cpp
    ../src/services/CoreUpdateService.h
    ../src/services/CoreUpdateService.cpp
    ../src/services/CoreUpdateInstallFiles.h
    ../src/services/CoreUpdateInstallFiles.cpp
    ../src/services/CoreUpdatePackageDownload.h
    ../src/services/CoreUpdatePackageDownload.cpp
    ../src/services/CoreUpdatePackageInstallation.h
    ../src/services/CoreUpdatePackageInstallation.cpp
    ../src/services/CoreUpdateReleaseMetadata.h
    ../src/services/CoreUpdateReleaseMetadata.cpp
    ../src/services/CoreUpdateReleaseResolution.h
    ../src/services/CoreUpdateReleaseResolution.cpp
    ../src/services/CoreUpdateOperations.h
    ../src/services/CoreUpdateOperations.cpp
    ../src/services/CoreUpdateVersion.h
    ../src/services/CoreUpdateVersion.cpp
    ../src/runtime/DnsConfigFragments.h
    ../src/runtime/DnsConfigFragments.cpp
    ../src/runtime/DnsAddressParser.h
    ../src/runtime/DnsAddressParser.cpp
    ../src/runtime/DnsHosts.h
    ../src/runtime/DnsHosts.cpp
    ../src/runtime/LegacyDnsConfigBuilder.h
    ../src/runtime/LegacyDnsConfigBuilder.cpp
    ../src/runtime/SingBoxFakeIpFilter.h
    ../src/runtime/SingBoxFakeIpFilter.cpp
    ../src/runtime/SingBoxDnsConfigSupport.h
    ../src/runtime/SingBoxDnsConfigSupport.cpp
    ../src/runtime/SingBoxDnsRuleConfig.h
    ../src/runtime/SingBoxDnsRuleConfig.cpp
    ../src/runtime/SingBoxDnsConfigBuilder.h
    ../src/runtime/SingBoxDnsConfigBuilder.cpp
    ../src/runtime/ProtocolConfigMapper.h
    ../src/runtime/ProtocolConfigMapper.cpp
    ../src/runtime/RoutingConfigFragments.h
    ../src/runtime/RoutingConfigFragments.cpp
    ../src/runtime/RoutingRuleJsonMapper.h
    ../src/runtime/RoutingRuleJsonMapper.cpp
    ../src/runtime/RoutingRuleJsonSupport.h
    ../src/runtime/RoutingRuleJsonSupport.cpp
    ../src/runtime/SingBoxRoutingRuleMapper.h
    ../src/runtime/SingBoxRoutingRuleMapper.cpp
    ../src/runtime/core/ICoreBackend.h
    ../src/runtime/core/CoreDescriptor.h
    ../src/runtime/core/CoreDescriptorRegistry.h
    ../src/runtime/core/CoreCatalog.h
    ../src/runtime/core/CoreBackendRegistry.h
    ../src/backends/CoreBackendRegistry.cpp
    ../src/backends/singbox/SingBoxCoreDescriptor.h
    ../src/backends/singbox/SingBoxConfigFragments.h
    ../src/backends/singbox/SingBoxConfigFragments.cpp
    ../src/backends/singbox/SingBoxTunCompatConfigFragments.h
    ../src/backends/singbox/SingBoxTunCompatConfigFragments.cpp
    ../src/backends/singbox/SingBoxOutboundConfigFragments.h
    ../src/backends/singbox/SingBoxOutboundConfigFragments.cpp
    ../src/backends/singbox/SingBoxOutboundConfigSupport.h
    ../src/backends/singbox/SingBoxOutboundConfigSupport.cpp
    ../src/backends/singbox/SingBoxTlsConfigFragments.h
    ../src/backends/singbox/SingBoxTlsConfigFragments.cpp
    ../src/backends/singbox/SingBoxTransportConfigFragments.h
    ../src/backends/singbox/SingBoxTransportConfigFragments.cpp
    ../src/backends/singbox/SingBoxRoutingConfigFragments.h
    ../src/backends/singbox/SingBoxRoutingConfigFragments.cpp
    ../src/backends/singbox/SingBoxCoreBackend.h
    ../src/backends/singbox/SingBoxCoreBackend.cpp
    ../src/backends/xray/XrayCoreDescriptor.h
    ../src/backends/xray/XrayConfigFragments.h
    ../src/backends/xray/XrayConfigFragments.cpp
    ../src/backends/xray/XrayOutboundConfigFragments.h
    ../src/backends/xray/XrayOutboundConfigFragments.cpp
    ../src/backends/xray/XrayStreamConfigFragments.h
    ../src/backends/xray/XrayStreamConfigFragments.cpp
    ../src/backends/xray/XrayTransportConfigFragments.h
    ../src/backends/xray/XrayTransportConfigFragments.cpp
    ../src/backends/xray/XraySecurityConfigFragments.h
    ../src/backends/xray/XraySecurityConfigFragments.cpp
    ../src/backends/xray/XrayCoreBackend.h
    ../src/backends/xray/XrayCoreBackend.cpp
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
    ../src/app/RuntimeState.h
    ../src/app/RuntimeState.cpp
)

songbird_add_qt_test(songbird_runtime_state_tests runtime-state
    SOURCES
        ${SONGBIRD_RUNTIME_STATE_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
)

set(SONGBIRD_MAIN_WINDOW_TEST_SOURCES
    MainWindowTests.cpp
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
    ../src/ui/theme/AppTheme.h
    ../src/ui/theme/AppTheme.cpp
    ../src/ui/theme/AppThemePalette.h
    ../src/ui/theme/AppThemePalette.cpp
    ../src/ui/theme/AppThemeStyle.h
    ../src/ui/theme/AppThemeStyle.cpp
    ../src/ui/theme/ServerTableTheme.h
    ../src/ui/theme/ServerTableTheme.cpp
    ../src/resources/app_icons.qrc
    ../src/resources/toolbar_icons.qrc
    ../src/resources/themes.qrc
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

set(SONGBIRD_CLIENT_CONFIG_WRITER_TEST_SOURCES
    ClientConfigWriterTests.cpp
    ../src/runtime/ClientConfigWriter.h
    ../src/runtime/ClientConfigWriter.cpp
    ../src/runtime/DnsConfigFragments.h
    ../src/runtime/DnsConfigFragments.cpp
    ../src/runtime/DnsAddressParser.h
    ../src/runtime/DnsAddressParser.cpp
    ../src/runtime/DnsHosts.h
    ../src/runtime/DnsHosts.cpp
    ../src/runtime/LegacyDnsConfigBuilder.h
    ../src/runtime/LegacyDnsConfigBuilder.cpp
    ../src/runtime/SingBoxFakeIpFilter.h
    ../src/runtime/SingBoxFakeIpFilter.cpp
    ../src/runtime/SingBoxDnsConfigSupport.h
    ../src/runtime/SingBoxDnsConfigSupport.cpp
    ../src/runtime/SingBoxDnsRuleConfig.h
    ../src/runtime/SingBoxDnsRuleConfig.cpp
    ../src/runtime/SingBoxDnsConfigBuilder.h
    ../src/runtime/SingBoxDnsConfigBuilder.cpp
    ../src/runtime/ProtocolConfigMapper.h
    ../src/runtime/ProtocolConfigMapper.cpp
    ../src/runtime/RoutingConfigFragments.h
    ../src/runtime/RoutingConfigFragments.cpp
    ../src/runtime/RoutingRuleJsonMapper.h
    ../src/runtime/RoutingRuleJsonMapper.cpp
    ../src/runtime/RoutingRuleJsonSupport.h
    ../src/runtime/RoutingRuleJsonSupport.cpp
    ../src/runtime/SingBoxRoutingRuleMapper.h
    ../src/runtime/SingBoxRoutingRuleMapper.cpp
    ../src/runtime/core/ICoreBackend.h
    ../src/runtime/core/CoreCatalog.h
    ../src/runtime/core/CoreBackendRegistry.h
    ../src/backends/CoreBackendRegistry.cpp
    ../src/backends/singbox/SingBoxConfigFragments.h
    ../src/backends/singbox/SingBoxConfigFragments.cpp
    ../src/backends/singbox/SingBoxTunCompatConfigFragments.h
    ../src/backends/singbox/SingBoxTunCompatConfigFragments.cpp
    ../src/backends/singbox/SingBoxOutboundConfigFragments.h
    ../src/backends/singbox/SingBoxOutboundConfigFragments.cpp
    ../src/backends/singbox/SingBoxOutboundConfigSupport.h
    ../src/backends/singbox/SingBoxOutboundConfigSupport.cpp
    ../src/backends/singbox/SingBoxTlsConfigFragments.h
    ../src/backends/singbox/SingBoxTlsConfigFragments.cpp
    ../src/backends/singbox/SingBoxTransportConfigFragments.h
    ../src/backends/singbox/SingBoxTransportConfigFragments.cpp
    ../src/backends/singbox/SingBoxRoutingConfigFragments.h
    ../src/backends/singbox/SingBoxRoutingConfigFragments.cpp
    ../src/backends/singbox/SingBoxCoreBackend.h
    ../src/backends/singbox/SingBoxCoreBackend.cpp
    ../src/backends/xray/XrayConfigFragments.h
    ../src/backends/xray/XrayConfigFragments.cpp
    ../src/backends/xray/XrayOutboundConfigFragments.h
    ../src/backends/xray/XrayOutboundConfigFragments.cpp
    ../src/backends/xray/XrayStreamConfigFragments.h
    ../src/backends/xray/XrayStreamConfigFragments.cpp
    ../src/backends/xray/XrayTransportConfigFragments.h
    ../src/backends/xray/XrayTransportConfigFragments.cpp
    ../src/backends/xray/XraySecurityConfigFragments.h
    ../src/backends/xray/XraySecurityConfigFragments.cpp
    ../src/backends/xray/XrayCoreBackend.h
    ../src/backends/xray/XrayCoreBackend.cpp
)

songbird_add_qt_test(songbird_client_config_writer_tests client-config-writer-tun-compat
    SOURCES
        ${SONGBIRD_CLIENT_CONFIG_WRITER_TEST_SOURCES}
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
    ../src/persistence/JsonConfigRepository.h
    ../src/persistence/JsonConfigRepository.cpp
    ../src/persistence/JsonConfigCollectionSerialization.h
    ../src/persistence/JsonConfigCollectionSerialization.cpp
    ../src/persistence/JsonConfigServerSerialization.h
    ../src/persistence/JsonConfigServerSerialization.cpp
    ../src/persistence/JsonConfigRoutingSerialization.h
    ../src/persistence/JsonConfigRoutingSerialization.cpp
    ../src/persistence/JsonConfigSubscriptionSerialization.h
    ../src/persistence/JsonConfigSubscriptionSerialization.cpp
    ../src/persistence/JsonConfigPolicySerialization.h
    ../src/persistence/JsonConfigPolicySerialization.cpp
    ../src/persistence/JsonConfigRootSerialization.h
    ../src/persistence/JsonConfigRootSerialization.cpp
    ../src/persistence/JsonConfigSerialization.h
    ../src/persistence/JsonConfigSerialization.cpp
    ../src/persistence/JsonConfigStateSerialization.h
    ../src/persistence/JsonConfigStateSerialization.cpp
    ../src/persistence/JsonConfigTunSerialization.h
    ../src/persistence/JsonConfigTunSerialization.cpp
    ../src/persistence/JsonConfigUiSerialization.h
    ../src/persistence/JsonConfigUiSerialization.cpp
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
    ../src/services/ProxyAvailabilityCheckService.h
    ../src/services/ProxyAvailabilityCheckService.cpp
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
    ../src/services/SpeedTestBatchConfig.h
    ../src/services/SpeedTestBatchConfig.cpp
    ../src/services/SpeedTestPortReservation.h
    ../src/services/SpeedTestPortReservation.cpp
    ../src/services/SpeedTestRuntimeProcess.h
    ../src/services/SpeedTestRuntimeProcess.cpp
    ../src/services/SpeedTestUrlProbe.h
    ../src/services/SpeedTestUrlProbe.cpp
)

songbird_add_qt_test(songbird_speed_test_service_internal_tests speed-test-service-internal
    SOURCES
        ${SONGBIRD_SPEED_TEST_SERVICE_INTERNAL_TEST_SOURCES}
    LIBRARIES
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

set(SONGBIRD_SUBSCRIPTION_SERVICE_TEST_SOURCES
    SubscriptionServiceTests.cpp
    ../src/services/SubscriptionService.h
    ../src/services/SubscriptionService.cpp
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
    ../src/services/SubscriptionService.h
    ../src/services/SubscriptionService.cpp
    ../src/subscription/SubscriptionImportTextParser.h
    ../src/subscription/SubscriptionImportTextParser.cpp
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
    ../src/common/SystemProxyMode.h
)

songbird_add_qt_test(songbird_system_proxy_mode_tests system-proxy-mode
    SOURCES
        ${SONGBIRD_SYSTEM_PROXY_MODE_TEST_SOURCES}
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
    ../src/services/CoreUpdateVersion.h
    ../src/services/CoreUpdateVersion.cpp
    ../src/common/GitHubMirrorHelper.h
    ../src/common/GitHubUrls.h
    ../src/common/OperationResult.h
    ../src/common/UserAgent.h
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
    ../src/app/BackgroundTaskCoordinator.h
    ../src/app/BackgroundTaskCoordinator.cpp
    ../src/app/IUserFeedback.h
    ../src/services/AppUpdateService.h
    ../src/services/AppUpdateService.cpp
    ../src/services/AppUpdateReleaseMetadata.h
    ../src/services/AppUpdateReleaseMetadata.cpp
    ../src/services/CoreUpdateVersion.h
    ../src/services/CoreUpdateVersion.cpp
    ../src/common/GitHubMirrorHelper.h
    ../src/common/GitHubUrls.h
    ../src/common/OperationResult.h
    ../src/common/UserAgent.h
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
    ../src/services/CoreUpdateService.h
    ../src/services/CoreUpdateService.cpp
    ../src/services/CoreUpdateInstallFiles.h
    ../src/services/CoreUpdateInstallFiles.cpp
    ../src/services/CoreUpdatePackageDownload.h
    ../src/services/CoreUpdatePackageDownload.cpp
    ../src/services/CoreUpdatePackageInstallation.h
    ../src/services/CoreUpdatePackageInstallation.cpp
    ../src/services/CoreUpdateReleaseMetadata.h
    ../src/services/CoreUpdateReleaseMetadata.cpp
    ../src/services/CoreUpdateReleaseResolution.h
    ../src/services/CoreUpdateReleaseResolution.cpp
    ../src/services/CoreUpdateOperations.h
    ../src/services/CoreUpdateOperations.cpp
    ../src/services/CoreUpdateVersion.h
    ../src/services/CoreUpdateVersion.cpp
    ../src/runtime/DnsConfigFragments.h
    ../src/runtime/DnsConfigFragments.cpp
    ../src/runtime/DnsAddressParser.h
    ../src/runtime/DnsAddressParser.cpp
    ../src/runtime/DnsHosts.h
    ../src/runtime/DnsHosts.cpp
    ../src/runtime/LegacyDnsConfigBuilder.h
    ../src/runtime/LegacyDnsConfigBuilder.cpp
    ../src/runtime/SingBoxFakeIpFilter.h
    ../src/runtime/SingBoxFakeIpFilter.cpp
    ../src/runtime/SingBoxDnsConfigSupport.h
    ../src/runtime/SingBoxDnsConfigSupport.cpp
    ../src/runtime/SingBoxDnsRuleConfig.h
    ../src/runtime/SingBoxDnsRuleConfig.cpp
    ../src/runtime/SingBoxDnsConfigBuilder.h
    ../src/runtime/SingBoxDnsConfigBuilder.cpp
    ../src/runtime/ProtocolConfigMapper.h
    ../src/runtime/ProtocolConfigMapper.cpp
    ../src/runtime/RoutingConfigFragments.h
    ../src/runtime/RoutingConfigFragments.cpp
    ../src/runtime/RoutingRuleJsonMapper.h
    ../src/runtime/RoutingRuleJsonMapper.cpp
    ../src/runtime/RoutingRuleJsonSupport.h
    ../src/runtime/RoutingRuleJsonSupport.cpp
    ../src/runtime/SingBoxRoutingRuleMapper.h
    ../src/runtime/SingBoxRoutingRuleMapper.cpp
    ../src/runtime/core/ICoreBackend.h
    ../src/runtime/core/CoreCatalog.h
    ../src/runtime/core/CoreBackendRegistry.h
    ../src/backends/CoreBackendRegistry.cpp
    ../src/backends/singbox/SingBoxConfigFragments.h
    ../src/backends/singbox/SingBoxConfigFragments.cpp
    ../src/backends/singbox/SingBoxTunCompatConfigFragments.h
    ../src/backends/singbox/SingBoxTunCompatConfigFragments.cpp
    ../src/backends/singbox/SingBoxOutboundConfigFragments.h
    ../src/backends/singbox/SingBoxOutboundConfigFragments.cpp
    ../src/backends/singbox/SingBoxOutboundConfigSupport.h
    ../src/backends/singbox/SingBoxOutboundConfigSupport.cpp
    ../src/backends/singbox/SingBoxTlsConfigFragments.h
    ../src/backends/singbox/SingBoxTlsConfigFragments.cpp
    ../src/backends/singbox/SingBoxTransportConfigFragments.h
    ../src/backends/singbox/SingBoxTransportConfigFragments.cpp
    ../src/backends/singbox/SingBoxRoutingConfigFragments.h
    ../src/backends/singbox/SingBoxRoutingConfigFragments.cpp
    ../src/backends/singbox/SingBoxCoreBackend.h
    ../src/backends/singbox/SingBoxCoreBackend.cpp
    ../src/backends/xray/XrayConfigFragments.h
    ../src/backends/xray/XrayConfigFragments.cpp
    ../src/backends/xray/XrayOutboundConfigFragments.h
    ../src/backends/xray/XrayOutboundConfigFragments.cpp
    ../src/backends/xray/XrayStreamConfigFragments.h
    ../src/backends/xray/XrayStreamConfigFragments.cpp
    ../src/backends/xray/XrayTransportConfigFragments.h
    ../src/backends/xray/XrayTransportConfigFragments.cpp
    ../src/backends/xray/XraySecurityConfigFragments.h
    ../src/backends/xray/XraySecurityConfigFragments.cpp
    ../src/backends/xray/XrayCoreBackend.h
    ../src/backends/xray/XrayCoreBackend.cpp
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
    ../src/services/GeoResourceUpdateService.h
    ../src/services/GeoResourceUpdateService.cpp
    ../src/services/GeoResourceDownload.h
    ../src/services/GeoResourceDownload.cpp
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
    ../src/subscription/SubscriptionContentParser.h
    ../src/subscription/SubscriptionContentParser.cpp
    ../src/subscription/SubscriptionSingBoxParser.h
    ../src/subscription/SubscriptionSingBoxParser.cpp
    ../src/subscription/ClashSubscriptionParser.h
    ../src/subscription/ClashSubscriptionParser.cpp
    ../src/subscription/ClashProxyItemParser.h
    ../src/subscription/ClashProxyItemParser.cpp
    ../src/subscription/ClashYamlProxyParser.h
    ../src/subscription/ClashYamlProxyParser.cpp
    ../src/subscription/SubscriptionJsonSupport.h
    ../src/subscription/SubscriptionJsonSupport.cpp
    ../src/subscription/SubscriptionYamlParser.h
    ../src/subscription/SubscriptionYamlParser.cpp
    ../src/subscription/SubscriptionYamlScalarValueParser.h
    ../src/subscription/SubscriptionYamlScalarValueParser.cpp
    ../src/subscription/SubscriptionYamlFlowValueParser.h
    ../src/subscription/SubscriptionYamlFlowValueParser.cpp
    ../src/subscription/CustomConfigTextParser.h
    ../src/subscription/CustomConfigTextParser.cpp
    ../src/subscription/ShareUrlParser.h
    ../src/subscription/ShareUrlParser.cpp
    ../src/subscription/ShareUrlProtocolParsers.h
    ../src/subscription/ShareUrlProtocolParsers.cpp
    ../src/subscription/ShareUrlSingBoxProtocolParsers.cpp
    ../src/subscription/ShareUrlVmessParser.h
    ../src/subscription/ShareUrlVmessParser.cpp
    ../src/subscription/ShareUrlParserSupport.h
    ../src/subscription/ShareUrlParserSupport.cpp
    ../src/subscription/SubscriptionImportTextParser.h
    ../src/subscription/SubscriptionImportTextParser.cpp
    ../src/common/PortValidator.h
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
    ../src/services/ServerService.h
    ../src/services/ServerService.cpp
    ../src/services/ServerCustomConfigStore.h
    ../src/services/ServerCustomConfigStore.cpp
    ../src/services/ServerListOperations.h
    ../src/services/ServerListOperations.cpp
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
    ../src/common/SystemProxyMode.h
    ../src/persistence/IConfigRepository.h
    ../src/persistence/JsonConfigRepository.h
    ../src/persistence/JsonConfigRepository.cpp
    ../src/persistence/JsonConfigSerialization.h
    ../src/persistence/JsonConfigSerialization.cpp
    ../src/persistence/JsonConfigRootSerialization.h
    ../src/persistence/JsonConfigRootSerialization.cpp
    ../src/persistence/JsonConfigCollectionSerialization.h
    ../src/persistence/JsonConfigCollectionSerialization.cpp
    ../src/persistence/JsonConfigServerSerialization.h
    ../src/persistence/JsonConfigServerSerialization.cpp
    ../src/persistence/JsonConfigRoutingSerialization.h
    ../src/persistence/JsonConfigRoutingSerialization.cpp
    ../src/persistence/JsonConfigSubscriptionSerialization.h
    ../src/persistence/JsonConfigSubscriptionSerialization.cpp
    ../src/persistence/JsonConfigPolicySerialization.h
    ../src/persistence/JsonConfigPolicySerialization.cpp
    ../src/persistence/JsonConfigStateSerialization.h
    ../src/persistence/JsonConfigStateSerialization.cpp
    ../src/persistence/JsonConfigTunSerialization.h
    ../src/persistence/JsonConfigTunSerialization.cpp
    ../src/persistence/JsonConfigUiSerialization.h
    ../src/persistence/JsonConfigUiSerialization.cpp
    ../src/runtime/ClientConfigWriter.h
    ../src/runtime/ClientConfigWriter.cpp
    ../src/runtime/DnsConfigFragments.h
    ../src/runtime/DnsConfigFragments.cpp
    ../src/runtime/DnsAddressParser.h
    ../src/runtime/DnsAddressParser.cpp
    ../src/runtime/DnsHosts.h
    ../src/runtime/DnsHosts.cpp
    ../src/runtime/LegacyDnsConfigBuilder.h
    ../src/runtime/LegacyDnsConfigBuilder.cpp
    ../src/runtime/SingBoxFakeIpFilter.h
    ../src/runtime/SingBoxFakeIpFilter.cpp
    ../src/runtime/SingBoxDnsConfigSupport.h
    ../src/runtime/SingBoxDnsConfigSupport.cpp
    ../src/runtime/SingBoxDnsRuleConfig.h
    ../src/runtime/SingBoxDnsRuleConfig.cpp
    ../src/runtime/SingBoxDnsConfigBuilder.h
    ../src/runtime/SingBoxDnsConfigBuilder.cpp
    ../src/runtime/ProtocolConfigMapper.h
    ../src/runtime/ProtocolConfigMapper.cpp
    ../src/runtime/RoutingConfigFragments.h
    ../src/runtime/RoutingConfigFragments.cpp
    ../src/runtime/RoutingRuleJsonMapper.h
    ../src/runtime/RoutingRuleJsonMapper.cpp
    ../src/runtime/RoutingRuleJsonSupport.h
    ../src/runtime/RoutingRuleJsonSupport.cpp
    ../src/runtime/SingBoxRoutingRuleMapper.h
    ../src/runtime/SingBoxRoutingRuleMapper.cpp
    ../src/runtime/core/ICoreBackend.h
    ../src/runtime/core/CoreCatalog.h
    ../src/runtime/core/CoreBackendRegistry.h
    ../src/backends/CoreBackendRegistry.cpp
    ../src/backends/singbox/SingBoxConfigFragments.h
    ../src/backends/singbox/SingBoxConfigFragments.cpp
    ../src/backends/singbox/SingBoxTunCompatConfigFragments.h
    ../src/backends/singbox/SingBoxTunCompatConfigFragments.cpp
    ../src/backends/singbox/SingBoxOutboundConfigFragments.h
    ../src/backends/singbox/SingBoxOutboundConfigFragments.cpp
    ../src/backends/singbox/SingBoxOutboundConfigSupport.h
    ../src/backends/singbox/SingBoxOutboundConfigSupport.cpp
    ../src/backends/singbox/SingBoxTlsConfigFragments.h
    ../src/backends/singbox/SingBoxTlsConfigFragments.cpp
    ../src/backends/singbox/SingBoxTransportConfigFragments.h
    ../src/backends/singbox/SingBoxTransportConfigFragments.cpp
    ../src/backends/singbox/SingBoxRoutingConfigFragments.h
    ../src/backends/singbox/SingBoxRoutingConfigFragments.cpp
    ../src/backends/singbox/SingBoxCoreBackend.h
    ../src/backends/singbox/SingBoxCoreBackend.cpp
    ../src/backends/xray/XrayConfigFragments.h
    ../src/backends/xray/XrayConfigFragments.cpp
    ../src/backends/xray/XrayOutboundConfigFragments.h
    ../src/backends/xray/XrayOutboundConfigFragments.cpp
    ../src/backends/xray/XrayStreamConfigFragments.h
    ../src/backends/xray/XrayStreamConfigFragments.cpp
    ../src/backends/xray/XrayTransportConfigFragments.h
    ../src/backends/xray/XrayTransportConfigFragments.cpp
    ../src/backends/xray/XraySecurityConfigFragments.h
    ../src/backends/xray/XraySecurityConfigFragments.cpp
    ../src/backends/xray/XrayCoreBackend.h
    ../src/backends/xray/XrayCoreBackend.cpp
    ../src/runtime/CoreLifecycleService.h
    ../src/runtime/CoreLifecycleService.cpp
    ../src/runtime/ICoreProcessHost.h
    ../src/runtime/CoreProcessOutputBuffer.h
    ../src/runtime/CoreProcessOutputBuffer.cpp
    ../src/runtime/ProtocolCoreCompat.h
    ../src/runtime/QtCoreProcessHost.h
    ../src/runtime/QtCoreProcessHost.cpp
    ../src/runtime/TunCompatCoreRequirement.h
    ../src/services/CoreUpdateService.h
    ../src/services/CoreUpdateService.cpp
    ../src/services/CoreUpdateInstallFiles.h
    ../src/services/CoreUpdateInstallFiles.cpp
    ../src/services/CoreUpdatePackageDownload.h
    ../src/services/CoreUpdatePackageDownload.cpp
    ../src/services/CoreUpdatePackageInstallation.h
    ../src/services/CoreUpdatePackageInstallation.cpp
    ../src/services/CoreUpdateReleaseMetadata.h
    ../src/services/CoreUpdateReleaseMetadata.cpp
    ../src/services/CoreUpdateReleaseResolution.h
    ../src/services/CoreUpdateReleaseResolution.cpp
    ../src/services/CoreUpdateOperations.h
    ../src/services/CoreUpdateOperations.cpp
    ../src/services/CoreUpdateVersion.h
    ../src/services/CoreUpdateVersion.cpp
    ../src/services/GeoResourceUpdateService.h
    ../src/services/GeoResourceUpdateService.cpp
    ../src/services/GeoResourceDownload.h
    ../src/services/GeoResourceDownload.cpp
    ../src/services/ProxyAvailabilityCheckService.h
    ../src/services/ProxyAvailabilityCheckService.cpp
    ../src/services/SpeedTestServiceInternal.h
    ../src/services/SpeedTestPortReservation.h
    ../src/services/SpeedTestPortReservation.cpp
    ../src/services/SpeedTestUrlProbe.h
    ../src/services/SpeedTestUrlProbe.cpp
    ../src/services/SubscriptionService.h
    ../src/services/SubscriptionService.cpp
    ../src/services/SubscriptionUpdateService.h
    ../src/services/SubscriptionUpdateService.cpp
    ../src/subscription/CustomConfigTextParser.h
    ../src/subscription/CustomConfigTextParser.cpp
    ../src/subscription/ShareUrlParser.h
    ../src/subscription/ShareUrlParser.cpp
    ../src/subscription/ShareUrlProtocolParsers.h
    ../src/subscription/ShareUrlProtocolParsers.cpp
    ../src/subscription/ShareUrlSingBoxProtocolParsers.cpp
    ../src/subscription/ShareUrlVmessParser.h
    ../src/subscription/ShareUrlVmessParser.cpp
    ../src/subscription/ShareUrlParserSupport.h
    ../src/subscription/ShareUrlParserSupport.cpp
    ../src/subscription/SubscriptionContentParser.h
    ../src/subscription/SubscriptionContentParser.cpp
    ../src/subscription/SubscriptionSingBoxParser.h
    ../src/subscription/SubscriptionSingBoxParser.cpp
    ../src/subscription/ClashSubscriptionParser.h
    ../src/subscription/ClashSubscriptionParser.cpp
    ../src/subscription/ClashProxyItemParser.h
    ../src/subscription/ClashProxyItemParser.cpp
    ../src/subscription/ClashYamlProxyParser.h
    ../src/subscription/ClashYamlProxyParser.cpp
    ../src/subscription/SubscriptionJsonSupport.h
    ../src/subscription/SubscriptionJsonSupport.cpp
    ../src/subscription/SubscriptionYamlParser.h
    ../src/subscription/SubscriptionYamlParser.cpp
    ../src/subscription/SubscriptionYamlScalarValueParser.h
    ../src/subscription/SubscriptionYamlScalarValueParser.cpp
    ../src/subscription/SubscriptionYamlFlowValueParser.h
    ../src/subscription/SubscriptionYamlFlowValueParser.cpp
    ../src/subscription/SubscriptionImportTextParser.h
    ../src/subscription/SubscriptionImportTextParser.cpp
    ../src/ui/dialogs/SubscriptionSettingsPageWidget.h
    ../src/ui/dialogs/SubscriptionSettingsPageWidget.cpp
    ../src/ui/theme/AppTheme.h
    ../src/ui/theme/AppTheme.cpp
    ../src/ui/theme/AppThemePalette.h
    ../src/ui/theme/AppThemePalette.cpp
    ../src/ui/theme/AppThemeStyle.h
    ../src/ui/theme/AppThemeStyle.cpp
    ../src/ui/theme/ServerTableTheme.h
    ../src/ui/theme/ServerTableTheme.cpp
    ../src/resources/themes.qrc
)

if(WIN32)
    list(APPEND SONGBIRD_END_TO_END_SMOKE_TEST_SOURCES
        ../src/platform/windows/WindowsSystemProxyService.h
        ../src/platform/windows/WindowsSystemProxyService.cpp
    )
endif()

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
