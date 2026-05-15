set(QT_V2RAYN_SOURCES
    app/main.cpp
    app/AppBootstrap.h
    app/AppBootstrap.cpp
    app/SingleInstanceBootstrap.h
    app/SingleInstanceBootstrap.cpp
    app/StartupAdminElevation.h
    common/OperationResult.h
    common/DataSizeFormatter.h
    common/GitHubMirrorHelper.h
    common/SystemProxyMode.h
    domain/models/Config.h
    domain/models/PolicyGroupItem.h
    domain/models/RoutingItem.h
    domain/models/RoutingRule.h
    domain/models/ServerStatistics.h
    domain/models/StatisticsSessionState.h
    domain/models/SubItem.h
    domain/models/VmessItem.h
    platform/windows/SingleInstanceGuard.h
    platform/windows/SingleInstanceGuard.cpp
    platform/windows/WindowsAutoRunService.h
    platform/windows/WindowsAutoRunService.cpp
    platform/windows/WindowsGlobalHotkeyBinding.h
    platform/windows/WindowsGlobalHotkeyBinding.cpp
    platform/windows/WindowsGlobalHotkeyKeyInterop.h
    platform/windows/WindowsGlobalHotkeyKeyInterop.cpp
    platform/windows/WindowsGlobalHotkeyService.h
    platform/windows/WindowsGlobalHotkeyService.cpp
    platform/windows/WindowsSystemProxyService.h
    platform/windows/WindowsSystemProxyService.cpp
    platform/windows/WindowsPacServer.h
    platform/windows/WindowsPacServer.cpp
    runtime/CoreInfo.h
    runtime/ProtocolCoreCompat.h
    runtime/ClientConfigWriter.h
    runtime/ClientConfigWriter.cpp
    runtime/ClashConfigWriter.h
    runtime/ClashConfigWriter.cpp
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
    services/GrpcStatisticsBackend.h
    services/GrpcStatisticsBackend.cpp
    services/ClashRestStatisticsBackend.h
    services/ClashRestStatisticsBackend.cpp
    services/GrpcStatisticsParser.h
    services/GrpcStatisticsParser.cpp
    services/StatisticsService.h
    services/StatisticsService.cpp
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
    ui/dialogs/DnsSettingsDialog.h
    ui/dialogs/DnsSettingsDialog.cpp
    ui/dialogs/GlobalHotkeyDialog.h
    ui/dialogs/GlobalHotkeyDialog.cpp
    ui/dialogs/SettingsDialog.h
    ui/dialogs/SettingsDialog.cpp
    ui/theme/AppTheme.h
    ui/theme/AppTheme.cpp
    ui/mainwindow/MainWindow.h
    ui/mainwindow/MainWindow.cpp
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
    ui/tray/TrayController.h
    ui/tray/TrayController.cpp
)

if(WIN32)
    list(APPEND QT_V2RAYN_SOURCES
        resources/v2rayq.rc
    )
endif()

set(QT_V2RAYN_TRANSLATION_TS_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../translations/v2rayq_zh_CN.ts
)

