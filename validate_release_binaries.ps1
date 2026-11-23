[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$binaries = @(
    (Join-Path $root 'build\foo_metadata_isolation.dll'),
    (Join-Path $root 'build\x64\foo_metadata_isolation.dll')
)
$forbidden = @(
    '/metadata-isolation-probe',
    '/metadata-isolation-probe-tooltip',
    'Research: Set projected shadow',
    'Metadata Isolation tooltip probe',
    'A distinct second tooltip mapping.',
    'diagnostic_',
    'Diagnostic:',
    'benchmark_result=',
    '.metadata-isolation-probe.txt',
    'ACCESS_TOKEN_SENTINEL_DO_NOT_LOG',
    'REFRESH_TOKEN_SENTINEL_DO_NOT_LOG',
    'PASSWORD_SENTINEL_DO_NOT_LOG',
    'COOKIE_SENTINEL_DO_NOT_LOG'
)

foreach ($binary in $binaries) {
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
        throw "Missing release binary: $binary"
    }
    $bytes = [IO.File]::ReadAllBytes($binary)
    $ascii = [Text.Encoding]::ASCII.GetString($bytes)
    $unicode = [Text.Encoding]::Unicode.GetString($bytes)
    foreach ($token in $forbidden) {
        if ($ascii.IndexOf($token, [StringComparison]::Ordinal) -ge 0 -or
            $unicode.IndexOf($token, [StringComparison]::Ordinal) -ge 0) {
            throw "Forbidden test-probe token '$token' found in release binary: $binary"
        }
    }

    # $ascii already has exactly one character per input byte. Native .NET
    # ordinal substring search avoids millions of interpreted PowerShell
    # byte-array accesses without weakening the original RSDS/path checks.
    $codeViewRecords = 0
    $offset = $ascii.IndexOf('RSDS', [StringComparison]::Ordinal)
    while ($offset -ge 0 -and $offset -le $bytes.Length - 25) {
        $pathStart = $offset + 24
        $pathEnd = $pathStart
        while ($pathEnd -lt $bytes.Length -and $bytes[$pathEnd] -ne 0 -and
            ($pathEnd - $pathStart) -lt 1024) {
            ++$pathEnd
        }
        if ($pathEnd -gt $pathStart -and $pathEnd -lt $bytes.Length) {
            $pdbPath = [Text.Encoding]::ASCII.GetString($bytes, $pathStart, $pathEnd - $pathStart)
            if ($pdbPath.EndsWith('.pdb', [StringComparison]::OrdinalIgnoreCase)) {
                ++$codeViewRecords
                if ([IO.Path]::IsPathRooted($pdbPath) -or $pdbPath -ne [IO.Path]::GetFileName($pdbPath)) {
                    throw "Absolute or directory-qualified PDB path '$pdbPath' found in release binary: $binary"
                }
            }
        }
        $offset = $ascii.IndexOf('RSDS', $offset + 1, [StringComparison]::Ordinal)
    }
    if ($codeViewRecords -ne 1) {
        throw "Expected exactly one portable CodeView/PDB record in release binary, found ${codeViewRecords}: $binary"
    }
}

Write-Host 'Release binary probe-token and portable-PDB-path validation passed for Win32 and x64.'
