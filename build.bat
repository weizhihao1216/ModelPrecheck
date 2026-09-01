@echo off
title ModelValidator Build Script
setlocal EnableDelayedExpansion

set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%build
set DIST_DIR=%PROJECT_DIR%dist

REM Prefer QTDIR env, then local Qt 5.14.2, then legacy Qt 5.11 path
set "QT_DIR="
if defined QTDIR if exist "%QTDIR%\bin\windeployqt.exe" set "QT_DIR=%QTDIR%"
if not defined QT_DIR if exist "D:\Qt\5.14.2\msvc2017_64\bin\windeployqt.exe" set "QT_DIR=D:\Qt\5.14.2\msvc2017_64"
if not defined QT_DIR if exist "D:\HR\DEV\ThirdParty\Qt5.11\vc140\x64\bin\windeployqt.exe" set "QT_DIR=D:\HR\DEV\ThirdParty\Qt5.11\vc140\x64"

set "PATH=%QT_DIR%\bin;C:\Program Files\CMake\bin;%PATH%"

echo ============================================================
echo   Model Validator One-Click Build Script
echo ============================================================
echo.

if not defined QT_DIR (
    echo [ERROR] Qt not found. Install Qt 5.x msvc2017_64 or set QTDIR.
    pause
    exit /b 1
)

echo Using Qt: %QT_DIR%
echo.

echo [1/3] Configuring CMake Project (VS2022 x64)...
cmake -G "Visual Studio 17 2022" -A x64 -B "%BUILD_DIR%" -DCMAKE_INSTALL_PREFIX="%DIST_DIR%" -DCMAKE_PREFIX_PATH="%QT_DIR%"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [2/3] Building and Installing Release Target...
cmake --build "%BUILD_DIR%" --config Release --target install
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build or install failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [3/3] Deploying Qt Runtime Dependencies...
"%QT_DIR%\bin\windeployqt.exe" "%DIST_DIR%\ModelValidator.exe"

echo.
echo ============================================================
echo   Build Completed Successfully!
echo   Output Executable: %DIST_DIR%\ModelValidator.exe
echo ============================================================
echo.

pause
