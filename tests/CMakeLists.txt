find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test Widgets Network Svg)

set(SONGBIRD_SHARE_URL_TEST_SOURCES
    ShareUrlTests.cpp
    ../src/subscription/ShareUrlBuilder.h
    ../src/subscription/ShareUrlBuilder.cpp
    ../src/subscription/ShareUrlParser.h
    ../src/subscription/ShareUrlParser.cpp
    ../src/subscription/SubscriptionImportTextParser.h
    ../src/subscription/SubscriptionImportTextParser.cpp
    ../src/common/PortValidator.h
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_share_url_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_SHARE_URL_TEST_SOURCES}
    )
else()
    add_executable(songbird_share_url_tests ${SONGBIRD_SHARE_URL_TEST_SOURCES})
endif()

target_include_directories(songbird_share_url_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_share_url_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

add_test(NAME share-url-transports COMMAND songbird_share_url_tests)
set_tests_properties(share-url-transports PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_share_url_tests)
endif()

set(SONGBIRD_ADD_SERVER_DIALOG_TEST_SOURCES
    AddServerDialogTests.cpp
    ../src/ui/dialogs/AddServerDialog.h
    ../src/ui/dialogs/AddServerDialog.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_add_server_dialog_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_ADD_SERVER_DIALOG_TEST_SOURCES}
    )
else()
    add_executable(songbird_add_server_dialog_tests ${SONGBIRD_ADD_SERVER_DIALOG_TEST_SOURCES})
endif()

target_include_directories(songbird_add_server_dialog_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_add_server_dialog_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME add-server-dialog-roundtrip COMMAND songbird_add_server_dialog_tests)
set_tests_properties(add-server-dialog-roundtrip PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_add_server_dialog_tests)
endif()

set(SONGBIRD_SETTINGS_DIALOG_TEST_SOURCES
    SettingsDialogTests.cpp
    ../src/ui/dialogs/CoreSettingsPageWidget.h
    ../src/ui/dialogs/CoreSettingsPageWidget.cpp
    ../src/ui/dialogs/DnsSettingsPageWidget.h
    ../src/ui/dialogs/DnsSettingsPageWidget.cpp
    ../src/ui/dialogs/GeneralSettingsPageWidget.h
    ../src/ui/dialogs/GeneralSettingsPageWidget.cpp
    ../src/ui/dialogs/RoutingSettingsPageWidget.h
    ../src/ui/dialogs/RoutingSettingsPageWidget.cpp
    ../src/ui/dialogs/SubscriptionSettingsPageWidget.h
    ../src/ui/dialogs/SubscriptionSettingsPageWidget.cpp
    ../src/ui/dialogs/SettingsDialog.h
    ../src/ui/dialogs/SettingsDialog.cpp
    ../src/ui/dialogs/TunSettingsPageWidget.h
    ../src/ui/dialogs/TunSettingsPageWidget.cpp
    ../src/ui/theme/AppTheme.h
    ../src/ui/theme/AppTheme.cpp
    ../src/resources/themes.qrc
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_settings_dialog_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_SETTINGS_DIALOG_TEST_SOURCES}
    )
else()
    add_executable(songbird_settings_dialog_tests ${SONGBIRD_SETTINGS_DIALOG_TEST_SOURCES})
endif()

target_include_directories(songbird_settings_dialog_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_settings_dialog_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Svg
)

add_test(NAME settings-dialog-download COMMAND songbird_settings_dialog_tests)
set_tests_properties(settings-dialog-download PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_settings_dialog_tests)
endif()

set(SONGBIRD_TUN_SETTINGS_APPLY_DECISION_TEST_SOURCES
    TunSettingsApplyDecisionTests.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_tun_settings_apply_decision_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_TUN_SETTINGS_APPLY_DECISION_TEST_SOURCES}
    )
else()
    add_executable(songbird_tun_settings_apply_decision_tests ${SONGBIRD_TUN_SETTINGS_APPLY_DECISION_TEST_SOURCES})
