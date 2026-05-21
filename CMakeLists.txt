cmake_minimum_required(VERSION 3.24)

project(SongBird VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

option(BUILD_TEST "Build test targets." OFF)
set(SONGBIRD_FORCE_MAJOR_VERSION ""
    CACHE STRING "Force a specific Qt major version (5 or 6). Leave empty for auto-detect.")

set(SONGBIRD_QT_PACKAGE_HINT "")
set(SONGBIRD_QT5_DIR_HINT "")
set(SONGBIRD_QT6_DIR_HINT "")

set(QT_DEFAULT_MAJOR_VERSION 6)

if(SONGBIRD_FORCE_MAJOR_VERSION
    AND NOT SONGBIRD_FORCE_MAJOR_VERSION STREQUAL "5"
    AND NOT SONGBIRD_FORCE_MAJOR_VERSION STREQUAL "6")
    message(FATAL_ERROR
        "SONGBIRD_FORCE_MAJOR_VERSION must be empty, 5, or 6. "
        "Current value: ${SONGBIRD_FORCE_MAJOR_VERSION}")
endif()

if(SONGBIRD_FORCE_MAJOR_VERSION STREQUAL "6")
    if(DEFINED Qt6_DIR AND EXISTS "${Qt6_DIR}/Qt6Config.cmake")
        set(SONGBIRD_QT6_DIR_HINT "${Qt6_DIR}")
        set(SONGBIRD_QT_PACKAGE_HINT "${Qt6_DIR}")
    elseif(DEFINED QT_DIR AND EXISTS "${QT_DIR}/Qt6Config.cmake")
        set(SONGBIRD_QT_PACKAGE_HINT "${QT_DIR}")
    endif()
elseif(SONGBIRD_FORCE_MAJOR_VERSION STREQUAL "5")
    if(DEFINED Qt5_DIR AND EXISTS "${Qt5_DIR}/Qt5Config.cmake")
        set(SONGBIRD_QT5_DIR_HINT "${Qt5_DIR}")
        set(SONGBIRD_QT_PACKAGE_HINT "${Qt5_DIR}")
    elseif(DEFINED QT_DIR AND EXISTS "${QT_DIR}/Qt5Config.cmake")
        set(SONGBIRD_QT_PACKAGE_HINT "${QT_DIR}")
    endif()
elseif(DEFINED Qt6_DIR AND EXISTS "${Qt6_DIR}/Qt6Config.cmake")
    set(SONGBIRD_QT6_DIR_HINT "${Qt6_DIR}")
    set(SONGBIRD_QT_PACKAGE_HINT "${Qt6_DIR}")
elseif(DEFINED Qt5_DIR AND EXISTS "${Qt5_DIR}/Qt5Config.cmake")
    set(SONGBIRD_QT5_DIR_HINT "${Qt5_DIR}")
    set(SONGBIRD_QT_PACKAGE_HINT "${Qt5_DIR}")
elseif(DEFINED QT_DIR
    AND (EXISTS "${QT_DIR}/Qt6Config.cmake" OR EXISTS "${QT_DIR}/Qt5Config.cmake"))
    set(SONGBIRD_QT_PACKAGE_HINT "${QT_DIR}")
endif()

foreach(_qt_stale_cache_var IN ITEMS
    Qt5_DIR
    Qt5Core_DIR
    Qt5Gui_DIR
    Qt5Widgets_DIR
    Qt5Network_DIR
    Qt5LinguistTools_DIR
    Qt6_DIR
    Qt6Core_DIR
    Qt6Gui_DIR
    Qt6Widgets_DIR
    Qt6Network_DIR
    Qt6LinguistTools_DIR)
    if(DEFINED ${_qt_stale_cache_var})
        unset(${_qt_stale_cache_var} CACHE)
    endif()
endforeach()

if(SONGBIRD_FORCE_MAJOR_VERSION STREQUAL "6")
    find_package(Qt6 REQUIRED COMPONENTS Widgets Network HINTS
        "${SONGBIRD_QT6_DIR_HINT}" "${SONGBIRD_QT_PACKAGE_HINT}")
    set(QT_VERSION_MAJOR 6)
elseif(SONGBIRD_FORCE_MAJOR_VERSION STREQUAL "5")
    find_package(Qt5 REQUIRED COMPONENTS Widgets Network HINTS
        "${SONGBIRD_QT5_DIR_HINT}" "${SONGBIRD_QT_PACKAGE_HINT}")
    set(QT_VERSION_MAJOR 5)
elseif(SONGBIRD_QT6_DIR_HINT)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Network HINTS "${SONGBIRD_QT6_DIR_HINT}")
    set(QT_VERSION_MAJOR 6)
