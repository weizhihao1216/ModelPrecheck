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

find_package(Qt5 COMPONENTS
    Widgets
    Charts
    PrintSupport
    Qml
    Quick
    QuickControls2
    QuickWidgets
    REQUIRED
)

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
    src/core/MultiObjectHarness.h
    src/core/MultiObjectHarness.cpp
    src/core/InterfaceMappingProfile.h
    src/core/InterfaceMappingProfile.cpp
    src/core/SingleThreadMultiObjectTester.h
    src/core/SingleThreadMultiObjectTester.cpp
    src/core/FleetSingleThreadMultiObjectTester.h
    src/core/FleetSingleThreadMultiObjectTester.cpp
    src/core/SessionStore.h
    src/core/SessionStore.cpp
    src/core/ReportGenerator.h
    src/core/ReportGenerator.cpp
    src/core/PrecheckSummary.h
    src/core/PrecheckSummary.cpp
    src/ui/CppSyntaxHighlighter.h
    src/ui/CppSyntaxHighlighter.cpp
    src/qml/QmlAppController.h
    src/qml/QmlAppController.cpp
    src/qml/QmlPrecheckRunner.h
    src/qml/QmlPrecheckRunner.cpp
    src/resources.qrc
)

add_executable(ModelValidator WIN32 ${SOURCES})

target_link_libraries(ModelValidator PRIVATE
    Qt5::Widgets
    Qt5::Charts
    Qt5::Qml
    Qt5::Quick
    Qt5::QuickControls2
    psapi.lib
)

# Add sample DLL targets for test verification (only if present)
if(EXISTS "${CMAKE_SOURCE_DIR}/sample_models/CompatibleWeaponF/CMakeLists.txt")
    add_subdirectory(sample_models/CompatibleWeaponF)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/sample_models/CompatibleWeaponG/CMakeLists.txt")
    add_subdirectory(sample_models/CompatibleWeaponG)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/sample_models/testcase_kit/CMakeLists.txt")
    add_subdirectory(sample_models/testcase_kit)
endif()

# Explicit smoke target for the generated multi-object adapter DLL and scheduler.
add_executable(MultiObjectHarnessSmoke EXCLUDE_FROM_ALL
    tests/MultiObjectHarnessSmoke.cpp
    src/core/MultiObjectHarness.cpp
    src/core/InterfaceMappingProfile.cpp
    src/core/PeAnalyzer.cpp
    src/core/SingleThreadMultiObjectTester.cpp
    src/core/UserCodeHarness.cpp
    src/utils/MemoryUtils.cpp
    src/utils/SehHelper.cpp
)
target_compile_definitions(MultiObjectHarnessSmoke PRIVATE NOMINMAX)
target_link_libraries(MultiObjectHarnessSmoke PRIVATE Qt5::Core psapi.lib)

# Static precheck path smoke (PackageScanner + Header/Lib/PE) — no LoadLibrary.
add_executable(StaticPrecheckSmoke EXCLUDE_FROM_ALL
    tests/StaticPrecheckSmoke.cpp
    src/core/PackageScanner.cpp
    src/core/HeaderAnalyzer.cpp
    src/core/LibAnalyzer.cpp
    src/core/PeAnalyzer.cpp
    src/core/DllLoader.cpp
    src/utils/MemoryUtils.cpp
    src/utils/SehHelper.cpp
)
target_compile_definitions(StaticPrecheckSmoke PRIVATE NOMINMAX)
target_link_libraries(StaticPrecheckSmoke PRIVATE Qt5::Core psapi.lib)

add_executable(SampleModelsMatrix EXCLUDE_FROM_ALL
    tests/SampleModelsMatrix.cpp
    src/core/PackageScanner.cpp
    src/core/HeaderAnalyzer.cpp
    src/core/LibAnalyzer.cpp
    src/core/PeAnalyzer.cpp
    src/core/DllLoader.cpp
    src/core/PrecheckSummary.cpp
    src/utils/MemoryUtils.cpp
    src/utils/SehHelper.cpp
)
target_compile_definitions(SampleModelsMatrix PRIVATE NOMINMAX)
target_link_libraries(SampleModelsMatrix PRIVATE Qt5::Core psapi.lib)

# Install configuration (Default target directory: ${CMAKE_SOURCE_DIR}/dist)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist" CACHE PATH "Install path prefix" FORCE)
endif()

install(TARGETS ModelValidator DESTINATION .)

# CompatibleWeapon F/G packages (include/lib/models) when targets exist
if(TARGET CompatibleWeaponF)
    install(TARGETS CompatibleWeaponF
            RUNTIME DESTINATION sample_models/CompatibleWeaponF/models
            ARCHIVE DESTINATION sample_models/CompatibleWeaponF/lib
            LIBRARY DESTINATION sample_models/CompatibleWeaponF/models)
    install(FILES
            sample_models/CompatibleWeaponF/WeaponModel.h
            sample_models/CompatibleWeaponF/WeaponObject.h
            DESTINATION sample_models/CompatibleWeaponF/include)
    install(FILES sample_models/CompatibleWeaponF/README.md
            DESTINATION sample_models/CompatibleWeaponF)
endif()

if(TARGET CompatibleWeaponG)
    install(TARGETS CompatibleWeaponG
            RUNTIME DESTINATION sample_models/CompatibleWeaponG/models
            ARCHIVE DESTINATION sample_models/CompatibleWeaponG/lib
            LIBRARY DESTINATION sample_models/CompatibleWeaponG/models)
    install(FILES
            sample_models/CompatibleWeaponG/WeaponModel.h
            sample_models/CompatibleWeaponG/WeaponObject.h
            DESTINATION sample_models/CompatibleWeaponG/include)
    install(FILES sample_models/CompatibleWeaponG/README.md
            DESTINATION sample_models/CompatibleWeaponG)
endif()