endif()

target_include_directories(songbird_tun_settings_apply_decision_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_tun_settings_apply_decision_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME tun-settings-apply-decision COMMAND songbird_tun_settings_apply_decision_tests)
set_tests_properties(tun-settings-apply-decision PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_tun_settings_apply_decision_tests)
endif()

set(SONGBIRD_SETTINGS_DIALOG_APPLY_PLAN_TEST_SOURCES
    SettingsDialogApplyPlanTests.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_settings_dialog_apply_plan_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_SETTINGS_DIALOG_APPLY_PLAN_TEST_SOURCES}
    )
else()
    add_executable(songbird_settings_dialog_apply_plan_tests ${SONGBIRD_SETTINGS_DIALOG_APPLY_PLAN_TEST_SOURCES})
endif()

target_include_directories(songbird_settings_dialog_apply_plan_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_settings_dialog_apply_plan_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME settings-dialog-apply-plan COMMAND songbird_settings_dialog_apply_plan_tests)
set_tests_properties(settings-dialog-apply-plan PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_settings_dialog_apply_plan_tests)
endif()

set(SONGBIRD_STARTUP_ADMIN_ELEVATION_TEST_SOURCES
    StartupAdminElevationTests.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_startup_admin_elevation_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_STARTUP_ADMIN_ELEVATION_TEST_SOURCES}
    )
else()
    add_executable(songbird_startup_admin_elevation_tests ${SONGBIRD_STARTUP_ADMIN_ELEVATION_TEST_SOURCES})
endif()

target_include_directories(songbird_startup_admin_elevation_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_startup_admin_elevation_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME startup-admin-elevation COMMAND songbird_startup_admin_elevation_tests)
set_tests_properties(startup-admin-elevation PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_startup_admin_elevation_tests)
endif()

set(SONGBIRD_APP_BOOTSTRAP_TUN_RUNTIME_TEST_SOURCES
    AppBootstrapTunRuntimeTests.cpp
    ../src/app/CoreStartupCheckpoint.h
    ../src/runtime/CoreConfigPreflight.h
    ../src/runtime/CoreConfigPreflight.cpp
    ../src/runtime/DnsConfigFragments.h
    ../src/runtime/DnsConfigFragments.cpp
    ../src/runtime/ProtocolConfigMapper.h
    ../src/runtime/ProtocolConfigMapper.cpp
    ../src/runtime/RoutingConfigFragments.h
    ../src/runtime/RoutingConfigFragments.cpp
    ../src/runtime/RoutingRuleJsonMapper.h
    ../src/runtime/RoutingRuleJsonMapper.cpp
    ../src/runtime/core/ICoreBackend.h
    ../src/runtime/core/CoreCatalog.h
    ../src/runtime/core/CoreBackendRegistry.h
    ../src/runtime/core/CoreBackendRegistry.cpp
    ../src/runtime/core/singbox/SingBoxConfigFragments.h
    ../src/runtime/core/singbox/SingBoxConfigFragments.cpp
    ../src/runtime/core/singbox/SingBoxCoreBackend.h
    ../src/runtime/core/singbox/SingBoxCoreBackend.cpp
    ../src/runtime/core/xray/XrayConfigFragments.h
    ../src/runtime/core/xray/XrayConfigFragments.cpp
    ../src/runtime/core/xray/XrayCoreBackend.h
    ../src/runtime/core/xray/XrayCoreBackend.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_app_bootstrap_tun_runtime_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_APP_BOOTSTRAP_TUN_RUNTIME_TEST_SOURCES}
    )
else()
    add_executable(songbird_app_bootstrap_tun_runtime_tests ${SONGBIRD_APP_BOOTSTRAP_TUN_RUNTIME_TEST_SOURCES})
endif()

target_include_directories(songbird_app_bootstrap_tun_runtime_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_app_bootstrap_tun_runtime_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME app-bootstrap-tun-runtime COMMAND songbird_app_bootstrap_tun_runtime_tests)
set_tests_properties(app-bootstrap-tun-runtime PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_app_bootstrap_tun_runtime_tests)
endif()

