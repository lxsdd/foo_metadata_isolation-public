[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][ValidateSet('x86', 'x64')][string]$Architecture,
    [Parameter(Mandatory = $true)][string]$RuntimeRoot,
    [Parameter(Mandatory = $true)][string]$FixtureRoot,
    [string]$Python = 'python'
)

$ErrorActionPreference = 'Stop'
$exe = Join-Path $RuntimeRoot 'foobar2000.exe'
if (-not (Test-Path -LiteralPath $exe) -or -not (Test-Path -LiteralPath (Join-Path $RuntimeRoot 'portable_mode_enabled'))) { throw 'Portable runtime is required' }
$work = Join-Path ([IO.Path]::GetFullPath($FixtureRoot)) "phase12-physical-$Architecture"
if (Test-Path -LiteralPath $work) { throw "Refusing to overwrite $work" }

function Invoke-Probe([string]$Token, [string[]]$Paths) {
    $report = "$($Paths[0]).metadata-isolation-probe.txt"
    if (Test-Path -LiteralPath $report) { throw "Stale report: $report" }
    Start-Process -FilePath $exe -WorkingDirectory $RuntimeRoot -WindowStyle Hidden -ArgumentList (@($Token) + @($Paths | ForEach-Object { '"' + $_ + '"' })) | Out-Null
    $deadline = [DateTime]::UtcNow.AddSeconds(60)
    $previousLength = -1L
    $stable = 0
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $report) {
            $length = (Get-Item -LiteralPath $report).Length
            if ($length -gt 0 -and $length -eq $previousLength) { ++$stable } else { $stable = 0 }
            if ($stable -ge 3) { break }
            $previousLength = $length
        }
        Start-Sleep -Milliseconds 100
    }
    if ($stable -lt 3) { throw "Timeout waiting for stable report: $Token" }
    $text = Get-Content -LiteralPath $report -Raw
    if ($text -match 'write_result=error') { throw "Probe failed: $Token`n$text" }
    return $report
}
function Test-Oracle(
    [string]$Before,
    [string]$After,
    [string]$Format,
    [string]$Expect,
    [string]$Report,
    [int]$TargetIndex,
    [switch]$Native) {
    $args = @('tests/assert_phase12_physical_oracle.py', $Before, $After, '--format', $Format, '--expect', $Expect)
    if ($Native) { $args += '--native' }
    $args += @('--report', $Report, '--target-index', $TargetIndex)
    & $Python @args
    if ($LASTEXITCODE) { throw "Oracle failed: $After" }
}
function Assert-Mapping([string]$Report, [int]$Targets) {
    $text = Get-Content -LiteralPath $Report -Raw
    if ($text -notmatch "mapping_target_count=$Targets(?:\r?\n)" -or
        $text -notmatch 'mapping_rejected_count=0(?:\r?\n)' -or
        $text -notmatch 'mapping_complete=true(?:\r?\n)') {
        throw "Unexpected Physical mapping report: $Report`n$text"
    }
    $subsongZero = [regex]::Matches($text, 'mapping_target_\d+_subsong=0(?:\r?\n)').Count
    if ($subsongZero -ne $Targets) { throw "Expected $Targets subsong-0 targets; found $subsongZero" }
}

try {
    & $Python tests/make_phase12_physical_fixtures.py $work
    if ($LASTEXITCODE) { throw 'Fixture generation failed' }
    $fixtureDirectory = Join-Path $work 'Unicode-東京-Δ'
    $paths = @(
        Get-ChildItem -LiteralPath $fixtureDirectory -File |
        Where-Object { $_.Extension -in @('.mp3', '.flac') } |
        Sort-Object Name |
        ForEach-Object { $_.FullName }
    )
    if ($paths.Count -ne 6) { throw "Expected six Physical sources; found $($paths.Count)" }
    foreach ($path in $paths) { Copy-Item -LiteralPath $path -Destination "$path.before" }

    $mapReport = Invoke-Probe '/metadata-isolation-probe-map-all-handles-physical' $paths
    Assert-Mapping $mapReport $paths.Count
    Move-Item -LiteralPath $mapReport -Destination (Join-Path $work 'mapping.txt')

    $matrixReport = Invoke-Probe '/metadata-isolation-probe-physical-matrix' $paths
    Assert-Mapping $matrixReport $paths.Count
    for ($target = 0; $target -lt $paths.Count; ++$target) {
        $path = $paths[$target]
        $format = if ([IO.Path]::GetExtension($path) -eq '.mp3') { 'mp3' } else { 'flac' }
        Test-Oracle "$path.before" $path $format 'matrix' $matrixReport $target -Native:($path.EndsWith('physical-native.flac'))
        Copy-Item -LiteralPath $path -Destination "$path.pre-rg-remove"
    }
    Move-Item -LiteralPath $matrixReport -Destination (Join-Path $work 'matrix.txt')

    $removeReport = Invoke-Probe '/metadata-isolation-probe-physical-replaygain-remove' $paths
    Assert-Mapping $removeReport $paths.Count
    for ($target = 0; $target -lt $paths.Count; ++$target) {
        $path = $paths[$target]
        $format = if ([IO.Path]::GetExtension($path) -eq '.mp3') { 'mp3' } else { 'flac' }
        Test-Oracle "$path.pre-rg-remove" $path $format 'remove' $removeReport $target -Native:($path.EndsWith('physical-native.flac'))
    }
    Move-Item -LiteralPath $removeReport -Destination (Join-Path $work 'replaygain-remove.txt')
    Write-Host "Phase-12 physical harness completed: $work"
} finally {
    Start-Process -FilePath $exe -WorkingDirectory $RuntimeRoot -WindowStyle Hidden -ArgumentList '/exit' -Wait | Out-Null
}
