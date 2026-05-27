set(SONGBIRD_SOURCES
    app/main.cpp
    app/AppBootstrap.h
    app/AppBootstrap.cpp
    app/AppBootstrapObjects.h
    app/AppBootstrapObjects.cpp
    app/AppBootstrapWorkflowWiring.cpp
    app/FunctionRuntimeAdapters.h
    app/FunctionRuntimeAdapters.cpp
    app/AppUpdateCheckCoordinator.h
    app/AppUpdateCheckCoordinator.cpp
    app/AppUpdateCheckPresentation.h
    app/AppUpdateCheckPresentation.cpp
    app/AppUpdateInstallService.h
    app/AppUpdateInstallService.cpp
    app/ApplicationRestartCoordinator.h
    app/ApplicationRestartCoordinator.cpp
    app/IUserFeedback.h
    app/ConfigPathResolver.h
    app/ConfigBackupCoordinator.h
    app/ConfigBackupCoordinator.cpp
    app/DefaultServerSwitchCoordinator.h
    app/DefaultServerSwitchCoordinator.cpp
    app/CoreProcessCleanupService.h
    app/CoreProcessCleanupService.cpp
    app/CoreDiscoveryService.h
    app/CoreDiscoveryService.cpp
    app/CoreStartupCheckpoint.h
    app/CoreStartupChecklist.h
    app/CoreStartupChecklist.cpp
    app/OutboundLocationProbeService.h
    app/OutboundLocationProbeService.cpp
    app/BackgroundTaskCoordinator.h
    app/BackgroundTaskCoordinator.cpp
    app/BackgroundThreadTracker.h
    app/BackgroundThreadTracker.cpp
    app/ProxyRuntimeInterfaces.h
    app/CoreUpdateCoordinator.h
    app/CoreUpdateCoordinator.cpp
    app/CoreUpdatePendingState.h
    app/CoreUpdatePendingState.cpp
    app/GeoResourceUpdateCoordinator.h
    app/GeoResourceUpdateCoordinator.cpp
    app/ServerCollectionCoordinator.h
    app/ServerCollectionCoordinator.cpp
    app/ServerEditorCoordinator.h
    app/ServerEditorCoordinator.cpp
    app/SystemProxyCoordinator.h
    app/SystemProxyCoordinator.cpp
    app/SubscriptionWorkflowCoordinator.h
    app/SubscriptionWorkflowCoordinator.cpp
    app/PostStopAction.h
    app/ProxyCrashRestartPolicy.h
    app/ProxyCrashRestartPolicy.cpp
    app/RuntimeState.h
    app/RuntimeState.cpp
    app/AppRuntimeResolver.h
    app/AppRuntimeResolver.cpp
    app/RuntimeStateSnapshotBuilder.h
    app/RuntimeStateSnapshotBuilder.cpp
    app/ProxySession.h
    app/ProxySession.cpp
    app/SettingsDialogRunner.h
    app/SettingsDialogRunner.cpp
    app/SettingsApplyCoordinator.h
    app/SettingsApplyCoordinator.cpp
    app/SettingsWorkflowCoordinator.h
    app/SettingsWorkflowCoordinator.cpp
    app/SpeedTestCoordinator.h
    app/SpeedTestCoordinator.cpp
    app/SingleInstanceBootstrap.h
    app/SingleInstanceBootstrap.cpp
    app/StartupAdminElevation.h
    app/TunRuntimeService.h
    app/TunRuntimeService.cpp
    app/TunAdminMessages.h
    app/TunModeCoordinator.h
    app/TunModeCoordinator.cpp
    common/OperationResult.h
    common/DataSizeFormatter.h
    common/GitHubMirrorHelper.h
    common/GitHubUrls.h
    common/PortValidator.h
    common/ServerDisplayName.h
    common/SystemProxyMode.h
    common/UserAgent.h
    domain/models/Config.h
    domain/models/PolicyGroupItem.h
    domain/models/RoutingProfiles.h
    domain/models/RoutingProfiles.cpp
    domain/models/RoutingItem.h
    domain/models/RoutingRule.h
    domain/models/SubItem.h
    domain/models/VmessItem.h
    platform/windows/SingleInstanceGuard.h
    platform/windows/SingleInstanceGuard.cpp
    platform/windows/WindowsAutoRunService.h
    platform/windows/WindowsAutoRunService.cpp
    platform/windows/WindowsSystemProxyService.h
    platform/windows/WindowsSystemProxyService.cpp
    platform/windows/WindowsUwpLoopbackService.h
    platform/windows/WindowsUwpLoopbackService.cpp
    platform/windows/WindowsUwpLoopbackSupport.h
    platform/windows/WindowsUwpLoopbackSupport.cpp
    runtime/CoreInfo.h
    runtime/ProtocolCoreCompat.h
    runtime/CoreConfigPreflight.h
    runtime/CoreConfigPreflight.cpp
    runtime/ClientConfigWriter.h
    runtime/ClientConfigWriter.cpp
    runtime/DnsAddressParser.h
    runtime/DnsAddressParser.cpp
    runtime/DnsConfigFragments.h
    runtime/DnsConfigFragments.cpp
    runtime/DnsHosts.h
    runtime/DnsHosts.cpp
    runtime/LegacyDnsConfigBuilder.h
    runtime/LegacyDnsConfigBuilder.cpp
    runtime/SingBoxFakeIpFilter.h
    runtime/SingBoxFakeIpFilter.cpp
    runtime/SingBoxDnsConfigSupport.h
    runtime/SingBoxDnsConfigSupport.cpp
    runtime/SingBoxDnsRuleConfig.h
    runtime/SingBoxDnsRuleConfig.cpp
    runtime/SingBoxDnsConfigBuilder.h
    runtime/SingBoxDnsConfigBuilder.cpp
    runtime/ProtocolConfigMapper.h
    runtime/ProtocolConfigMapper.cpp
    runtime/RoutingConfigFragments.h
    runtime/RoutingConfigFragments.cpp
    runtime/RoutingRuleJsonMapper.h
    runtime/RoutingRuleJsonMapper.cpp
    runtime/RoutingRuleJsonSupport.h
    runtime/RoutingRuleJsonSupport.cpp
    runtime/SingBoxRoutingRuleMapper.h
    runtime/SingBoxRoutingRuleMapper.cpp
    runtime/core/ICoreBackend.h
    runtime/core/CoreDescriptor.h
    runtime/core/CoreDescriptorRegistry.h
    runtime/core/CoreCatalog.h
    runtime/core/CoreBackendRegistry.h
    backends/CoreBackendRegistry.cpp
    backends/singbox/SingBoxCoreDescriptor.h
    backends/singbox/SingBoxConfigFragments.h
    backends/singbox/SingBoxConfigFragments.cpp
    backends/singbox/SingBoxTunCompatConfigFragments.h
    backends/singbox/SingBoxTunCompatConfigFragments.cpp
    backends/singbox/SingBoxOutboundConfigFragments.h
    backends/singbox/SingBoxOutboundConfigFragments.cpp
    backends/singbox/SingBoxOutboundConfigSupport.h
    backends/singbox/SingBoxOutboundConfigSupport.cpp
    backends/singbox/SingBoxTlsConfigFragments.h
    backends/singbox/SingBoxTlsConfigFragments.cpp
    backends/singbox/SingBoxTransportConfigFragments.h
    backends/singbox/SingBoxTransportConfigFragments.cpp
    backends/singbox/SingBoxRoutingConfigFragments.h
    backends/singbox/SingBoxRoutingConfigFragments.cpp
    backends/singbox/SingBoxCoreBackend.h
    backends/singbox/SingBoxCoreBackend.cpp
    backends/xray/XrayCoreDescriptor.h
    backends/xray/XrayConfigFragments.h
    backends/xray/XrayConfigFragments.cpp
    backends/xray/XrayOutboundConfigFragments.h
    backends/xray/XrayOutboundConfigFragments.cpp
    backends/xray/XrayStreamConfigFragments.h
    backends/xray/XrayStreamConfigFragments.cpp
    backends/xray/XrayTransportConfigFragments.h
    backends/xray/XrayTransportConfigFragments.cpp
    backends/xray/XraySecurityConfigFragments.h
    backends/xray/XraySecurityConfigFragments.cpp
    backends/xray/XrayCoreBackend.h
    backends/xray/XrayCoreBackend.cpp
    runtime/ICoreProcessHost.h
    runtime/CoreProcessOutputBuffer.h
    runtime/CoreProcessOutputBuffer.cpp
    runtime/QtCoreProcessHost.h
    runtime/QtCoreProcessHost.cpp
    persistence/IConfigRepository.h
    persistence/JsonConfigRepository.h
    persistence/JsonConfigRepository.cpp
    persistence/JsonConfigCollectionSerialization.h
    persistence/JsonConfigCollectionSerialization.cpp
    persistence/JsonConfigServerSerialization.h
    persistence/JsonConfigServerSerialization.cpp
    persistence/JsonConfigRoutingSerialization.h
    persistence/JsonConfigRoutingSerialization.cpp
    persistence/JsonConfigSubscriptionSerialization.h
    persistence/JsonConfigSubscriptionSerialization.cpp
    persistence/JsonConfigPolicySerialization.h
    persistence/JsonConfigPolicySerialization.cpp
    persistence/JsonConfigRootSerialization.h
    persistence/JsonConfigRootSerialization.cpp
    persistence/JsonConfigSerialization.h
    persistence/JsonConfigSerialization.cpp
    persistence/JsonConfigStateSerialization.h
    persistence/JsonConfigStateSerialization.cpp
    persistence/JsonConfigTunSerialization.h
    persistence/JsonConfigTunSerialization.cpp
    persistence/JsonConfigUiSerialization.h
    persistence/JsonConfigUiSerialization.cpp
    services/ServerService.h
    services/ServerService.cpp
    services/ServerCustomConfigStore.h
    services/ServerCustomConfigStore.cpp
    services/ServerListOperations.h
    services/ServerListOperations.cpp
    services/ConfigBackupService.h
    services/ConfigBackupService.cpp
    services/ConfigBackupStateDocument.h
    services/ConfigBackupStateDocument.cpp
    services/AppUpdateService.h
    services/AppUpdateService.cpp
    services/AppUpdateReleaseMetadata.h
    services/AppUpdateReleaseMetadata.cpp
    services/CoreUpdateOperations.h
    services/CoreUpdateOperations.cpp
    services/CoreUpdateService.h
    services/CoreUpdateService.cpp
    services/CoreUpdateInstallFiles.h
    services/CoreUpdateInstallFiles.cpp
    services/CoreUpdatePackageDownload.h
    services/CoreUpdatePackageDownload.cpp
    services/CoreUpdatePackageInstallation.h
    services/CoreUpdatePackageInstallation.cpp
    services/CoreUpdateReleaseMetadata.h
    services/CoreUpdateReleaseMetadata.cpp
    services/CoreUpdateReleaseResolution.h
    services/CoreUpdateReleaseResolution.cpp
    services/CoreUpdateVersion.h
    services/CoreUpdateVersion.cpp
    services/RoutingService.h
    services/RoutingService.cpp
    services/SpeedTestBatchConfig.h
    services/SpeedTestBatchConfig.cpp
    services/SpeedTestPortReservation.h
    services/SpeedTestPortReservation.cpp
    services/SpeedTestRuntimeProcess.h
    services/SpeedTestRuntimeProcess.cpp
    services/SpeedTestRuntimeRunner.h
    services/SpeedTestRuntimeRunner.cpp
    services/SpeedTestBatchRuntimeRunner.h
    services/SpeedTestBatchRuntimeRunner.cpp
    services/SpeedTestUrlProbe.h
    services/SpeedTestUrlProbe.cpp
    services/SpeedTestRequestItem.h
    services/SpeedTestWorker.h
    services/SpeedTestWorker.cpp
    services/SpeedTestController.h
    services/SpeedTestController.cpp
    services/ProxyAvailabilityCheckService.h
    services/ProxyAvailabilityCheckService.cpp
    services/GeoResourceUpdateService.h
    services/GeoResourceUpdateService.cpp
    services/GeoResourceDownload.h
    services/GeoResourceDownload.cpp
    services/SubscriptionService.h
    services/SubscriptionService.cpp
    services/SubscriptionUpdateService.h
    services/SubscriptionUpdateService.cpp
    subscription/CustomConfigTextParser.h
    subscription/CustomConfigTextParser.cpp
    subscription/ClashSubscriptionParser.h
    subscription/ClashSubscriptionParser.cpp
    subscription/ClashProxyItemParser.h
    subscription/ClashProxyItemParser.cpp
    subscription/ClashYamlProxyParser.h
    subscription/ClashYamlProxyParser.cpp
    subscription/ShareUrlBuilder.h
    subscription/ShareUrlBuilder.cpp
    subscription/ShareUrlProtocolBuilders.h
    subscription/ShareUrlProtocolBuilders.cpp
    subscription/ShareUrlBuilderSupport.h
    subscription/ShareUrlBuilderSupport.cpp
    subscription/ShareUrlParser.h
    subscription/ShareUrlParser.cpp
    subscription/ShareUrlProtocolParsers.h
    subscription/ShareUrlProtocolParsers.cpp
    subscription/ShareUrlSingBoxProtocolParsers.cpp
    subscription/ShareUrlVmessParser.h
    subscription/ShareUrlVmessParser.cpp
    subscription/ShareUrlParserSupport.h
    subscription/ShareUrlParserSupport.cpp
    subscription/SubscriptionContentParser.h
    subscription/SubscriptionContentParser.cpp
    subscription/SubscriptionSingBoxParser.h
    subscription/SubscriptionSingBoxParser.cpp
    subscription/SubscriptionJsonSupport.h
    subscription/SubscriptionJsonSupport.cpp
    subscription/SubscriptionYamlParser.h
    subscription/SubscriptionYamlParser.cpp
    subscription/SubscriptionYamlScalarValueParser.h
    subscription/SubscriptionYamlScalarValueParser.cpp
    subscription/SubscriptionYamlFlowValueParser.h
    subscription/SubscriptionYamlFlowValueParser.cpp
    subscription/SubscriptionImportTextParser.h
    subscription/SubscriptionImportTextParser.cpp
    subscription/SubscriptionUrlImportService.h
    subscription/SubscriptionUrlImportService.cpp
    third_party/qrcodegen/qrcodegen.hpp
    third_party/qrcodegen/qrcodegen.cpp
    ui/dialogs/AddServerDialog.h
    ui/dialogs/AddServerDialog.cpp
    ui/dialogs/AddServerDialogServerMapping.cpp
    ui/dialogs/AddServerDialogUi.cpp
    ui/dialogs/AddServerDialogOptions.h
    ui/dialogs/AddServerDialogOptions.cpp
    ui/dialogs/CustomServerDialog.h
    ui/dialogs/CustomServerDialog.cpp
    ui/dialogs/AboutDialog.h
    ui/dialogs/AboutDialog.cpp
    ui/dialogs/DnsSettingsPageWidget.h
    ui/dialogs/DnsSettingsPageWidget.cpp
    ui/dialogs/GeneralSettingsPageWidget.h
    ui/dialogs/GeneralSettingsPageWidget.cpp
    ui/dialogs/CoreSettingsPageWidget.h
    ui/dialogs/CoreSettingsPageWidget.cpp
    ui/dialogs/CoreSettingsPageWidgetUi.cpp
    ui/dialogs/CoreSettingsPageSupport.h
    ui/dialogs/CoreSettingsPageSupport.cpp
    ui/dialogs/RoutingBaseRouteCard.h
    ui/dialogs/RoutingBaseRouteCard.cpp
    ui/dialogs/RoutingCustomRuleSupport.h
    ui/dialogs/RoutingCustomRuleSupport.cpp
    ui/dialogs/RoutingSettingsPageWidget.h
    ui/dialogs/RoutingSettingsPageWidget.cpp
    ui/dialogs/RoutingSettingsPageWidgetUi.cpp
    ui/dialogs/SubscriptionSettingsPageWidget.h
    ui/dialogs/SubscriptionSettingsPageWidget.cpp
    ui/dialogs/SettingsDialog.h
    ui/dialogs/SettingsDialog.cpp
    ui/dialogs/TunSettingsPageWidget.h
    ui/dialogs/TunSettingsPageWidget.cpp
    ui/dialogs/UwpLoopbackDialog.h
    ui/dialogs/UwpLoopbackDialog.cpp
    ui/dialogs/UwpLoopbackDialogSupport.h
    ui/dialogs/UwpLoopbackDialogSupport.cpp
    ui/theme/AppTheme.h
    ui/theme/AppTheme.cpp
    ui/theme/AppThemePalette.h
    ui/theme/AppThemePalette.cpp
    ui/theme/AppThemeStyle.h
    ui/theme/AppThemeStyle.cpp
    ui/theme/ServerTableTheme.h
    ui/theme/ServerTableTheme.cpp
    ui/mainwindow/MainWindow.h
    ui/mainwindow/MainWindow.cpp
    ui/mainwindow/ToolbarWidgets.h
    ui/mainwindow/ToolbarWidgets.cpp
    ui/mainwindow/MainWindowToolbarActions.h
    ui/mainwindow/MainWindowToolbarActions.cpp
    ui/mainwindow/MainWindowConfigState.h
    ui/mainwindow/MainWindowConfigState.cpp
    ui/mainwindow/MainWindowServerSelectionSupport.h
    ui/mainwindow/MainWindowServerSelectionSupport.cpp
    ui/mainwindow/LogPanelWidget.h
    ui/mainwindow/LogPanelWidget.cpp
    ui/mainwindow/LogPanelSupport.h
    ui/mainwindow/LogPanelSupport.cpp
    ui/mainwindow/BackgroundTaskActionsController.h
    ui/mainwindow/BackgroundTaskActionsController.cpp
    ui/mainwindow/RoutingModeController.h
    ui/mainwindow/RoutingModeController.cpp
    ui/mainwindow/SubscriptionViewSupport.h
    ui/mainwindow/SubscriptionViewSupport.cpp
    ui/mainwindow/SubscriptionViewController.h
    ui/mainwindow/SubscriptionViewController.cpp
    ui/mainwindow/ServerWorkspaceWidget.h
    ui/mainwindow/ServerWorkspaceWidget.cpp
    ui/mainwindow/MainWindowShortcutController.h
    ui/mainwindow/MainWindowShortcutController.cpp
    ui/mainwindow/MainWindowTitleSupport.h
    ui/mainwindow/MainWindowTitleSupport.cpp
    ui/mainwindow/WindowLayoutStateSupport.h
    ui/mainwindow/WindowLayoutStateSupport.cpp
    ui/mainwindow/WindowLayoutStateController.h
    ui/mainwindow/WindowLayoutStateController.cpp
    ui/mainwindow/ServerListController.h
    ui/mainwindow/ServerListController.cpp
    ui/mainwindow/SharePanelWidget.h
    ui/mainwindow/SharePanelWidget.cpp
    ui/mainwindow/StartupOverlayWidget.h
    ui/mainwindow/StartupOverlayWidget.cpp
    ui/mainwindow/ProxyToolbarController.h
    ui/mainwindow/ProxyToolbarController.cpp
    ui/mainwindow/ServerSelectionController.h
    ui/mainwindow/ServerSelectionController.cpp
    ui/mainwindow/ServerActionsController.h
    ui/mainwindow/ServerActionsController.cpp
    ui/mainwindow/StatusBarSupport.h
    ui/mainwindow/StatusBarSupport.cpp
    ui/mainwindow/StatusBarController.h
    ui/mainwindow/StatusBarController.cpp
    ui/mainwindow/LogItemDelegate.h
    ui/mainwindow/LogItemDelegate.cpp
    ui/mainwindow/ServerTableView.h
    ui/mainwindow/ServerTableView.cpp
    ui/models/LogFilterProxyModel.h
    ui/models/LogFilterProxyModel.cpp
    ui/models/LogListModel.h
    ui/models/LogListModel.cpp
    ui/models/ServerFilterProxyModel.h
    ui/models/ServerFilterProxyModel.cpp
    ui/models/ServerTableModel.h
    ui/models/ServerTableModel.cpp
    ui/qr/QrCodeRenderer.h
    ui/qr/QrCodeRenderer.cpp
    resources/app_icons.qrc
    resources/toolbar_icons.qrc
    resources/themes.qrc
    ui/tray/TrayController.h
    ui/tray/TrayController.cpp
    ui/tray/TrayMenuSupport.h
    ui/tray/TrayMenuSupport.cpp
)

