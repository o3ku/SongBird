find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test Widgets)

set(QT_V2RAYN_SHARE_URL_TEST_SOURCES
    ShareUrlTests.cpp
    ../src/subscription/ShareUrlBuilder.h
    ../src/subscription/ShareUrlBuilder.cpp
    ../src/subscription/ShareUrlParser.h
    ../src/subscription/ShareUrlParser.cpp
    ../src/subscription/SubscriptionImportTextParser.h
    ../src/subscription/SubscriptionImportTextParser.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_share_url_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_SHARE_URL_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_share_url_tests ${QT_V2RAYN_SHARE_URL_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_share_url_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_share_url_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

add_test(NAME share-url-transports COMMAND qt_v2rayn_share_url_tests)
set_tests_properties(share-url-transports PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_share_url_tests)
endif()

set(QT_V2RAYN_CLASH_CONFIG_WRITER_TEST_SOURCES
    ClashConfigWriterTests.cpp
    ../src/runtime/ClashConfigWriter.h
    ../src/runtime/ClashConfigWriter.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_clash_config_writer_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_CLASH_CONFIG_WRITER_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_clash_config_writer_tests ${QT_V2RAYN_CLASH_CONFIG_WRITER_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_clash_config_writer_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_clash_config_writer_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

add_test(NAME clash-config-writer COMMAND qt_v2rayn_clash_config_writer_tests)
set_tests_properties(clash-config-writer PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_clash_config_writer_tests)
endif()

set(QT_V2RAYN_ADD_SERVER_DIALOG_TEST_SOURCES
    AddServerDialogTests.cpp
    ../src/ui/dialogs/AddServerDialog.h
    ../src/ui/dialogs/AddServerDialog.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_add_server_dialog_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_ADD_SERVER_DIALOG_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_add_server_dialog_tests ${QT_V2RAYN_ADD_SERVER_DIALOG_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_add_server_dialog_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_add_server_dialog_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME add-server-dialog-roundtrip COMMAND qt_v2rayn_add_server_dialog_tests)
set_tests_properties(add-server-dialog-roundtrip PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_add_server_dialog_tests)
endif()

set(QT_V2RAYN_SETTINGS_DIALOG_TEST_SOURCES
    SettingsDialogTests.cpp
    ../src/ui/dialogs/SettingsDialog.h
    ../src/ui/dialogs/SettingsDialog.cpp
    ../src/ui/theme/AppTheme.h
    ../src/ui/theme/AppTheme.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_settings_dialog_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_SETTINGS_DIALOG_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_settings_dialog_tests ${QT_V2RAYN_SETTINGS_DIALOG_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_settings_dialog_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_settings_dialog_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME settings-dialog-download COMMAND qt_v2rayn_settings_dialog_tests)
set_tests_properties(settings-dialog-download PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_settings_dialog_tests)
endif()

set(QT_V2RAYN_TUN_SETTINGS_APPLY_DECISION_TEST_SOURCES
    TunSettingsApplyDecisionTests.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_tun_settings_apply_decision_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_TUN_SETTINGS_APPLY_DECISION_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_tun_settings_apply_decision_tests ${QT_V2RAYN_TUN_SETTINGS_APPLY_DECISION_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_tun_settings_apply_decision_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_tun_settings_apply_decision_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME tun-settings-apply-decision COMMAND qt_v2rayn_tun_settings_apply_decision_tests)
set_tests_properties(tun-settings-apply-decision PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_tun_settings_apply_decision_tests)
endif()

set(QT_V2RAYN_STARTUP_ADMIN_ELEVATION_TEST_SOURCES
    StartupAdminElevationTests.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_startup_admin_elevation_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_STARTUP_ADMIN_ELEVATION_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_startup_admin_elevation_tests ${QT_V2RAYN_STARTUP_ADMIN_ELEVATION_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_startup_admin_elevation_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_startup_admin_elevation_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME startup-admin-elevation COMMAND qt_v2rayn_startup_admin_elevation_tests)
set_tests_properties(startup-admin-elevation PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_startup_admin_elevation_tests)
endif()

set(QT_V2RAYN_APP_BOOTSTRAP_TUN_RUNTIME_TEST_SOURCES
    AppBootstrapTunRuntimeTests.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_app_bootstrap_tun_runtime_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_APP_BOOTSTRAP_TUN_RUNTIME_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_app_bootstrap_tun_runtime_tests ${QT_V2RAYN_APP_BOOTSTRAP_TUN_RUNTIME_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_app_bootstrap_tun_runtime_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_app_bootstrap_tun_runtime_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME app-bootstrap-tun-runtime COMMAND qt_v2rayn_app_bootstrap_tun_runtime_tests)
set_tests_properties(app-bootstrap-tun-runtime PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_app_bootstrap_tun_runtime_tests)
endif()

set(QT_V2RAYN_GLOBAL_HOTKEY_DIALOG_TEST_SOURCES
    GlobalHotkeyDialogTests.cpp
    ../src/platform/windows/WindowsGlobalHotkeyKeyInterop.h
    ../src/platform/windows/WindowsGlobalHotkeyKeyInterop.cpp
    ../src/ui/dialogs/GlobalHotkeyDialog.h
    ../src/ui/dialogs/GlobalHotkeyDialog.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_global_hotkey_dialog_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_GLOBAL_HOTKEY_DIALOG_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_global_hotkey_dialog_tests ${QT_V2RAYN_GLOBAL_HOTKEY_DIALOG_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_global_hotkey_dialog_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_global_hotkey_dialog_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME global-hotkey-dialog-roundtrip COMMAND qt_v2rayn_global_hotkey_dialog_tests)
set_tests_properties(global-hotkey-dialog-roundtrip PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_global_hotkey_dialog_tests)
endif()

set(QT_V2RAYN_WINDOWS_GLOBAL_HOTKEY_BINDING_TEST_SOURCES
    WindowsGlobalHotkeyBindingTests.cpp
    ../src/platform/windows/WindowsGlobalHotkeyBinding.h
    ../src/platform/windows/WindowsGlobalHotkeyBinding.cpp
    ../src/platform/windows/WindowsGlobalHotkeyKeyInterop.h
    ../src/platform/windows/WindowsGlobalHotkeyKeyInterop.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_windows_global_hotkey_binding_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_WINDOWS_GLOBAL_HOTKEY_BINDING_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_windows_global_hotkey_binding_tests ${QT_V2RAYN_WINDOWS_GLOBAL_HOTKEY_BINDING_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_windows_global_hotkey_binding_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_windows_global_hotkey_binding_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME windows-global-hotkey-binding-mapping COMMAND qt_v2rayn_windows_global_hotkey_binding_tests)
set_tests_properties(windows-global-hotkey-binding-mapping PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_windows_global_hotkey_binding_tests)
endif()

set(QT_V2RAYN_WINDOWS_GLOBAL_HOTKEY_SERVICE_TEST_SOURCES
    WindowsGlobalHotkeyServiceTests.cpp
    ../src/platform/windows/WindowsGlobalHotkeyService.h
    ../src/platform/windows/WindowsGlobalHotkeyService.cpp
    ../src/platform/windows/WindowsGlobalHotkeyBinding.h
    ../src/platform/windows/WindowsGlobalHotkeyBinding.cpp
    ../src/platform/windows/WindowsGlobalHotkeyKeyInterop.h
    ../src/platform/windows/WindowsGlobalHotkeyKeyInterop.cpp
    ../src/ui/dialogs/GlobalHotkeyDialog.h
    ../src/ui/dialogs/GlobalHotkeyDialog.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_windows_global_hotkey_service_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_WINDOWS_GLOBAL_HOTKEY_SERVICE_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_windows_global_hotkey_service_tests ${QT_V2RAYN_WINDOWS_GLOBAL_HOTKEY_SERVICE_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_windows_global_hotkey_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_windows_global_hotkey_service_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME windows-global-hotkey-service-pause COMMAND qt_v2rayn_windows_global_hotkey_service_tests)
set_tests_properties(windows-global-hotkey-service-pause PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_windows_global_hotkey_service_tests)
endif()

set(QT_V2RAYN_MAIN_WINDOW_TEST_SOURCES
    MainWindowTests.cpp
    ../src/subscription/ShareUrlBuilder.h
    ../src/subscription/ShareUrlBuilder.cpp
    ../src/third_party/qrcodegen/qrcodegen.hpp
    ../src/third_party/qrcodegen/qrcodegen.cpp
    ../src/ui/mainwindow/MainWindow.h
    ../src/ui/mainwindow/MainWindow.cpp
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
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_main_window_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_MAIN_WINDOW_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_main_window_tests ${QT_V2RAYN_MAIN_WINDOW_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_main_window_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_main_window_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

add_test(NAME main-window-log-scroll COMMAND qt_v2rayn_main_window_tests)
set_tests_properties(main-window-log-scroll PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_main_window_tests)
endif()

set(QT_V2RAYN_CLIENT_CONFIG_WRITER_TEST_SOURCES
    ClientConfigWriterTests.cpp
    ../src/runtime/ClientConfigWriter.h
    ../src/runtime/ClientConfigWriter.cpp
    ../src/runtime/ClashConfigWriter.h
    ../src/runtime/ClashConfigWriter.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_client_config_writer_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_CLIENT_CONFIG_WRITER_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_client_config_writer_tests ${QT_V2RAYN_CLIENT_CONFIG_WRITER_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_client_config_writer_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_client_config_writer_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME client-config-writer-tun-compat COMMAND qt_v2rayn_client_config_writer_tests)
set_tests_properties(client-config-writer-tun-compat PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_client_config_writer_tests)
endif()

set(QT_V2RAYN_TUN_COMPAT_CORE_REQUIREMENT_TEST_SOURCES
    TunCompatCoreRequirementTests.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_tun_compat_core_requirement_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_TUN_COMPAT_CORE_REQUIREMENT_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_tun_compat_core_requirement_tests ${QT_V2RAYN_TUN_COMPAT_CORE_REQUIREMENT_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_tun_compat_core_requirement_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_tun_compat_core_requirement_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME tun-compat-core-requirement COMMAND qt_v2rayn_tun_compat_core_requirement_tests)
set_tests_properties(tun-compat-core-requirement PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_tun_compat_core_requirement_tests)
endif()

set(QT_V2RAYN_JSON_CONFIG_REPOSITORY_TEST_SOURCES
    JsonConfigRepositoryTests.cpp
    ../src/persistence/JsonConfigRepository.h
    ../src/persistence/JsonConfigRepository.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_json_config_repository_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_JSON_CONFIG_REPOSITORY_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_json_config_repository_tests ${QT_V2RAYN_JSON_CONFIG_REPOSITORY_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_json_config_repository_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_json_config_repository_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME json-config-repository-defaults COMMAND qt_v2rayn_json_config_repository_tests)
set_tests_properties(json-config-repository-defaults PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_json_config_repository_tests)
endif()

set(QT_V2RAYN_CONFIG_FILE_IMPORT_PARSER_TEST_SOURCES
    ConfigFileImportParserTests.cpp
    ../src/subscription/ConfigFileImportParser.h
    ../src/subscription/ConfigFileImportParser.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_config_file_import_parser_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_CONFIG_FILE_IMPORT_PARSER_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_config_file_import_parser_tests ${QT_V2RAYN_CONFIG_FILE_IMPORT_PARSER_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_config_file_import_parser_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_config_file_import_parser_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME config-file-import-parser-stream-settings COMMAND qt_v2rayn_config_file_import_parser_tests)
set_tests_properties(config-file-import-parser-stream-settings PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_config_file_import_parser_tests)
endif()

set(QT_V2RAYN_SERVER_CONFIG_WRITER_TEST_SOURCES
    ServerConfigWriterTests.cpp
    ../src/runtime/ServerConfigWriter.h
    ../src/runtime/ServerConfigWriter.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_server_config_writer_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_SERVER_CONFIG_WRITER_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_server_config_writer_tests ${QT_V2RAYN_SERVER_CONFIG_WRITER_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_server_config_writer_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_server_config_writer_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME server-config-writer COMMAND qt_v2rayn_server_config_writer_tests)
set_tests_properties(server-config-writer PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_server_config_writer_tests)
endif()

set(QT_V2RAYN_PROXY_AVAILABILITY_CHECK_TEST_SOURCES
    ProxyAvailabilityCheckServiceTests.cpp
    ../src/services/ProxyAvailabilityCheckService.h
    ../src/services/ProxyAvailabilityCheckService.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_proxy_availability_check_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_PROXY_AVAILABILITY_CHECK_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_proxy_availability_check_tests ${QT_V2RAYN_PROXY_AVAILABILITY_CHECK_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_proxy_availability_check_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_proxy_availability_check_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

add_test(NAME proxy-availability-check COMMAND qt_v2rayn_proxy_availability_check_tests)
set_tests_properties(proxy-availability-check PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_proxy_availability_check_tests)
endif()

set(QT_V2RAYN_SPEED_TEST_SERVICE_INTERNAL_TEST_SOURCES
    SpeedTestServiceInternalTests.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_speed_test_service_internal_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_SPEED_TEST_SERVICE_INTERNAL_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_speed_test_service_internal_tests ${QT_V2RAYN_SPEED_TEST_SERVICE_INTERNAL_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_speed_test_service_internal_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_speed_test_service_internal_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

add_test(NAME speed-test-service-internal COMMAND qt_v2rayn_speed_test_service_internal_tests)
set_tests_properties(speed-test-service-internal PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_speed_test_service_internal_tests)
endif()

set(QT_V2RAYN_SUBSCRIPTION_SERVICE_TEST_SOURCES
    SubscriptionServiceTests.cpp
    ../src/services/SubscriptionService.h
    ../src/services/SubscriptionService.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_subscription_service_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_SUBSCRIPTION_SERVICE_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_subscription_service_tests ${QT_V2RAYN_SUBSCRIPTION_SERVICE_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_subscription_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_subscription_service_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME subscription-service COMMAND qt_v2rayn_subscription_service_tests)
set_tests_properties(subscription-service PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_subscription_service_tests)
endif()

set(QT_V2RAYN_ROUTING_SERVICE_TEST_SOURCES
    RoutingServiceTests.cpp
    ../src/services/RoutingService.h
    ../src/services/RoutingService.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_routing_service_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_ROUTING_SERVICE_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_routing_service_tests ${QT_V2RAYN_ROUTING_SERVICE_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_routing_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_routing_service_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME routing-service COMMAND qt_v2rayn_routing_service_tests)
set_tests_properties(routing-service PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_routing_service_tests)
endif()

set(QT_V2RAYN_STATISTICS_SERVICE_TEST_SOURCES
    StatisticsServiceTests.cpp
    ../src/services/StatisticsService.h
    ../src/services/StatisticsService.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_statistics_service_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_STATISTICS_SERVICE_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_statistics_service_tests ${QT_V2RAYN_STATISTICS_SERVICE_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_statistics_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_statistics_service_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME statistics-service COMMAND qt_v2rayn_statistics_service_tests)
set_tests_properties(statistics-service PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_statistics_service_tests)
endif()

set(QT_V2RAYN_CORE_UPDATE_SERVICE_TEST_SOURCES
    CoreUpdateServiceTests.cpp
    ../src/services/CoreUpdateService.h
    ../src/services/CoreUpdateService.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_core_update_service_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_CORE_UPDATE_SERVICE_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_core_update_service_tests ${QT_V2RAYN_CORE_UPDATE_SERVICE_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_core_update_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_core_update_service_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

add_test(NAME core-update-service COMMAND qt_v2rayn_core_update_service_tests)
set_tests_properties(core-update-service PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_core_update_service_tests)
endif()

set(QT_V2RAYN_SUBSCRIPTION_PARSER_TEST_SOURCES
    SubscriptionParserTests.cpp
    ../src/subscription/SubscriptionContentParser.h
    ../src/subscription/SubscriptionContentParser.cpp
    ../src/subscription/CustomConfigTextParser.h
    ../src/subscription/CustomConfigTextParser.cpp
    ../src/subscription/ShareUrlParser.h
    ../src/subscription/ShareUrlParser.cpp
    ../src/subscription/SubscriptionImportTextParser.h
    ../src/subscription/SubscriptionImportTextParser.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_subscription_parser_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_SUBSCRIPTION_PARSER_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_subscription_parser_tests ${QT_V2RAYN_SUBSCRIPTION_PARSER_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_subscription_parser_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_subscription_parser_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

add_test(NAME subscription-parser COMMAND qt_v2rayn_subscription_parser_tests)
set_tests_properties(subscription-parser PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_subscription_parser_tests)
endif()

set(QT_V2RAYN_SERVER_SERVICE_TEST_SOURCES
    ServerServiceTests.cpp
    ../src/services/ServerService.h
    ../src/services/ServerService.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_server_service_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_SERVER_SERVICE_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_server_service_tests ${QT_V2RAYN_SERVER_SERVICE_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_server_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_server_service_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME server-service COMMAND qt_v2rayn_server_service_tests)
set_tests_properties(server-service PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_server_service_tests)
endif()

set(QT_V2RAYN_PROTOCOL_CORE_COMPAT_TEST_SOURCES
    ProtocolCoreCompatTests.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn_protocol_core_compat_tests
        MANUAL_FINALIZATION
        ${QT_V2RAYN_PROTOCOL_CORE_COMPAT_TEST_SOURCES}
    )
else()
    add_executable(qt_v2rayn_protocol_core_compat_tests ${QT_V2RAYN_PROTOCOL_CORE_COMPAT_TEST_SOURCES})
endif()

target_include_directories(qt_v2rayn_protocol_core_compat_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(qt_v2rayn_protocol_core_compat_tests
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME protocol-core-compat COMMAND qt_v2rayn_protocol_core_compat_tests)
set_tests_properties(protocol-core-compat PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>"
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn_protocol_core_compat_tests)
endif()
