@echo off
title Install VS Build Tools (C++)
setlocal
cd /d "%~dp0"

if not exist "%~dp0vs_BuildTools.exe" (
    echo [ERROR] 未找到 vs_BuildTools.exe
    echo 请先运行 download_prerequisites.ps1 下载安装包。
    pause
    exit /b 1
)

echo ============================================================
echo   即将安装 Visual Studio Build Tools
echo   工作负载: 使用 C++ 的桌面开发 / VCTools
echo   需要管理员权限与网络，安装体积较大。
echo ============================================================
echo.
pause

REM --wait: 等安装结束再返回；--passive: 进度条界面，少交互
"%~dp0vs_BuildTools.exe" --wait --passive ^
  --add Microsoft.VisualStudio.Workload.VCTools ^
  --includeRecommended ^
  --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64

set ERR=%ERRORLEVEL%
echo.
if %ERR% equ 0 (
    echo [OK] 安装流程已结束。请重新打开 ModelValidator 后再试编译。
) else (
    echo [WARN] 安装程序退出码: %ERR%
    echo 若已取消或失败，可手动双击 vs_BuildTools.exe，勾选「使用 C++ 的桌面开发」。
)
echo.
pause
exit /b %ERR%
