@echo off
rem Removes all build output.
cd /d "%~dp0"
if exist build rmdir /s /q build
echo Clean.
