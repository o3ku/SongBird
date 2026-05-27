cmake_minimum_required(VERSION 3.24)

project(SongBird VERSION 2.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

option(BUILD_TEST "Build test targets." OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Svg)
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS LinguistTools)

set(SONGBIRD_HAS_LINGUISTTOOLS OFF)
if(TARGET Qt${QT_VERSION_MAJOR}::lrelease)
    set(SONGBIRD_HAS_LINGUISTTOOLS ON)
endif()

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_standard_project_setup()
endif()

add_subdirectory(src)

if(BUILD_TEST)
    include(CTest)
    add_subdirectory(tests)
endif()