set(SONGBIRD_APP_BOOTSTRAP_UI_COMMAND_PLAN_TEST_SOURCES
    AppBootstrapUiCommandPlanTests.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_app_bootstrap_ui_command_plan_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_APP_BOOTSTRAP_UI_COMMAND_PLAN_TEST_SOURCES}
    )
else()
    add_executable(songbird_app_bootstrap_ui_command_plan_tests ${SONGBIRD_APP_BOOTSTRAP_UI_COMMAND_PLAN_TEST_SOURCES})
endif()

target_include_directories(songbird_app_bootstrap_ui_command_plan_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_app_bootstrap_ui_command_plan_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME app-bootstrap-ui-command-plan COMMAND songbird_app_bootstrap_ui_command_plan_tests)
set_tests_properties(app-bootstrap-ui-command-plan PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_app_bootstrap_ui_command_plan_tests)
endif()

set(SONGBIRD_RUNTIME_STATE_TEST_SOURCES
    RuntimeStateTests.cpp
    ../src/app/RuntimeState.h
    ../src/app/RuntimeState.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_runtime_state_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_RUNTIME_STATE_TEST_SOURCES}
    )
else()
    add_executable(songbird_runtime_state_tests ${SONGBIRD_RUNTIME_STATE_TEST_SOURCES})
endif()

target_include_directories(songbird_runtime_state_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_runtime_state_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
)

add_test(NAME runtime-state COMMAND songbird_runtime_state_tests)
set_tests_properties(runtime-state PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_runtime_state_tests)
endif()

set(SONGBIRD_MAIN_WINDOW_TEST_SOURCES
    MainWindowTests.cpp
    ../src/subscription/ShareUrlBuilder.h
    ../src/subscription/ShareUrlBuilder.cpp
    ../src/third_party/qrcodegen/qrcodegen.hpp
    ../src/third_party/qrcodegen/qrcodegen.cpp
    ../src/ui/mainwindow/MainWindow.h
    ../src/ui/mainwindow/MainWindow.cpp
    ../src/ui/mainwindow/LogPanelWidget.h
    ../src/ui/mainwindow/LogPanelWidget.cpp
    ../src/ui/mainwindow/BackgroundTaskActionsController.h
    ../src/ui/mainwindow/BackgroundTaskActionsController.cpp
    ../src/ui/mainwindow/RoutingModeController.h
    ../src/ui/mainwindow/RoutingModeController.cpp
    ../src/ui/mainwindow/SubscriptionViewController.h
    ../src/ui/mainwindow/SubscriptionViewController.cpp
    ../src/ui/mainwindow/ServerWorkspaceWidget.h
    ../src/ui/mainwindow/ServerWorkspaceWidget.cpp
    ../src/ui/mainwindow/WindowLayoutStateController.h
    ../src/ui/mainwindow/WindowLayoutStateController.cpp
    ../src/ui/mainwindow/ServerListController.h
    ../src/ui/mainwindow/ServerListController.cpp
    ../src/ui/mainwindow/SharePanelWidget.h
    ../src/ui/mainwindow/SharePanelWidget.cpp
    ../src/ui/mainwindow/ProxyToolbarController.h
    ../src/ui/mainwindow/ProxyToolbarController.cpp
    ../src/ui/mainwindow/ServerSelectionController.h
    ../src/ui/mainwindow/ServerSelectionController.cpp
    ../src/ui/mainwindow/ServerActionsController.h
    ../src/ui/mainwindow/ServerActionsController.cpp
    ../src/ui/mainwindow/StatusBarController.h
    ../src/ui/mainwindow/StatusBarController.cpp
    ../src/ui/mainwindow/LogItemDelegate.h
    ../src/ui/mainwindow/LogItemDelegate.cpp
    ../src/ui/mainwindow/ServerTableView.h
    ../src/ui/mainwindow/ServerTableView.cpp
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
    ../src/resources/app_icons.qrc
    ../src/resources/toolbar_icons.qrc
    ../src/resources/themes.qrc
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_main_window_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_MAIN_WINDOW_TEST_SOURCES}
    )
