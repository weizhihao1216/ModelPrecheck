cmake_minimum_required(VERSION 3.12)
project(ModelValidator VERSION 1.0 LANGUAGES C CXX)

add_compile_definitions(NOMINMAX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force MSVC execution character set UTF-8
if(MSVC)
    add_compile_options(/utf-8)
    add_compile_options(/W3)
endif()

# Qt path: prefer local Qt 5.14.2 (msvc2017_64), fall back to legacy Qt 5.11 path
if(DEFINED ENV{QTDIR} AND EXISTS "$ENV{QTDIR}/lib/cmake/Qt5")
    set(_QT_ROOT "$ENV{QTDIR}")
elseif(EXISTS "D:/Qt/5.14.2/msvc2017_64/lib/cmake/Qt5")
    set(_QT_ROOT "D:/Qt/5.14.2/msvc2017_64")
elseif(EXISTS "D:/HR/DEV/ThirdParty/Qt5.11/vc140/x64/lib/cmake/Qt5")
    set(_QT_ROOT "D:/HR/DEV/ThirdParty/Qt5.11/vc140/x64")
else()
    set(_QT_ROOT "")
endif()

if(_QT_ROOT)
    set(Qt5_DIR "${_QT_ROOT}/lib/cmake/Qt5" CACHE PATH "Qt5 Directory" FORCE)
    list(INSERT CMAKE_PREFIX_PATH 0 "${_QT_ROOT}")
    message(STATUS "Using Qt at: ${_QT_ROOT}")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 COMPONENTS Widgets Charts REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
)

set(SOURCES
    src/main.cpp
    src/utils/SehHelper.h
    src/utils/SehHelper.cpp
    src/utils/MemoryUtils.h
    src/utils/MemoryUtils.cpp
    src/core/PeAnalyzer.h
    src/core/PeAnalyzer.cpp
    src/core/HeaderAnalyzer.h
    src/core/HeaderAnalyzer.cpp
    src/core/LibAnalyzer.h
    src/core/LibAnalyzer.cpp
    src/core/PackageScanner.h
    src/core/PackageScanner.cpp
    src/core/DllLoader.h
    src/core/DllLoader.cpp
    src/core/PerfProfiler.h
    src/core/PerfProfiler.cpp
    src/core/FunctionalVerifier.h
    src/core/FunctionalVerifier.cpp
    src/core/ConcurrencyTester.h
    src/core/ConcurrencyTester.cpp
    src/core/UserCodeHarness.h
    src/core/UserCodeHarness.cpp
    src/core/ReportGenerator.h
    src/core/ReportGenerator.cpp
    src/ui/LogConsoleWidget.h
    src/ui/LogConsoleWidget.cpp
    src/ui/ChartViewerWidget.h
    src/ui/ChartViewerWidget.cpp
    src/ui/TrajectoryViewWidget.h
    src/ui/TrajectoryViewWidget.cpp
    src/ui/CppSyntaxHighlighter.h
    src/ui/CppSyntaxHighlighter.cpp
    src/ui/CppCodeEditor.h
    src/ui/CppCodeEditor.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/resources.qrc
)

add_executable(ModelValidator WIN32 ${SOURCES})

target_link_libraries(ModelValidator PRIVATE
    Qt5::Widgets
    Qt5::Charts
    psapi.lib
)

# Add sample DLL targets for test verification
add_subdirectory(sample_models/StandardWeapon)
add_subdirectory(sample_models/FaultyWeapon)
add_subdirectory(sample_models/MultiInstanceWeapon)
add_subdirectory(sample_models/ConflictWeaponA)
add_subdirectory(sample_models/ConflictWeaponB)
add_subdirectory(sample_models/CompatibleWeaponC)
add_subdirectory(sample_models/CompatibleWeaponD)
add_subdirectory(sample_models/CompatibleWeaponE)

# Install configuration (Default target directory: ${CMAKE_SOURCE_DIR}/dist)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist" CACHE PATH "Install path prefix" FORCE)
endif()

install(TARGETS ModelValidator DESTINATION .)
install(TARGETS StandardWeapon FaultyWeapon MultiInstanceWeapon
                ConflictWeaponA ConflictWeaponB CompatibleWeaponC CompatibleWeaponD CompatibleWeaponE
        DESTINATION sample_models)
install(FILES sample_models/StandardWeapon/WeaponModel.h DESTINATION sample_models)
install(FILES sample_models/MultiInstanceWeapon/WeaponModel.h DESTINATION sample_models RENAME WeaponModel_MultiInstance.h)
install(FILES sample_models/ConflictWeaponA/WeaponModel.h DESTINATION sample_models RENAME WeaponModel_ConflictA.h)
install(FILES sample_models/ConflictWeaponB/WeaponModel.h DESTINATION sample_models RENAME WeaponModel_ConflictB.h)
install(FILES sample_models/CompatibleWeaponC/WeaponModel.h DESTINATION sample_models RENAME WeaponModel_CompatibleC.h)
install(FILES sample_models/CompatibleWeaponD/WeaponModel.h DESTINATION sample_models RENAME WeaponModel_CompatibleD.h)
install(FILES sample_models/CompatibleWeaponE/WeaponModel.h DESTINATION sample_models RENAME WeaponModel_CompatibleE.h)
install(FILES src/ui/themestyle.qss DESTINATION .)

