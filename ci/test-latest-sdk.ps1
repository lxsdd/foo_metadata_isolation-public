#requires -Version 7.2
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# This compatibility experiment intentionally changes ONLY a disposable
# GitHub Windows checkout, never the user's local SDK or music library.
if ($env:GITHUB_ACTIONS -cne 'true' -or [string]::IsNullOrWhiteSpace($env:GITHUB_WORKSPACE) -or
    [string]::IsNullOrWhiteSpace($env:RUNNER_TEMP)) {
    throw 'Latest-SDK compatibility swap is CI-only (isolated temporary runner).'
}
$Root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$Workspace = (Resolve-Path -LiteralPath $env:GITHUB_WORKSPACE).Path
if ($Root -cne $Workspace) { throw 'SDK swap refuses a non-workspace repository.' }
$SdkRoot = Join-Path $Root 'sdk'
$SourceRoot = Join-Path $Root 'external/sdk-template'
$Vendor = Join-Path $SourceRoot 'vendor/sdk'
$Commit = '31a35947a7527826c2199491692802e3ce3d8bd7'
$Tree = '2ba8caeb73f72633ad7f60ee5f72ed8074a0eb17'
$Version = '2026-10-01'
$Git = (Get-Command 'git.exe' -ErrorAction Stop).Source
if (-not (Test-Path -LiteralPath $SdkRoot -PathType Container)) { throw 'Existing SDK slot missing.' }
if (-not (Test-Path -LiteralPath $Vendor -PathType Container)) { throw 'Pinned SDK template checkout missing.' }
$actualCommit = (& $Git -C $SourceRoot rev-parse HEAD).Trim()
$actualTree = (& $Git -C $SourceRoot rev-parse 'HEAD:vendor/sdk').Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -cne $Commit -or $actualTree -cne $Tree) {
    throw 'Wrong owner SDK commit/tree; do not stage.'
}
$manifest = Get-Content -LiteralPath (Join-Path $SourceRoot 'SDK-PIN.json') -Raw | ConvertFrom-Json
if ($manifest.sdk_version -cne $Version -or
    $manifest.upstream_url -cne 'https://www.foobar2000.org/downloads/SDK-2026-10-01.7z' -or
    $manifest.upstream_archive_sha256 -cne 'd4c55077336fae81bf8df0259b5b2748fa45ea84132c656ead93eb123cbcdc26' -or
    [int64]$manifest.archive_size_bytes -ne 793738L) {
    throw 'Original official SDK pin evidence differs from the verified owner Git tree.'
}
foreach ($entry in @('sdk-license.txt','sdk-readme.html','foobar2000/SDK/foobar2000.h',
    'foobar2000/SDK/foobar2000_SDK.vcxproj', 'foobar2000/helpers/foobar2000_sdk_helpers.vcxproj',
    'foobar2000/foobar2000_component_client/foobar2000_component_client.vcxproj',
    'foobar2000/shared/shared-Win32.lib','foobar2000/shared/shared-x64.lib',
    'pfc/pfc.vcxproj','libPPUI/libPPUI.vcxproj')) {
    if (-not (Test-Path -LiteralPath (Join-Path $Vendor $entry) -PathType Leaf)) {
        throw "Pinned SDK source lacks $entry"
    }
}
$readme=Get-Content -LiteralPath (Join-Path $Vendor 'sdk-readme.html') -Raw
$license=Get-Content -LiteralPath (Join-Path $Vendor 'sdk-license.txt') -Raw
if (-not $readme.Contains("foobar2000 SDK, version $Version") -or
    -not $license.Contains('Redistribution and use in source and binary forms')) {
    throw 'Pinned version and redistributed SDK notices are incomplete.'
}
# Stage and preflight before replacing the *temporary CI checkout* SDK slot.
$Pending = Join-Path $env:RUNNER_TEMP ('metadata-isolation-sdk-2026-10-01-'+[guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $Pending -Force | Out-Null
    Copy-Item -Path (Join-Path $Vendor '*') -Destination $Pending -Recurse -Force
    if (-not (Test-Path -LiteralPath (Join-Path $Pending 'foobar2000/SDK/foobar2000_SDK.vcxproj'))) {
        throw 'Copied pinned SDK source was incomplete.'
    }
    # Deliberately discard old SDK files only from this fresh GitHub workspace
    # to prevent stale 2025 headers entering new SDK compiles.
    Remove-Item -LiteralPath $SdkRoot -Recurse -Force
    Move-Item -LiteralPath $Pending -Destination $SdkRoot -ErrorAction Stop
    Write-Host "METADATA_ISOLATION_SDK_SOURCE_2026_10_01=PASS GitCommit=$Commit SDKTree=$Tree"
    Write-Host 'METADATA_ISOLATION_SDK_UPSTREAM_ARCHIVE_SHA256=NOT_TESTED (Git tree verified)'
} finally {
    Remove-Item -LiteralPath $Pending -Recurse -Force -ErrorAction SilentlyContinue
}