else()
    add_executable(songbird_main_window_tests ${SONGBIRD_MAIN_WINDOW_TEST_SOURCES})
endif()

target_include_directories(songbird_main_window_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_main_window_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Svg
)

add_test(NAME main-window-log-scroll COMMAND songbird_main_window_tests)
set_tests_properties(main-window-log-scroll PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_main_window_tests)
endif()

set(SONGBIRD_CLIENT_CONFIG_WRITER_TEST_SOURCES
    ClientConfigWriterTests.cpp
    ../src/runtime/ClientConfigWriter.h
    ../src/runtime/ClientConfigWriter.cpp
    ../src/runtime/DnsConfigFragments.h
    ../src/runtime/DnsConfigFragments.cpp
    ../src/runtime/ProtocolConfigMapper.h
    ../src/runtime/ProtocolConfigMapper.cpp
    ../src/runtime/RoutingConfigFragments.h
    ../src/runtime/RoutingConfigFragments.cpp
    ../src/runtime/RoutingRuleJsonMapper.h
    ../src/runtime/RoutingRuleJsonMapper.cpp
    ../src/runtime/core/ICoreBackend.h
    ../src/runtime/core/CoreCatalog.h
    ../src/runtime/core/CoreBackendRegistry.h
    ../src/runtime/core/CoreBackendRegistry.cpp
    ../src/runtime/core/singbox/SingBoxConfigFragments.h
    ../src/runtime/core/singbox/SingBoxConfigFragments.cpp
    ../src/runtime/core/singbox/SingBoxCoreBackend.h
    ../src/runtime/core/singbox/SingBoxCoreBackend.cpp
    ../src/runtime/core/xray/XrayConfigFragments.h
    ../src/runtime/core/xray/XrayConfigFragments.cpp
    ../src/runtime/core/xray/XrayCoreBackend.h
    ../src/runtime/core/xray/XrayCoreBackend.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_client_config_writer_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_CLIENT_CONFIG_WRITER_TEST_SOURCES}
    )
else()
    add_executable(songbird_client_config_writer_tests ${SONGBIRD_CLIENT_CONFIG_WRITER_TEST_SOURCES})
endif()

target_include_directories(songbird_client_config_writer_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_client_config_writer_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME client-config-writer-tun-compat COMMAND songbird_client_config_writer_tests)
set_tests_properties(client-config-writer-tun-compat PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_client_config_writer_tests)
endif()

set(SONGBIRD_TUN_COMPAT_CORE_REQUIREMENT_TEST_SOURCES
    TunCompatCoreRequirementTests.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_tun_compat_core_requirement_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_TUN_COMPAT_CORE_REQUIREMENT_TEST_SOURCES}
    )
else()
    add_executable(songbird_tun_compat_core_requirement_tests ${SONGBIRD_TUN_COMPAT_CORE_REQUIREMENT_TEST_SOURCES})
endif()

target_include_directories(songbird_tun_compat_core_requirement_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_tun_compat_core_requirement_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME tun-compat-core-requirement COMMAND songbird_tun_compat_core_requirement_tests)
set_tests_properties(tun-compat-core-requirement PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_tun_compat_core_requirement_tests)
endif()

set(SONGBIRD_JSON_CONFIG_REPOSITORY_TEST_SOURCES
    JsonConfigRepositoryTests.cpp
    ../src/persistence/JsonConfigRepository.h
    ../src/persistence/JsonConfigRepository.cpp
    ../src/persistence/JsonConfigCollectionSerialization.h
    ../src/persistence/JsonConfigCollectionSerialization.cpp
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

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_json_config_repository_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_JSON_CONFIG_REPOSITORY_TEST_SOURCES}
    )
