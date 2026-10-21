[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$ComponentPath,
    [string]$DllName,
    [string]$ExpectedWin32DllPath,
    [string]$ExpectedX64DllPath
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-PeMachine {
    param([Parameter(Mandatory=$true)][string]$Path)
    $bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Path))
    if ($bytes.Length -lt 0x40 -or $bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) {
        throw "$Path is not a valid PE file (missing MZ header)."
    }
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
    if ($peOffset -lt 0 -or ($peOffset + 6) -gt $bytes.Length) {
        throw "$Path has an invalid PE header offset."
    }
    if ($bytes[$peOffset] -ne 0x50 -or $bytes[$peOffset + 1] -ne 0x45 -or $bytes[$peOffset + 2] -ne 0 -or $bytes[$peOffset + 3] -ne 0) {
        throw "$Path has no PE signature."
    }
    [BitConverter]::ToUInt16($bytes, $peOffset + 4)
}

$component = (Resolve-Path -LiteralPath $ComponentPath).Path
$temp = Join-Path ([IO.Path]::GetTempPath()) ('fb2k-verify-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $temp | Out-Null

try {
    # Inspect ZIP names before extraction. Valid combined packages contain
    # exactly the two architecture DLLs and, optionally, an x64/ directory.
    # Never allow disguised binaries, payload extras or duplicate names.
    $archive = [IO.Compression.ZipFile]::OpenRead($component)
    try {
        $allowed = @($DllName, "x64/$DllName", 'x64/')
        if ([string]::IsNullOrWhiteSpace($DllName)) {
            $allowed = @('x64/') # Resolve an inferred DLL below for compatibility.
        }
        $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($entry in $archive.Entries) {
            $entryName = $entry.FullName
            if ($entryName -match '[\\\\]' -or $entryName.StartsWith('/') -or
                $entryName.Contains('../') -or $entryName.Contains('/./') -or
                -not $names.Add($entryName)) {
                throw 'Combined component contains unsafe or duplicated ZIP names'
            }
            if ($entry.Length -gt 67108864) {
                throw 'Combined component contains oversized ZIP member'
            }
        }
        $actual = @($names | Where-Object { $_ -cne 'x64/' })
        if ($actual.Count -ne 2) {
            throw 'Combined component must contain exactly one Win32 and one x64 DLL'
        }
        if ([string]::IsNullOrWhiteSpace($DllName)) {
            $rootNames = @($actual | Where-Object { $_ -notmatch '/' })
            if ($rootNames.Count -ne 1 -or -not $rootNames[0].EndsWith('.dll',[StringComparison]::OrdinalIgnoreCase)) {
                throw 'Could not uniquely infer root component DLL'
            }
            $DllName = $rootNames[0]
        }
        if (-not $names.Contains($DllName) -or -not $names.Contains("x64/$DllName")) {
            throw 'Combined component has unexpected architecture paths'
        }
        if (@($names | Where-Object { $_ -cne $DllName -and $_ -cne "x64/$DllName" -and $_ -cne 'x64/' }).Count -ne 0) {
            throw 'Combined component has unapproved extra payload entries'
        }
    } finally {
        $archive.Dispose()
    }
    [IO.Compression.ZipFile]::ExtractToDirectory($component, $temp)

    if (Test-Path -LiteralPath (Join-Path $temp 'x86')) {
        throw 'Invalid combined component layout: x86/ directory exists. Win32 DLL must be in the archive root.'
    }

    if ([string]::IsNullOrWhiteSpace($DllName)) {
        $rootDlls = @(Get-ChildItem -LiteralPath $temp -File -Filter '*.dll')
        if ($rootDlls.Count -ne 1) { throw "Expected exactly one root DLL, found $($rootDlls.Count)." }
        $DllName = $rootDlls[0].Name
    }

    $rootDll = Join-Path $temp $DllName
    $x64Dll = Join-Path (Join-Path $temp 'x64') $DllName
    if (-not (Test-Path -LiteralPath $rootDll -PathType Leaf)) { throw "Missing root Win32 DLL: $DllName" }
    if (-not (Test-Path -LiteralPath $x64Dll -PathType Leaf)) { throw "Missing x64 DLL: x64/$DllName" }

    $rootMachine = Get-PeMachine -Path $rootDll
    $x64Machine = Get-PeMachine -Path $x64Dll
    if ($rootMachine -ne 0x014c) { throw ('Root DLL machine 0x{0:X4}; expected x86/I386 0x014C.' -f $rootMachine) }
    if ($x64Machine -ne 0x8664) { throw ('x64 DLL machine 0x{0:X4}; expected AMD64 0x8664.' -f $x64Machine) }

    # Machine and ZIP structure are not content provenance. The packed bytes
    # must be the exact compiler outputs of the checked-out source tree.
    $hasWin32 = -not [string]::IsNullOrWhiteSpace($ExpectedWin32DllPath)
    $hasX64 = -not [string]::IsNullOrWhiteSpace($ExpectedX64DllPath)
    if ($hasWin32 -ne $hasX64) {
        throw 'Expected built DLL paths must be supplied together'
    }
    $actualWin32Hash = (Get-FileHash -LiteralPath $rootDll -Algorithm SHA256).Hash.ToLowerInvariant()
    $actualX64Hash = (Get-FileHash -LiteralPath $x64Dll -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hasWin32) {
        $builtWin32Hash = (Get-FileHash -LiteralPath $ExpectedWin32DllPath -Algorithm SHA256).Hash.ToLowerInvariant()
        $builtX64Hash = (Get-FileHash -LiteralPath $ExpectedX64DllPath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualWin32Hash -cne $builtWin32Hash -or $actualX64Hash -cne $builtX64Hash) {
            throw 'Packaged DLL bytes do not match the exact built Win32 and x64 DLLs'
        }
    }
    $componentHash = (Get-FileHash -LiteralPath $component -Algorithm SHA256).Hash.ToLowerInvariant()
    [pscustomobject]@{
        Component = $component
        DllName = $DllName
        RootMachine = ('0x{0:X4}' -f $rootMachine)
        X64Machine = ('0x{0:X4}' -f $x64Machine)
        Win32DllSha256 = $actualWin32Hash
        X64DllSha256 = $actualX64Hash
        PayloadMatchesBuild = $hasWin32
        Sha256 = $componentHash
        Status = 'PASS'
    }
} finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
}
