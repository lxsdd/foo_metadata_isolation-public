[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('x86', 'x64')]
    [string]$Architecture,

    [Parameter(Mandatory = $true)]
    [string]$RuntimeRoot,

    [Parameter(Mandatory = $true)]
    [string]$Fixture,

    [string]$Python = 'python'
)

$ErrorActionPreference = 'Stop'
$exe = Join-Path $RuntimeRoot 'foobar2000.exe'
$portable = Join-Path $RuntimeRoot 'portable_mode_enabled'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "Missing foobar2000 executable: $exe"
}
if (-not (Test-Path -LiteralPath $portable -PathType Leaf)) {
    throw "Refusing non-portable runtime: $RuntimeRoot"
}
$source = [IO.Path]::GetFullPath($Fixture)
if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
    throw "Missing audit classifier fixture: $source"
}
$report = "$source.metadata-isolation-probe.txt"
if (Test-Path -LiteralPath $report) {
    throw "Refusing stale report: $report"
}
$before = (Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash

$runtimeProcess = $null
try {
    $runtimeProcess = Start-Process -FilePath $exe -WorkingDirectory $RuntimeRoot -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    do {
        if ($runtimeProcess.HasExited) {
            throw "foobar2000 exited during warm-up: $RuntimeRoot"
        }
        $runtimeProcess.Refresh()
        $componentLoaded = @($runtimeProcess.Modules | Where-Object {
            $_.ModuleName -eq 'foo_metadata_isolation.dll'
        }).Count -eq 1
        if ($runtimeProcess.MainWindowHandle -ne 0 -and $componentLoaded) { break }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    if ($runtimeProcess.MainWindowHandle -eq 0 -or -not $componentLoaded) {
        throw "foobar2000 did not finish component warm-up: $RuntimeRoot"
    }

    Start-Process -FilePath $exe -WorkingDirectory $RuntimeRoot -WindowStyle Hidden `
        -ArgumentList @(
            '/metadata-isolation-probe-physical-audit-classifier',
            ('"' + $source + '"')
        ) | Out-Null

    $deadline = [DateTime]::UtcNow.AddSeconds(60)
    $previousLength = -1L
    $stable = 0
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $report) {
            $length = (Get-Item -LiteralPath $report).Length
            if ($length -gt 0 -and $length -eq $previousLength) { ++$stable }
            else { $stable = 0 }
            if ($stable -ge 3) { break }
            $previousLength = $length
        }
        Start-Sleep -Milliseconds 100
    }
    if ($stable -lt 3) {
        throw "Timed out waiting for audit classifier report: $report"
    }

    $after = (Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash
    if (-not [string]::Equals($before, $after, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Read-only audit classifier modified its source fixture: $source"
    }

    & $Python tests/assert_physical_metadata_audit_runtime.py $source
    if ($LASTEXITCODE) { throw 'Audit classifier runtime oracle failed' }

    Write-Host "Physical Metadata Audit classifier $Architecture PASS; source SHA-256 unchanged: $after"
} finally {
    if ($runtimeProcess -and -not $runtimeProcess.HasExited) {
        $runtimeProcess.Refresh()
        if ($runtimeProcess.MainWindowHandle -ne 0) {
            $null = $runtimeProcess.CloseMainWindow()
        }
        if (-not $runtimeProcess.WaitForExit(15000)) {
            Stop-Process -Id $runtimeProcess.Id -Force
        }
    }
}