set(QT_V2RAYN_QT_TRANSLATION_QM_FILES)
if(TARGET Qt${QT_VERSION_MAJOR}::qmake)
    get_target_property(QT_V2RAYN_QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
endif()
if(QT_V2RAYN_QMAKE_EXECUTABLE)
    execute_process(
        COMMAND "${QT_V2RAYN_QMAKE_EXECUTABLE}" -query QT_INSTALL_TRANSLATIONS
        OUTPUT_VARIABLE QT_V2RAYN_QT_TRANSLATIONS_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()
if(NOT QT_V2RAYN_QT_TRANSLATIONS_DIR AND QT_VERSION_MAJOR EQUAL 5)
    message(WARNING "Qt translations directory not found. Set QT_V2RAYN_QT_TRANSLATIONS_DIR manually or ensure qmake is discoverable.")
endif()

set(QT_V2RAYN_QT_ZH_QM
    "${QT_V2RAYN_QT_TRANSLATIONS_DIR}/qt_zh_CN.qm"
)
if(EXISTS "${QT_V2RAYN_QT_ZH_QM}")
    list(APPEND QT_V2RAYN_QT_TRANSLATION_QM_FILES "${QT_V2RAYN_QT_ZH_QM}")
endif()

set(QT_V2RAYN_QTBASE_ZH_QM
    "${QT_V2RAYN_QT_TRANSLATIONS_DIR}/qtbase_zh_CN.qm"
)
if(EXISTS "${QT_V2RAYN_QTBASE_ZH_QM}")
    list(APPEND QT_V2RAYN_QT_TRANSLATION_QM_FILES "${QT_V2RAYN_QTBASE_ZH_QM}")
endif()

set(QT_V2RAYN_TRANSLATION_QM_FILES)
if(QT_V2RAYN_HAS_LINGUISTTOOLS)
    if(QT_VERSION_MAJOR GREATER_EQUAL 6)
        qt_add_translation(QT_V2RAYN_TRANSLATION_QM_FILES ${QT_V2RAYN_TRANSLATION_TS_FILES})
    else()
        qt5_add_translation(QT_V2RAYN_TRANSLATION_QM_FILES ${QT_V2RAYN_TRANSLATION_TS_FILES})
    endif()

    add_custom_target(qt_v2rayn_translations ALL DEPENDS ${QT_V2RAYN_TRANSLATION_QM_FILES})

    # Generate a .qrc that embeds compiled .qm files into the executable
    set(QT_V2RAYN_TRANSLATIONS_QRC "${CMAKE_CURRENT_BINARY_DIR}/translations.qrc")
    file(WRITE "${QT_V2RAYN_TRANSLATIONS_QRC}" "<RCC>\n    <qresource prefix=\"/translations\">\n")
    foreach(qm_file ${QT_V2RAYN_TRANSLATION_QM_FILES})
        get_filename_component(qm_name "${qm_file}" NAME)
        file(APPEND "${QT_V2RAYN_TRANSLATIONS_QRC}" "        <file alias=\"${qm_name}\">${qm_file}</file>\n")
    endforeach()
    foreach(qm_file ${QT_V2RAYN_QT_TRANSLATION_QM_FILES})
        get_filename_component(qm_name "${qm_file}" NAME)
        file(APPEND "${QT_V2RAYN_TRANSLATIONS_QRC}" "        <file alias=\"${qm_name}\">${qm_file}</file>\n")
    endforeach()
    file(APPEND "${QT_V2RAYN_TRANSLATIONS_QRC}" "    </qresource>\n</RCC>\n")

    # Ensure .qm files exist before rcc processes the .qrc
    set_source_files_properties("${QT_V2RAYN_TRANSLATIONS_QRC}" PROPERTIES
        OBJECT_DEPENDS "${QT_V2RAYN_TRANSLATION_QM_FILES}"
    )

    list(APPEND QT_V2RAYN_SOURCES "${QT_V2RAYN_TRANSLATIONS_QRC}")
endif()

if(QT_V2RAYN_HAS_GRPC)
    set(QT_V2RAYN_STATISTICS_PROTO
        ${CMAKE_CURRENT_SOURCE_DIR}/proto/Statistics.proto
    )
    set(QT_V2RAYN_STATISTICS_PROTO_BINARY_DIR
        ${CMAKE_CURRENT_BINARY_DIR}/generated/proto
    )

    protobuf_generate(
        LANGUAGE cpp
        OUT_VAR QT_V2RAYN_STATISTICS_PROTO_SOURCES
        PROTOS ${QT_V2RAYN_STATISTICS_PROTO}
        IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto
        PROTOC_OUT_DIR ${QT_V2RAYN_STATISTICS_PROTO_BINARY_DIR}
    )

    protobuf_generate(
        LANGUAGE grpc
        OUT_VAR QT_V2RAYN_STATISTICS_GRPC_SOURCES
        GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
        PROTOS ${QT_V2RAYN_STATISTICS_PROTO}
        IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto
        PROTOC_OUT_DIR ${QT_V2RAYN_STATISTICS_PROTO_BINARY_DIR}
        PLUGIN "protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
        DEPENDENCIES gRPC::grpc_cpp_plugin
    )

    add_library(qt_v2rayn_statistics_proto STATIC
        ${QT_V2RAYN_STATISTICS_PROTO_SOURCES}
        ${QT_V2RAYN_STATISTICS_GRPC_SOURCES}
    )

    target_include_directories(qt_v2rayn_statistics_proto
        PUBLIC
            ${QT_V2RAYN_STATISTICS_PROTO_BINARY_DIR}
    )

    target_link_libraries(qt_v2rayn_statistics_proto
        PUBLIC
            protobuf::libprotobuf
            gRPC::grpc++
    )
endif()

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(qt_v2rayn
        MANUAL_FINALIZATION
        WIN32
        ${QT_V2RAYN_SOURCES}
    )
else()
    add_executable(qt_v2rayn WIN32 ${QT_V2RAYN_SOURCES})
endif()

target_include_directories(qt_v2rayn
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(qt_v2rayn
    PRIVATE
        QT_V2RAYN_HAS_GRPC_STATISTICS=$<BOOL:${QT_V2RAYN_HAS_GRPC}>
        QT_V2RAYN_APP_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(qt_v2rayn
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

if(QT_V2RAYN_HAS_GRPC)
    target_link_libraries(qt_v2rayn
        PRIVATE
            qt_v2rayn_statistics_proto
    )
endif()

set_target_properties(qt_v2rayn PROPERTIES
    OUTPUT_NAME "v2rayq"
)

if(TARGET qt_v2rayn_translations)
    add_dependencies(qt_v2rayn qt_v2rayn_translations)
endif()

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(qt_v2rayn)
endif()

if(WIN32)
    target_link_libraries(qt_v2rayn PRIVATE Advapi32 Shell32 Wininet User32 Iphlpapi)
endif()

if(WIN32 AND MSVC)
    # The Windows manifest is embedded via resources/v2rayq.rc (RT_MANIFEST).
    # Disable the linker's auto-generated manifest to avoid LNK4078 conflicts
    # and to keep dpiAwareness=PerMonitorV2 authoritative.
    target_link_options(qt_v2rayn PRIVATE /MANIFEST:NO)
endif()

install(TARGETS qt_v2rayn
    RUNTIME DESTINATION .
)

if(QT_V2RAYN_TRANSLATION_QM_FILES)
    install(FILES ${QT_V2RAYN_TRANSLATION_QM_FILES}
        DESTINATION translations
    )
endif()

install(FILES ${QT_V2RAYN_TRANSLATION_TS_FILES}
    DESTINATION translations
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../README.md
    ${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE
    DESTINATION .
)
