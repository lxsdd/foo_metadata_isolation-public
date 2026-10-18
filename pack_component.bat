@echo off
setlocal EnableExtensions

set "PROJECT_NAME=foo_metadata_isolation"
set "PROJECT_VERSION=1.0.0-dev"
set "ROOT_DIR=%~dp0"
set "BUILD_DIR=%ROOT_DIR%build"
set "STAGE_DIR=%BUILD_DIR%\package_tmp"
set "COMPONENT_FILE=%BUILD_DIR%\%PROJECT_NAME%_%PROJECT_VERSION%.fb2k-component"
set "BUILD_OK_MARKER=%BUILD_DIR%\last_build_success.marker"

if not exist "%BUILD_OK_MARKER%" (
    echo [ERROR] No successful build marker. Run build.bat first.
    exit /b 1
)
if not exist "%BUILD_DIR%\%PROJECT_NAME%.dll" exit /b 1
if not exist "%BUILD_DIR%\x64\%PROJECT_NAME%.dll" exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT_DIR%validate_release_binaries.ps1"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop'; $marker=(Get-Item '%BUILD_OK_MARKER%').LastWriteTimeUtc;" ^
  "$inputs=Get-ChildItem -Path '%ROOT_DIR%src','%ROOT_DIR%sdk','%ROOT_DIR%wtl','%ROOT_DIR%component.sln','%ROOT_DIR%component.props','%ROOT_DIR%build.bat' -Recurse -File -ErrorAction SilentlyContinue;" ^
  "$newer=$inputs | Where-Object LastWriteTimeUtc -gt $marker | Select-Object -First 1;" ^
  "if($newer){ Write-Host ('[ERROR] Build is stale: '+$newer.FullName); exit 2 }"
if errorlevel 1 exit /b 1

if exist "%STAGE_DIR%" rd /s /q "%STAGE_DIR%" || exit /b 1
mkdir "%STAGE_DIR%\x64" || exit /b 1
copy "%BUILD_DIR%\%PROJECT_NAME%.dll" "%STAGE_DIR%\" >nul || exit /b 1
copy "%BUILD_DIR%\x64\%PROJECT_NAME%.dll" "%STAGE_DIR%\x64\" >nul || exit /b 1
if exist "%ROOT_DIR%licenses\component-license.txt" (
    copy "%ROOT_DIR%licenses\component-license.txt" "%STAGE_DIR%\" >nul || exit /b 1
)

if exist "%COMPONENT_FILE%" del /f /q "%COMPONENT_FILE%" || exit /b 1
pushd "%STAGE_DIR%" || exit /b 1
if exist "C:\Program Files\7-Zip\7z.exe" (
    "C:\Program Files\7-Zip\7z.exe" a -tzip "%COMPONENT_FILE%" * >nul || (popd & exit /b 1)
) else if exist "C:\Program Files (x86)\7-Zip\7z.exe" (
    "C:\Program Files (x86)\7-Zip\7z.exe" a -tzip "%COMPONENT_FILE%" * >nul || (popd & exit /b 1)
) else (
    tar -a -c -f "%COMPONENT_FILE%" * >nul 2>nul || (popd & exit /b 1)
)
popd
rd /s /q "%STAGE_DIR%" || exit /b 1
echo Package created: %COMPONENT_FILE%
exit /b 0