else()
    add_executable(songbird_json_config_repository_tests ${SONGBIRD_JSON_CONFIG_REPOSITORY_TEST_SOURCES})
endif()

target_include_directories(songbird_json_config_repository_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_json_config_repository_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME json-config-repository-defaults COMMAND songbird_json_config_repository_tests)
set_tests_properties(json-config-repository-defaults PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_json_config_repository_tests)
endif()

set(SONGBIRD_CONFIG_FILE_IMPORT_PARSER_TEST_SOURCES
    ConfigFileImportParserTests.cpp
    ../src/subscription/ConfigFileImportParser.h
    ../src/subscription/ConfigFileImportParser.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_config_file_import_parser_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_CONFIG_FILE_IMPORT_PARSER_TEST_SOURCES}
    )
else()
    add_executable(songbird_config_file_import_parser_tests ${SONGBIRD_CONFIG_FILE_IMPORT_PARSER_TEST_SOURCES})
endif()

target_include_directories(songbird_config_file_import_parser_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_config_file_import_parser_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME config-file-import-parser-stream-settings COMMAND songbird_config_file_import_parser_tests)
set_tests_properties(config-file-import-parser-stream-settings PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_config_file_import_parser_tests)
endif()

set(SONGBIRD_SERVER_CONFIG_WRITER_TEST_SOURCES
    ServerConfigWriterTests.cpp
    ../src/runtime/ServerConfigWriter.h
    ../src/runtime/ServerConfigWriter.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_server_config_writer_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_SERVER_CONFIG_WRITER_TEST_SOURCES}
    )
else()
    add_executable(songbird_server_config_writer_tests ${SONGBIRD_SERVER_CONFIG_WRITER_TEST_SOURCES})
endif()

target_include_directories(songbird_server_config_writer_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_server_config_writer_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME server-config-writer COMMAND songbird_server_config_writer_tests)
set_tests_properties(server-config-writer PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_server_config_writer_tests)
endif()

set(SONGBIRD_PROXY_AVAILABILITY_CHECK_TEST_SOURCES
    ProxyAvailabilityCheckServiceTests.cpp
    ../src/services/ProxyAvailabilityCheckService.h
    ../src/services/ProxyAvailabilityCheckService.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_proxy_availability_check_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_PROXY_AVAILABILITY_CHECK_TEST_SOURCES}
    )
else()
    add_executable(songbird_proxy_availability_check_tests ${SONGBIRD_PROXY_AVAILABILITY_CHECK_TEST_SOURCES})
endif()

target_include_directories(songbird_proxy_availability_check_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_proxy_availability_check_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

add_test(NAME proxy-availability-check COMMAND songbird_proxy_availability_check_tests)
set_tests_properties(proxy-availability-check PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_proxy_availability_check_tests)
endif()

set(SONGBIRD_SPEED_TEST_SERVICE_INTERNAL_TEST_SOURCES
    SpeedTestServiceInternalTests.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_speed_test_service_internal_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_SPEED_TEST_SERVICE_INTERNAL_TEST_SOURCES}
    )
else()
    add_executable(songbird_speed_test_service_internal_tests ${SONGBIRD_SPEED_TEST_SERVICE_INTERNAL_TEST_SOURCES})
endif()

target_include_directories(songbird_speed_test_service_internal_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_speed_test_service_internal_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

add_test(NAME speed-test-service-internal COMMAND songbird_speed_test_service_internal_tests)
set_tests_properties(speed-test-service-internal PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_speed_test_service_internal_tests)
endif()

set(SONGBIRD_SUBSCRIPTION_SERVICE_TEST_SOURCES
    SubscriptionServiceTests.cpp
    ../src/services/SubscriptionService.h
    ../src/services/SubscriptionService.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_subscription_service_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_SUBSCRIPTION_SERVICE_TEST_SOURCES}
    )
else()
    add_executable(songbird_subscription_service_tests ${SONGBIRD_SUBSCRIPTION_SERVICE_TEST_SOURCES})