if(WIN32)
    list(APPEND SONGBIRD_SOURCES
        resources/SongBird.rc
    )
endif()

set(SONGBIRD_TRANSLATION_TS_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../translations/SongBird_zh_CN.ts
)

set(SONGBIRD_QT_TRANSLATION_QM_FILES)
if(TARGET Qt${QT_VERSION_MAJOR}::qmake)
    get_target_property(SONGBIRD_QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
endif()
if(SONGBIRD_QMAKE_EXECUTABLE)
    execute_process(
        COMMAND "${SONGBIRD_QMAKE_EXECUTABLE}" -query QT_INSTALL_TRANSLATIONS
        OUTPUT_VARIABLE SONGBIRD_QT_TRANSLATIONS_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()
if(NOT SONGBIRD_QT_TRANSLATIONS_DIR AND QT_VERSION_MAJOR EQUAL 5)
    message(WARNING "Qt translations directory not found. Set SONGBIRD_QT_TRANSLATIONS_DIR manually or ensure qmake is discoverable.")
endif()

set(SONGBIRD_QT_ZH_QM
    "${SONGBIRD_QT_TRANSLATIONS_DIR}/qt_zh_CN.qm"
)
if(EXISTS "${SONGBIRD_QT_ZH_QM}")
    list(APPEND SONGBIRD_QT_TRANSLATION_QM_FILES "${SONGBIRD_QT_ZH_QM}")
endif()

set(SONGBIRD_QTBASE_ZH_QM
    "${SONGBIRD_QT_TRANSLATIONS_DIR}/qtbase_zh_CN.qm"
)
if(EXISTS "${SONGBIRD_QTBASE_ZH_QM}")
    list(APPEND SONGBIRD_QT_TRANSLATION_QM_FILES "${SONGBIRD_QTBASE_ZH_QM}")
endif()

set(SONGBIRD_TRANSLATION_QM_FILES)
if(SONGBIRD_HAS_LINGUISTTOOLS)
    if(QT_VERSION_MAJOR GREATER_EQUAL 6)
        qt_add_translation(SONGBIRD_TRANSLATION_QM_FILES ${SONGBIRD_TRANSLATION_TS_FILES})
    else()
        qt5_add_translation(SONGBIRD_TRANSLATION_QM_FILES ${SONGBIRD_TRANSLATION_TS_FILES})
    endif()

    add_custom_target(songbird_translations ALL DEPENDS ${SONGBIRD_TRANSLATION_QM_FILES})

    # Generate a .qrc that embeds compiled .qm files into the executable
    set(SONGBIRD_TRANSLATIONS_QRC "${CMAKE_CURRENT_BINARY_DIR}/translations.qrc")
    file(WRITE "${SONGBIRD_TRANSLATIONS_QRC}" "<RCC>\n    <qresource prefix=\"/translations\">\n")
    foreach(qm_file ${SONGBIRD_TRANSLATION_QM_FILES})
        get_filename_component(qm_name "${qm_file}" NAME)
        file(APPEND "${SONGBIRD_TRANSLATIONS_QRC}" "        <file alias=\"${qm_name}\">${qm_file}</file>\n")
    endforeach()
    foreach(qm_file ${SONGBIRD_QT_TRANSLATION_QM_FILES})
        get_filename_component(qm_name "${qm_file}" NAME)
        file(APPEND "${SONGBIRD_TRANSLATIONS_QRC}" "        <file alias=\"${qm_name}\">${qm_file}</file>\n")
    endforeach()
    file(APPEND "${SONGBIRD_TRANSLATIONS_QRC}" "    </qresource>\n</RCC>\n")

    # Ensure .qm files exist before rcc processes the .qrc
    set_source_files_properties("${SONGBIRD_TRANSLATIONS_QRC}" PROPERTIES
        OBJECT_DEPENDS "${SONGBIRD_TRANSLATION_QM_FILES}"
    )

    list(APPEND SONGBIRD_SOURCES "${SONGBIRD_TRANSLATIONS_QRC}")
endif()


add_library(songbird_core_descriptors OBJECT
    runtime/core/CoreDescriptor.h
    runtime/core/CoreDescriptorRegistry.h
    runtime/core/CoreDescriptorRegistry.cpp
    backends/singbox/SingBoxCoreDescriptor.h
    backends/singbox/SingBoxCoreDescriptor.cpp
    backends/xray/XrayCoreDescriptor.h
    backends/xray/XrayCoreDescriptor.cpp
)

target_include_directories(songbird_core_descriptors
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(songbird_core_descriptors
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
)

if(MSVC)
    target_compile_options(songbird_core_descriptors PRIVATE /Z7)
endif()

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird
        MANUAL_FINALIZATION
        WIN32
        ${SONGBIRD_SOURCES}
    )
else()
    add_executable(songbird WIN32 ${SONGBIRD_SOURCES})
endif()

target_include_directories(songbird
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(songbird
    PRIVATE
        SONGBIRD_APP_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(songbird
    PRIVATE
        songbird_core_descriptors
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Svg
)

set_target_properties(songbird PROPERTIES
    OUTPUT_NAME "SongBird"
)

option(SONGBIRD_ENABLE_UPX "Compress release SongBird executable with UPX when upx is available" ON)
set(SONGBIRD_UPX_BUILD_TYPES "Release;MinSizeRel" CACHE STRING "Build types that run UPX on SongBird executable")
if(SONGBIRD_ENABLE_UPX)
    find_program(SONGBIRD_UPX_EXECUTABLE NAMES upx upx.exe)
    if(SONGBIRD_UPX_EXECUTABLE)
        message(STATUS "UPX found: ${SONGBIRD_UPX_EXECUTABLE}")
        if(CMAKE_BUILD_TYPE IN_LIST SONGBIRD_UPX_BUILD_TYPES)
            add_custom_command(TARGET songbird POST_BUILD
                COMMAND "${SONGBIRD_UPX_EXECUTABLE}" -9 "$<TARGET_FILE:songbird>"
                COMMENT "Compressing SongBird executable with UPX"
                VERBATIM
            )
        else()
            message(STATUS "UPX compression skipped for build type: ${CMAKE_BUILD_TYPE}")
        endif()
    else()
        message(STATUS "UPX not found; SongBird executable compression disabled.")
    endif()
endif()

if(TARGET songbird_translations)
    add_dependencies(songbird songbird_translations)
endif()

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird)
endif()

if(WIN32)
    target_link_libraries(songbird PRIVATE Advapi32 Shell32 Wininet User32 Iphlpapi)
endif()

if(WIN32 AND MSVC)
    # The Windows manifest is embedded via resources/SongBird.rc (RT_MANIFEST).
    # Disable the linker's auto-generated manifest to avoid LNK4078 conflicts
    # and to keep dpiAwareness=PerMonitorV2 authoritative.
    target_link_options(songbird PRIVATE /MANIFEST:NO)
endif()

install(TARGETS songbird
    RUNTIME DESTINATION .
)

if(SONGBIRD_TRANSLATION_QM_FILES)
    install(FILES ${SONGBIRD_TRANSLATION_QM_FILES}
        DESTINATION translations
    )
endif()

install(FILES ${SONGBIRD_TRANSLATION_TS_FILES}
    DESTINATION translations
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../README.md
    ${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE
    DESTINATION .
)
