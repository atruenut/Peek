@echo off
rem Builds (if needed) and runs the built-in self-test.
rem Usage: test.bat [x64|arm64]
setlocal
cd /d "%~dp0"
set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"
if not exist "build\%ARCH%\Peek.exe" call build.bat %ARCH% release
if errorlevel 1 exit /b 1
set "REPORT=%TEMP%\Peek-selftest.txt"
"build\%ARCH%\Peek.exe" --self-test "%REPORT%"
set "CODE=%ERRORLEVEL%"
if exist "%REPORT%" type "%REPORT%"
exit /b %CODE%