endif()

target_include_directories(songbird_subscription_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_subscription_service_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME subscription-service COMMAND songbird_subscription_service_tests)
set_tests_properties(subscription-service PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_subscription_service_tests)
endif()

set(SONGBIRD_ROUTING_SERVICE_TEST_SOURCES
    RoutingServiceTests.cpp
    ../src/services/RoutingService.h
    ../src/services/RoutingService.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_routing_service_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_ROUTING_SERVICE_TEST_SOURCES}
    )
else()
    add_executable(songbird_routing_service_tests ${SONGBIRD_ROUTING_SERVICE_TEST_SOURCES})
endif()

target_include_directories(songbird_routing_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_routing_service_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME routing-service COMMAND songbird_routing_service_tests)
set_tests_properties(routing-service PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_routing_service_tests)
endif()

set(SONGBIRD_SYSTEM_PROXY_MODE_TEST_SOURCES
    SystemProxyModeTests.cpp
    ../src/common/SystemProxyMode.h
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_system_proxy_mode_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_SYSTEM_PROXY_MODE_TEST_SOURCES}
    )
else()
    add_executable(songbird_system_proxy_mode_tests ${SONGBIRD_SYSTEM_PROXY_MODE_TEST_SOURCES})
endif()

target_include_directories(songbird_system_proxy_mode_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_system_proxy_mode_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME system-proxy-mode COMMAND songbird_system_proxy_mode_tests)
set_tests_properties(system-proxy-mode PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_system_proxy_mode_tests)
endif()


set(SONGBIRD_APP_UPDATE_SERVICE_TEST_SOURCES
    AppUpdateServiceTests.cpp
    ../src/services/AppUpdateService.h
    ../src/services/AppUpdateService.cpp
    ../src/common/GitHubMirrorHelper.h
    ../src/common/GitHubUrls.h
    ../src/common/OperationResult.h
    ../src/common/UserAgent.h
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_app_update_service_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_APP_UPDATE_SERVICE_TEST_SOURCES}
    )
else()
    add_executable(songbird_app_update_service_tests ${SONGBIRD_APP_UPDATE_SERVICE_TEST_SOURCES})
endif()

target_include_directories(songbird_app_update_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_app_update_service_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

add_test(NAME app-update-service COMMAND songbird_app_update_service_tests)
set_tests_properties(app-update-service PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_app_update_service_tests)
endif()



set(SONGBIRD_CORE_UPDATE_SERVICE_TEST_SOURCES
    CoreUpdateServiceTests.cpp
    ../src/services/CoreUpdateService.h
    ../src/services/CoreUpdateService.cpp
    ../src/runtime/DnsConfigFragments.h
    ../src/runtime/DnsConfigFragments.cpp
    ../src/runtime/ProtocolConfigMapper.h
    ../src/runtime/ProtocolConfigMapper.cpp
    ../src/runtime/RoutingConfigFragments.h
    ../src/runtime/RoutingConfigFragments.cpp
    ../src/runtime/RoutingRuleJsonMapper.h
    ../src/runtime/RoutingRuleJsonMapper.cpp
    ../src/runtime/core/ICoreBackend.h
    ../src/runtime/core/CoreCatalog.h
    ../src/runtime/core/CoreBackendRegistry.h
    ../src/runtime/core/CoreBackendRegistry.cpp
    ../src/runtime/core/singbox/SingBoxConfigFragments.h
    ../src/runtime/core/singbox/SingBoxConfigFragments.cpp
    ../src/runtime/core/singbox/SingBoxCoreBackend.h
    ../src/runtime/core/singbox/SingBoxCoreBackend.cpp
    ../src/runtime/core/xray/XrayConfigFragments.h
    ../src/runtime/core/xray/XrayConfigFragments.cpp
    ../src/runtime/core/xray/XrayCoreBackend.h
    ../src/runtime/core/xray/XrayCoreBackend.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_core_update_service_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_CORE_UPDATE_SERVICE_TEST_SOURCES}
    )
