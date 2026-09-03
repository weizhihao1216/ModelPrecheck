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

find_package(Qt5 COMPONENTS Widgets Charts PrintSupport REQUIRED)

set(QSCINTILLA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/qscintilla-install")
set(QSCINTILLA_INCLUDE_DIR "${QSCINTILLA_ROOT}/include")
set(QSCINTILLA_LIBRARY "${QSCINTILLA_ROOT}/lib/qscintilla2_qt5.lib")
if(NOT EXISTS "${QSCINTILLA_LIBRARY}")
    message(FATAL_ERROR
        "QScintilla static library not found at ${QSCINTILLA_LIBRARY}. "
        "Build it first (see third_party/build_qscintilla.bat).")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${QSCINTILLA_INCLUDE_DIR}
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
    src/core/ReportGenerator.h
    src/core/ReportGenerator.cpp
    src/ui/LogConsoleWidget.h
    src/ui/LogConsoleWidget.cpp
    src/ui/ChartViewerWidget.h
    src/ui/ChartViewerWidget.cpp
    src/ui/TrajectoryViewWidget.h
    src/ui/TrajectoryViewWidget.cpp
    src/ui/CppCodeEditor.h
    src/ui/CppCodeEditor.cpp
    src/ui/CppCodeCompletion.h
    src/ui/CppCodeCompletion.cpp
    src/ui/BusyOverlayWidget.h
    src/ui/BusyOverlayWidget.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/resources.qrc
)

add_executable(ModelValidator WIN32 ${SOURCES})

target_link_libraries(ModelValidator PRIVATE
    Qt5::Widgets
    Qt5::Charts
    Qt5::PrintSupport
    "${QSCINTILLA_LIBRARY}"
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
add_subdirectory(sample_models/CompatibleWeaponF)
add_subdirectory(sample_models/CompatibleWeaponG)
add_subdirectory(sample_models/RenamedInterfaceWeapon)
add_subdirectory(sample_models/PrecheckDemoWeapon)

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

# Install configuration (Default target directory: ${CMAKE_SOURCE_DIR}/dist)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist" CACHE PATH "Install path prefix" FORCE)
endif()

install(TARGETS ModelValidator DESTINATION .)

# Each sample model package uses fixed layout: include/ + lib/ + models/
set(_SAMPLE_MODELS
    StandardWeapon
    FaultyWeapon
    MultiInstanceWeapon
    RenamedInterfaceWeapon
    ConflictWeaponA
    ConflictWeaponB
    CompatibleWeaponC
    CompatibleWeaponD
    CompatibleWeaponE
)
foreach(_model IN LISTS _SAMPLE_MODELS)
    install(TARGETS ${_model}
            RUNTIME DESTINATION sample_models/${_model}/models
            ARCHIVE DESTINATION sample_models/${_model}/lib
            LIBRARY DESTINATION sample_models/${_model}/models)
endforeach()

install(TARGETS PrecheckDemoWeapon
        RUNTIME DESTINATION sample_models/PrecheckDemoWeapon/models
        ARCHIVE DESTINATION sample_models/PrecheckDemoWeapon/lib
        LIBRARY DESTINATION sample_models/PrecheckDemoWeapon/models)
install(FILES
        sample_models/PrecheckDemoWeapon/WeaponModel.h
        sample_models/PrecheckDemoWeapon/WeaponObject.h
        DESTINATION sample_models/PrecheckDemoWeapon/include)
install(FILES sample_models/PrecheckDemoWeapon/README.md
        DESTINATION sample_models/PrecheckDemoWeapon)

install(FILES sample_models/StandardWeapon/WeaponModel.h
        DESTINATION sample_models/StandardWeapon/include)
install(FILES sample_models/MultiInstanceWeapon/WeaponModel.h
        DESTINATION sample_models/MultiInstanceWeapon/include)
install(FILES sample_models/MultiInstanceWeapon/WeaponObject.h
        DESTINATION sample_models/MultiInstanceWeapon/include)
install(FILES sample_models/RenamedInterfaceWeapon/WeaponModel.h
        DESTINATION sample_models/RenamedInterfaceWeapon/include)
install(FILES sample_models/ConflictWeaponA/WeaponModel.h
        DESTINATION sample_models/ConflictWeaponA/include)
install(FILES sample_models/ConflictWeaponB/WeaponModel.h
        DESTINATION sample_models/ConflictWeaponB/include)
install(FILES sample_models/CompatibleWeaponC/WeaponModel.h
        DESTINATION sample_models/CompatibleWeaponC/include)
install(FILES sample_models/CompatibleWeaponD/WeaponModel.h
        DESTINATION sample_models/CompatibleWeaponD/include)
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

install(FILES src/ui/themestyle.qss DESTINATION .)
