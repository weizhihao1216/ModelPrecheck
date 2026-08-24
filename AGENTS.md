# AGENTS.md - Antigravity Agent Guidelines & Context

This file provides system context, tech stack rules, and component locations for AI coding assistants working on the **Model Validator (第三方武器模型DLL集成预检工具)** codebase.

## 1. Core Specification & Architecture

- **Project Purpose**: Validate third-party C++ weapon model DLLs prior to simulation engine integration.
- **Primary Spec File**: See [`README.md`](file:///d:/HR/Game/ModelPrecheck/README.md) for full requirement mapping (F1-F13).
- **Environment**: Visual Studio 2017 (MSVC v141 toolset), Qt 5.11 x64 (`D:\HR\DEV\ThirdParty\Qt5.11\vc140\x64`), CMake 3.12+.

## 2. Mandatory Rules

1. **NO MINMAX MACRO CONTAMINATION**: Always define `NOMINMAX` before including `<windows.h>` to prevent `windows.h` `min`/`max` macros from corrupting Qt headers (`<QtCharts/QValueAxis>`).
2. **SEH HARDWARE EXCEPTION SAFETY**: All DLL invocations (`LoadLibrary`, `Model_Init`, `Model_Step`, `Model_Destroy`) MUST be executed via MSVC SEH wrappers (`SafeCallInit`, `SafeCallStep`) in [`src/utils/SehHelper.cpp`](file:///d:/HR/Game/ModelPrecheck/src/utils/SehHelper.cpp).
3. **DECOUPLE QTCHARTS FROM HEADERS**: Do NOT include `QT_CHARTS_USE_NAMESPACE` inside header (`.h`) files to prevent namespace pollution. Forward declare QtCharts classes or include QtCharts headers only in `.cpp` files.

## 3. Key Entry Points & Class Roles

- `PeAnalyzer`: [`src/core/PeAnalyzer.h`](file:///d:/HR/Game/ModelPrecheck/src/core/PeAnalyzer.h) - PE header parser (x86/x64, CRT MD/MT, Import/Export directory).
- `DllLoader`: [`src/core/DllLoader.h`](file:///d:/HR/Game/ModelPrecheck/src/core/DllLoader.h) - Dynamic loading & SEH function binding.
- `PerfProfiler`: [`src/core/PerfProfiler.h`](file:///d:/HR/Game/ModelPrecheck/src/core/PerfProfiler.h) - Multithreaded stress testing, jitter analysis, memory leak detection.
- `FunctionalVerifier`: [`src/core/FunctionalVerifier.h`](file:///d:/HR/Game/ModelPrecheck/src/core/FunctionalVerifier.h) - Trajectory NaN/Inf, bounds, and degree/radian unit checks.
- `ReportGenerator`: [`src/core/ReportGenerator.h`](file:///d:/HR/Game/ModelPrecheck/src/core/ReportGenerator.h) - HTML/PDF test report generation.

## 4. Build Commands

```powershell
# One-click build script
.\build.bat

# Or manual CMake commands
cmake -G "Visual Studio 15 2017 Win64" -B build
cmake --build build --config Release --target install
```
