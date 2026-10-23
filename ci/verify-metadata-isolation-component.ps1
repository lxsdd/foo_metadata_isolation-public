[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$ComponentPath,
    [string]$DllName,
    [string]$ExpectedWin32DllPath,
    [string]$ExpectedX64DllPath
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

# The exact source-pinned shared verifier owns generic ZIP/PE validation.
# Product policy adds a strict two-DLL payload allowlist and SHA-256 checks
# against both locally built architecture binaries, without cloning that code.
$component = (Resolve-Path -LiteralPath $ComponentPath -ErrorAction Stop).Path
$archive = [IO.Compression.ZipFile]::OpenRead($component)
try {
    $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $archive.Entries) {
        $name = $entry.FullName
        if ($name.Contains([char]92) -or $name.StartsWith('/') -or
            $name.Contains('../') -or $name.Contains('/./') -or
            -not $names.Add($name)) {
            throw 'Combined component contains unsafe or duplicated ZIP names'
        }
        if ($entry.Length -gt 67108864) {
            throw 'Combined component contains oversized ZIP member'
        }
    }
    $payload = @($archive.Entries | Where-Object { $_.FullName -cne 'x64/' })
    if ($payload.Count -ne 2) {
        throw 'Combined component must contain exactly one Win32 and one x64 DLL'
    }
    if ([string]::IsNullOrWhiteSpace($DllName)) {
        $rootNames = @($payload | Where-Object { -not $_.FullName.Contains('/') })
        if ($rootNames.Count -ne 1 -or
            -not $rootNames[0].FullName.EndsWith('.dll',[StringComparison]::OrdinalIgnoreCase)) {
            throw 'Could not uniquely infer root component DLL'
        }
        $DllName = $rootNames[0].FullName
    }
    if ($payload[0].FullName -cne $DllName -and $payload[1].FullName -cne $DllName) {
        throw 'Combined component has no exact Win32 DLL path'
    }
    if ($payload[0].FullName -cne "x64/$DllName" -and
        $payload[1].FullName -cne "x64/$DllName") {
        throw 'Combined component has no exact x64 DLL path'
    }
    $hashes = @{}
    foreach ($entry in $payload) {
        $digest = [Security.Cryptography.SHA256]::Create()
        try {
            $stream = $entry.Open()
            try {
                $hashes[$entry.FullName] = [BitConverter]::ToString(
                    $digest.ComputeHash($stream)).Replace('-','').ToLowerInvariant()
            } finally { $stream.Dispose() }
        } finally { $digest.Dispose() }
    }
} finally { $archive.Dispose() }

$core = & "$PSScriptRoot/verify-fb2k-component.ps1" -ComponentPath $component -DllName $DllName
if ($core.Status -cne 'PASS' -or $core.DllName -cne $DllName) {
    throw 'Pinned shared component ZIP/PE validation failed'
}

$hasWin32 = -not [string]::IsNullOrWhiteSpace($ExpectedWin32DllPath)
$hasX64 = -not [string]::IsNullOrWhiteSpace($ExpectedX64DllPath)
if ($hasWin32 -ne $hasX64) {
    throw 'Expected built DLL paths must be supplied together'
}
$winHash = $hashes[$DllName]
$x64Hash = $hashes["x64/$DllName"]
if ($hasWin32) {
    $builtWinHash = (Get-FileHash -LiteralPath $ExpectedWin32DllPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $builtX64Hash = (Get-FileHash -LiteralPath $ExpectedX64DllPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($winHash -cne $builtWinHash -or $x64Hash -cne $builtX64Hash) {
        throw 'Packaged DLL bytes do not match the exact built Win32 and x64 DLLs'
    }
}
[pscustomobject]@{
    Component = $component
    DllName = $DllName
    RootMachine = $core.RootMachine
    X64Machine = $core.X64Machine
    Win32DllSha256 = $winHash
    X64DllSha256 = $x64Hash
    PayloadMatchesBuild = $hasWin32
    Sha256 = $core.Sha256
    Status = 'PASS'
}
