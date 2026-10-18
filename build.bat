@echo off
setlocal enabledelayedexpansion

rem MSBuild/.NET treats inherited Path and PATH keys as duplicates on some hosts.
set "__FB2K_ORIG_PATH=%PATH%"
set "PATH="
set "Path=%__FB2K_ORIG_PATH%"
set "__FB2K_ORIG_PATH="

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    echo [ERROR] vswhere.exe was not found: !VSWHERE!
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    set "MSBUILD=%%i"
    goto :found
)

:found
if not defined MSBUILD (
    echo [ERROR] MSBuild.exe was not found.
    exit /b 1
)

set "BUILD_DIR=%~dp0build"
set "BUILD_OK_MARKER=!BUILD_DIR!\last_build_success.marker"
set "BUILD_TARGET=Rebuild"
if defined FB2K_INCREMENTAL_BUILD set "BUILD_TARGET=Build"
set "TEST_PROBE_PROPERTY="
if /i "!FB2K_ENABLE_TEST_PROBES!"=="1" set "TEST_PROBE_PROPERTY=/p:EnableTestProbes=true"
if not exist "!BUILD_DIR!" mkdir "!BUILD_DIR!" >nul 2>nul
if exist "!BUILD_OK_MARKER!" del /f /q "!BUILD_OK_MARKER!" >nul 2>nul

echo Using MSBuild: !MSBUILD!
echo Building foo_metadata_isolation Win32 Release...
"!MSBUILD!" component.sln /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v145 /p:CppLanguageStandard=cpp23 !TEST_PROBE_PROPERTY! /t:!BUILD_TARGET!
if errorlevel 1 exit /b 1

echo Building foo_metadata_isolation x64 Release...
"!MSBUILD!" component.sln /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:CppLanguageStandard=cpp23 !TEST_PROBE_PROPERTY! /t:!BUILD_TARGET!
if errorlevel 1 exit /b 1

if not exist "!BUILD_DIR!\foo_metadata_isolation.dll" (
    echo [ERROR] Win32 output is missing.
    exit /b 1
)
if not exist "!BUILD_DIR!\x64\foo_metadata_isolation.dll" (
    echo [ERROR] x64 output is missing.
    exit /b 1
)

> "!BUILD_OK_MARKER!" echo Build succeeded at %DATE% %TIME%
echo Build successful.
exit /b 0