else()
    add_executable(songbird_core_update_service_tests ${SONGBIRD_CORE_UPDATE_SERVICE_TEST_SOURCES})
endif()

target_include_directories(songbird_core_update_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_core_update_service_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

add_test(NAME core-update-service COMMAND songbird_core_update_service_tests)
set_tests_properties(core-update-service PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_core_update_service_tests)
endif()

set(SONGBIRD_GEO_RESOURCE_UPDATE_SERVICE_TEST_SOURCES
    GeoResourceUpdateServiceTests.cpp
    ../src/services/GeoResourceUpdateService.h
    ../src/services/GeoResourceUpdateService.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_geo_resource_update_service_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_GEO_RESOURCE_UPDATE_SERVICE_TEST_SOURCES}
    )
else()
    add_executable(songbird_geo_resource_update_service_tests ${SONGBIRD_GEO_RESOURCE_UPDATE_SERVICE_TEST_SOURCES})
endif()

target_include_directories(songbird_geo_resource_update_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_geo_resource_update_service_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

add_test(NAME geo-resource-update-service COMMAND songbird_geo_resource_update_service_tests)
set_tests_properties(geo-resource-update-service PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_geo_resource_update_service_tests)
endif()

set(SONGBIRD_SUBSCRIPTION_PARSER_TEST_SOURCES
    SubscriptionParserTests.cpp
    ../src/subscription/SubscriptionContentParser.h
    ../src/subscription/SubscriptionContentParser.cpp
    ../src/subscription/CustomConfigTextParser.h
    ../src/subscription/CustomConfigTextParser.cpp
    ../src/subscription/ShareUrlParser.h
    ../src/subscription/ShareUrlParser.cpp
    ../src/subscription/SubscriptionImportTextParser.h
    ../src/subscription/SubscriptionImportTextParser.cpp
    ../src/common/PortValidator.h
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_subscription_parser_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_SUBSCRIPTION_PARSER_TEST_SOURCES}
    )
else()
    add_executable(songbird_subscription_parser_tests ${SONGBIRD_SUBSCRIPTION_PARSER_TEST_SOURCES})
endif()

target_include_directories(songbird_subscription_parser_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_subscription_parser_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

add_test(NAME subscription-parser COMMAND songbird_subscription_parser_tests)
set_tests_properties(subscription-parser PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_subscription_parser_tests)
endif()

set(SONGBIRD_SERVER_SERVICE_TEST_SOURCES
    ServerServiceTests.cpp
    ../src/services/ServerService.h
    ../src/services/ServerService.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_server_service_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_SERVER_SERVICE_TEST_SOURCES}
    )
else()
    add_executable(songbird_server_service_tests ${SONGBIRD_SERVER_SERVICE_TEST_SOURCES})
endif()

target_include_directories(songbird_server_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_server_service_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME server-service COMMAND songbird_server_service_tests)
set_tests_properties(server-service PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_server_service_tests)
endif()

set(SONGBIRD_PROTOCOL_CORE_COMPAT_TEST_SOURCES
    ProtocolCoreCompatTests.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_protocol_core_compat_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_PROTOCOL_CORE_COMPAT_TEST_SOURCES}
    )
else()
    add_executable(songbird_protocol_core_compat_tests ${SONGBIRD_PROTOCOL_CORE_COMPAT_TEST_SOURCES})
endif()