elseif(SONGBIRD_QT5_DIR_HINT)
    find_package(Qt5 REQUIRED COMPONENTS Widgets Network HINTS "${SONGBIRD_QT5_DIR_HINT}")
    set(QT_VERSION_MAJOR 5)
elseif(DEFINED QT_DIR AND EXISTS "${QT_DIR}/Qt6Config.cmake")
    find_package(Qt6 REQUIRED COMPONENTS Widgets Network HINTS "${QT_DIR}")
    set(QT_VERSION_MAJOR 6)
elseif(DEFINED QT_DIR AND EXISTS "${QT_DIR}/Qt5Config.cmake")
    find_package(Qt5 REQUIRED COMPONENTS Widgets Network HINTS "${QT_DIR}")
    set(QT_VERSION_MAJOR 5)
else()
    find_package(Qt6 QUIET COMPONENTS Widgets Network HINTS "${SONGBIRD_QT_PACKAGE_HINT}")
    if(Qt6_FOUND)
        set(QT_VERSION_MAJOR 6)
    else()
        find_package(Qt5 REQUIRED COMPONENTS Widgets Network HINTS "${SONGBIRD_QT_PACKAGE_HINT}")
        set(QT_VERSION_MAJOR 5)
    endif()
endif()

find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS LinguistTools HINTS "${SONGBIRD_QT_PACKAGE_HINT}")
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Svg)

set(_qt_package_name "Qt${QT_VERSION_MAJOR}")
set(_qt_found_var "${_qt_package_name}_FOUND")
set(_qt_version_var "${_qt_package_name}_VERSION")
set(_qt_dir_var "${_qt_package_name}_DIR")

message(STATUS "Qt search prefix path: ${CMAKE_PREFIX_PATH}")
if(SONGBIRD_FORCE_MAJOR_VERSION)
    message(STATUS "Qt major version forced by preset/cache: ${SONGBIRD_FORCE_MAJOR_VERSION}")
endif()
message(STATUS "Qt package selector result: QT_VERSION_MAJOR=${QT_VERSION_MAJOR}")
if(SONGBIRD_QT_PACKAGE_HINT)
    message(STATUS "Qt package root hint: ${SONGBIRD_QT_PACKAGE_HINT}")
endif()
if(DEFINED ${_qt_found_var})
    message(STATUS "Qt package find result: ${_qt_package_name}_FOUND=${${_qt_found_var}}")
endif()
if(DEFINED ${_qt_version_var})
    message(STATUS "Qt package version: ${${_qt_version_var}}")
endif()
if(DEFINED ${_qt_dir_var})
    message(STATUS "Qt package config dir: ${${_qt_dir_var}}")
endif()

foreach(_qt_component IN ITEMS Core Gui Widgets Network LinguistTools)
    set(_qt_component_package "Qt${QT_VERSION_MAJOR}${_qt_component}")
    set(_qt_component_dir_var "${_qt_component_package}_DIR")
    if(DEFINED ${_qt_component_dir_var})
        message(STATUS "Qt component config dir: ${_qt_component_package}=${${_qt_component_dir_var}}")
    endif()
endforeach()


set(SONGBIRD_HAS_LINGUISTTOOLS OFF)
if(TARGET Qt${QT_VERSION_MAJOR}::lrelease)
    set(SONGBIRD_HAS_LINGUISTTOOLS ON)
endif()

message(STATUS "SongBird translation toolchain: ${SONGBIRD_HAS_LINGUISTTOOLS}")

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_standard_project_setup()
endif()

add_subdirectory(src)

if(BUILD_TEST)
    include(CTest)
    add_subdirectory(tests)
endif()

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "SongBird")
set(CPACK_PACKAGE_VENDOR "SongBird")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)

