@echo off
rem Peek build script. Requires a MinGW-w64 GCC/G++ toolchain on PATH.
rem
rem Usage:
rem   build.bat [x64|arm64] [release|debug]
rem
rem Output: build\<arch>\Peek.exe
rem
rem Toolchains:
rem   x64   - x86_64-w64-mingw32-g++, or plain g++ from an MSYS2 MinGW shell
rem   arm64 - aarch64-w64-mingw32-g++ (for example from llvm-mingw)
rem
rem Set CXX and RC yourself to override tool detection.
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=release"

if /i "%ARCH%"=="x64" (
    set "PREFIX=x86_64-w64-mingw32-"
) else if /i "%ARCH%"=="arm64" (
    set "PREFIX=aarch64-w64-mingw32-"
) else (
    echo Unknown architecture "%ARCH%". Use x64 or arm64.
    exit /b 1
)

if not defined CXX (
    where !PREFIX!g++ >nul 2>nul
    if not errorlevel 1 (
        set "CXX=!PREFIX!g++"
    ) else if /i "%ARCH%"=="x64" (
        where g++ >nul 2>nul
        if not errorlevel 1 set "CXX=g++"
    )
)
if not defined CXX (
    echo Could not find a MinGW-w64 C++ compiler for %ARCH%.
    echo Install MSYS2 ^(https://www.msys2.org^) and its mingw-w64 GCC package, or llvm-mingw for arm64,
    echo then make sure the compiler is on PATH.
    exit /b 1
)
if not defined RC (
    where !PREFIX!windres >nul 2>nul
    if not errorlevel 1 (
        set "RC=!PREFIX!windres"
    ) else (
        set "RC=windres"
    )
)

set "OUT=build\%ARCH%"
if not exist "%OUT%\obj" mkdir "%OUT%\obj"

set "COMMON=-std=c++20 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -Wall -Wextra -Isrc -Iresources"
if /i "%CONFIG%"=="debug" (
    set "OPT=-O0 -g -DPEEK_DEBUG"
    set "LINKOPT="
) else (
    set "OPT=-O2 -DNDEBUG -ffunction-sections -fdata-sections"
    set "LINKOPT=-s -Wl,--gc-sections"
)

echo Compiler : %CXX%
echo Config   : %ARCH% %CONFIG%

echo Compiling resources...
"%RC%" -O coff -I resources resources\peek.rc -o "%OUT%\obj\peek_res.o"
if errorlevel 1 exit /b 1

set "OBJECTS="
set "FAILED=0"
for /r src %%F in (*.cpp) do (
    set "REL=%%~pnF"
    set "OBJ=%OUT%\obj\%%~nF.o"
    echo   %%~nxF
    "%CXX%" %COMMON% %OPT% -c "%%F" -o "!OBJ!"
    if errorlevel 1 set "FAILED=1"
    set "OBJECTS=!OBJECTS! "!OBJ!""
)
if "%FAILED%"=="1" (
    echo Build failed.
    exit /b 1
)

echo Linking...
"%CXX%" -municode -mwindows -static -static-libgcc -static-libstdc++ %LINKOPT% !OBJECTS! "%OUT%\obj\peek_res.o" -o "%OUT%\Peek.exe" ^
    -lcomctl32 -ldwmapi -lole32 -loleaut32 -lshell32 -lshlwapi -lcomdlg32 -lrstrtmgr -lwbemuuid -lpsapi -ladvapi32 -lgdi32 -luser32 -luuid -luxtheme
if errorlevel 1 (
    echo Link failed.
    exit /b 1
)

echo Built %OUT%\Peek.exe
exit /b 0
