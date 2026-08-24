@echo off
title ModelValidator Build Script

set PROJECT_DIR=%~dp0
set QT_DIR=D:\HR\DEV\ThirdParty\Qt5.11\vc140\x64
set BUILD_DIR=%PROJECT_DIR%build
set DIST_DIR=%PROJECT_DIR%dist

set PATH=%QT_DIR%\bin;%PATH%

echo ============================================================
echo   Model Validator One-Click Build Script
echo ============================================================
echo.

if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo [ERROR] Qt environment not found at: %QT_DIR%
    pause
    exit /b 1
)

echo [1/3] Configuring CMake Project (VS2017 Win64)...
cmake -G "Visual Studio 15 2017 Win64" -B "%BUILD_DIR%" -DCMAKE_INSTALL_PREFIX="%DIST_DIR%"
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
