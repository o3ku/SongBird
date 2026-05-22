set(SONGBIRD_SOURCES
    app/main.cpp
    app/AppBootstrap.h
    app/AppBootstrap.cpp
    app/ConfigPathResolver.h
    app/CoreStartupCheckpoint.h
    app/SettingsDialogSession.h
    app/SettingsDialogSession.cpp
    app/SingleInstanceBootstrap.h
    app/SingleInstanceBootstrap.cpp
    app/StartupAdminElevation.h
    common/OperationResult.h
    common/DataSizeFormatter.h
    common/GitHubMirrorHelper.h
    common/ServerDisplayName.h
    common/SystemProxyMode.h
    common/UserAgent.h
    domain/models/Config.h
    domain/models/PolicyGroupItem.h
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
    runtime/CoreInfo.h
    runtime/ProtocolCoreCompat.h
    runtime/CoreConfigPreflight.h
    runtime/CoreConfigPreflight.cpp
    runtime/ClientConfigWriter.h
    runtime/ClientConfigWriter.cpp
    runtime/ServerConfigWriter.h
    runtime/ServerConfigWriter.cpp
    runtime/ICoreProcessHost.h
    runtime/QtCoreProcessHost.h
    runtime/QtCoreProcessHost.cpp
    runtime/CoreLifecycleService.h
    runtime/CoreLifecycleService.cpp
    persistence/IConfigRepository.h
    persistence/JsonConfigRepository.h
    persistence/JsonConfigRepository.cpp
    persistence/JsonConfigCollectionSerialization.h
    persistence/JsonConfigCollectionSerialization.cpp
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
    services/ConfigBackupService.h
    services/ConfigBackupService.cpp
    services/PolicyGroupService.h
    services/PolicyGroupService.cpp
    services/CoreUpdateService.h
    services/CoreUpdateService.cpp
    services/RoutingService.h
    services/RoutingService.cpp
    services/SpeedTestService.h
    services/SpeedTestService.cpp
    services/ProxyAvailabilityCheckService.h
    services/ProxyAvailabilityCheckService.cpp
    services/GeoResourceUpdateService.h
    services/GeoResourceUpdateService.cpp
    services/SubscriptionService.h
    services/SubscriptionService.cpp
    services/SubscriptionUpdateService.h
    services/SubscriptionUpdateService.cpp
    subscription/CustomConfigTextParser.h
    subscription/CustomConfigTextParser.cpp
    subscription/ConfigFileImportParser.h
    subscription/ConfigFileImportParser.cpp
    subscription/ShareUrlBuilder.h
    subscription/ShareUrlBuilder.cpp
    subscription/ShareUrlParser.h
    subscription/ShareUrlParser.cpp
    subscription/SubscriptionContentParser.h
    subscription/SubscriptionContentParser.cpp
    subscription/SubscriptionImportTextParser.h
    subscription/SubscriptionImportTextParser.cpp
    third_party/qrcodegen/qrcodegen.hpp
    third_party/qrcodegen/qrcodegen.cpp
    ui/dialogs/AddServerDialog.h
    ui/dialogs/AddServerDialog.cpp
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
    ui/dialogs/RoutingSettingsPageWidget.h
    ui/dialogs/RoutingSettingsPageWidget.cpp
    ui/dialogs/SubscriptionSettingsPageWidget.h
    ui/dialogs/SubscriptionSettingsPageWidget.cpp
    ui/dialogs/SettingsDialog.h
    ui/dialogs/SettingsDialog.cpp
    ui/dialogs/TunSettingsPageWidget.h
    ui/dialogs/TunSettingsPageWidget.cpp
    ui/theme/AppTheme.h
    ui/theme/AppTheme.cpp
    ui/mainwindow/MainWindow.h
    ui/mainwindow/MainWindow.cpp
    ui/mainwindow/LogPanelWidget.h
    ui/mainwindow/LogPanelWidget.cpp
    ui/mainwindow/BackgroundTaskActionsController.h
    ui/mainwindow/BackgroundTaskActionsController.cpp
    ui/mainwindow/RoutingModeController.h
    ui/mainwindow/RoutingModeController.cpp
    ui/mainwindow/SubscriptionViewController.h
    ui/mainwindow/SubscriptionViewController.cpp
    ui/mainwindow/ServerWorkspaceWidget.h
    ui/mainwindow/ServerWorkspaceWidget.cpp
    ui/mainwindow/WindowLayoutStateController.h
    ui/mainwindow/WindowLayoutStateController.cpp
    ui/mainwindow/ServerListController.h
    ui/mainwindow/ServerListController.cpp
    ui/mainwindow/SharePanelWidget.h
    ui/mainwindow/SharePanelWidget.cpp
    ui/mainwindow/ProxyToolbarController.h
    ui/mainwindow/ProxyToolbarController.cpp
    ui/mainwindow/ServerSelectionController.h
    ui/mainwindow/ServerSelectionController.cpp
    ui/mainwindow/ServerActionsController.h
    ui/mainwindow/ServerActionsController.cpp
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
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Svg
)

set_target_properties(songbird PROPERTIES
    OUTPUT_NAME "SongBird"
)

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