target_include_directories(songbird_protocol_core_compat_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_protocol_core_compat_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME protocol-core-compat COMMAND songbird_protocol_core_compat_tests)
set_tests_properties(protocol-core-compat PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_protocol_core_compat_tests)
endif()

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
    ../src/runtime/ProtocolConfigMapper.h
    ../src/runtime/ProtocolConfigMapper.cpp
    ../src/runtime/RoutingConfigFragments.h
    ../src/runtime/RoutingConfigFragments.cpp
    ../src/runtime/RoutingRuleJsonMapper.h
    ../src/runtime/RoutingRuleJsonMapper.cpp
    ../src/runtime/core/ICoreBackend.h
    ../src/runtime/core/CoreCatalog.h
    ../src/runtime/core/CoreBackendRegistry.h
    ../src/runtime/core/CoreBackendRegistry.cpp
    ../src/runtime/core/singbox/SingBoxConfigFragments.h
    ../src/runtime/core/singbox/SingBoxConfigFragments.cpp
    ../src/runtime/core/singbox/SingBoxCoreBackend.h
    ../src/runtime/core/singbox/SingBoxCoreBackend.cpp
    ../src/runtime/core/xray/XrayConfigFragments.h
    ../src/runtime/core/xray/XrayConfigFragments.cpp
    ../src/runtime/core/xray/XrayCoreBackend.h
    ../src/runtime/core/xray/XrayCoreBackend.cpp
    ../src/runtime/CoreLifecycleService.h
    ../src/runtime/CoreLifecycleService.cpp
    ../src/runtime/ICoreProcessHost.h
    ../src/runtime/ProtocolCoreCompat.h
    ../src/runtime/QtCoreProcessHost.h
    ../src/runtime/QtCoreProcessHost.cpp
    ../src/runtime/TunCompatCoreRequirement.h
    ../src/services/CoreUpdateService.h
    ../src/services/CoreUpdateService.cpp
    ../src/services/GeoResourceUpdateService.h
    ../src/services/GeoResourceUpdateService.cpp
    ../src/services/ProxyAvailabilityCheckService.h
    ../src/services/ProxyAvailabilityCheckService.cpp
    ../src/services/SpeedTestServiceInternal.h
    ../src/services/SubscriptionService.h
    ../src/services/SubscriptionService.cpp
    ../src/services/SubscriptionUpdateService.h
    ../src/services/SubscriptionUpdateService.cpp
    ../src/subscription/CustomConfigTextParser.h
    ../src/subscription/CustomConfigTextParser.cpp
    ../src/subscription/ShareUrlParser.h
    ../src/subscription/ShareUrlParser.cpp
    ../src/subscription/SubscriptionContentParser.h
    ../src/subscription/SubscriptionContentParser.cpp
    ../src/subscription/SubscriptionImportTextParser.h
    ../src/subscription/SubscriptionImportTextParser.cpp
    ../src/ui/dialogs/SubscriptionSettingsPageWidget.h
    ../src/ui/dialogs/SubscriptionSettingsPageWidget.cpp
    ../src/ui/theme/AppTheme.h
    ../src/ui/theme/AppTheme.cpp
    ../src/resources/themes.qrc
)

if(WIN32)
    list(APPEND SONGBIRD_END_TO_END_SMOKE_TEST_SOURCES
        ../src/platform/windows/WindowsSystemProxyService.h
        ../src/platform/windows/WindowsSystemProxyService.cpp
    )
endif()

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(songbird_end_to_end_smoke_tests
        MANUAL_FINALIZATION
        ${SONGBIRD_END_TO_END_SMOKE_TEST_SOURCES}
    )
else()
    add_executable(songbird_end_to_end_smoke_tests ${SONGBIRD_END_TO_END_SMOKE_TEST_SOURCES})
endif()

target_include_directories(songbird_end_to_end_smoke_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(songbird_end_to_end_smoke_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Svg
)

if(WIN32)
    target_link_libraries(songbird_end_to_end_smoke_tests
        PRIVATE
            wininet
    )
endif()

add_test(NAME end-to-end-smoke COMMAND songbird_end_to_end_smoke_tests)
set_tests_properties(end-to-end-smoke PROPERTIES
    LABELS "smoke"
    TIMEOUT 7200
    ENVIRONMENT "QT_QPA_PLATFORM=windows;QTEST_FUNCTION_TIMEOUT=7200000"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(songbird_end_to_end_smoke_tests)
endif()
