@echo off
REM Build static QScintilla for ModelPrecheck (Qt 5.14.2 msvc2017_64).
setlocal
set QTDIR=D:\Qt\5.14.2\msvc2017_64
set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
set SRC=%~dp0QScintilla_src-2.14.1\src
set INST=%~dp0qscintilla-install

call "%VCVARS%"
if errorlevel 1 exit /b 1
cd /d "%SRC%"
"%QTDIR%\bin\qmake.exe" CONFIG+=staticlib
if errorlevel 1 exit /b 1
nmake release
if errorlevel 1 exit /b 1

if not exist "%INST%\include" mkdir "%INST%\include"
if not exist "%INST%\lib" mkdir "%INST%\lib"
xcopy /E /I /Y "%SRC%\Qsci" "%INST%\include\Qsci" >nul
copy /Y "%SRC%\release\qscintilla2_qt5.lib" "%INST%\lib\" >nul
echo Installed to %INST%
endlocal
