[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Win32','x64')][string]$Architecture,
    [Parameter(Mandatory)][string]$ComponentDll,
    [switch]$RunLargeBenchmark,
    [switch]$RunDirectGuard,
    [switch]$RunPhysicalMetadata,
    [switch]$RunVirtualReplayGain
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($env:GITHUB_ACTIONS -ne 'true' -or [string]::IsNullOrWhiteSpace($env:RUNNER_TEMP)) {
    throw 'Only disposable GitHub Actions runners are allowed to execute this write test'
}
if (@(Get-Process -Name foobar2000 -ErrorAction SilentlyContinue).Count -gt 0) {
    throw 'An unrelated foobar process is running. Refusing test.'
}
$specs = @{
    Win32 = @{ url='https://www.foobar2000.org/files/foobar2000_v2.26.1.exe'; hash='849BC4424326DA81049746E9A2255A3A707E56B76CC69ED11714ED23C69ECE52' }
    x64 = @{ url='https://www.foobar2000.org/files/foobar2000-x64_v2.26.1.exe'; hash='72FE15A88B085EE4E49D68114E2BB9655000C2E0FDF40DCC128BBEF8BCAF3600' }
}
$spec = $specs[$Architecture]
$root = Join-Path $env:RUNNER_TEMP ('mi-virtual-write-'+$Architecture+'-'+[Guid]::NewGuid().ToString('N'))
$portable = Join-Path $root 'foobar'
$exe = Join-Path $portable 'foobar2000.exe'
$source = Join-Path $root 'track-01.mp3'
$before = Join-Path $root 'track-01.mp3.before'
$installer = Join-Path $root 'official-installer.exe'
$process = $null

try {
    $null = New-Item -ItemType Directory -Path $root
    $component = (Resolve-Path -LiteralPath $ComponentDll -ErrorAction Stop).Path
    if ([IO.Path]::GetFileName($component) -cne 'foo_metadata_isolation.dll') {
        throw 'Unexpected test-only DLL name'
    }
    Invoke-WebRequest -Uri $spec.url -OutFile $installer
    if ((Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash -ine $spec.hash) {
        throw 'Official foobar installer hash mismatch'
    }
    $installed = Start-Process -FilePath $installer -ArgumentList @('/S',"/D=$portable") -PassThru -Wait
    if ($installed.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $exe)) {
        throw 'Isolated official foobar install failed'
    }
    $null = New-Item -ItemType File -Path (Join-Path $portable 'portable_mode_enabled')
    $components = Join-Path $portable 'components'
    $null = New-Item -ItemType Directory -Force -Path $components
    Copy-Item -LiteralPath $component -Destination (Join-Path $components 'foo_metadata_isolation.dll')

    & python tests/make_isolated_embedded_mp3.py $source
    if ($LASTEXITCODE -ne 0) { throw 'Synthetic embedded-Cue fixture generation failed' }
    Copy-Item -LiteralPath $source -Destination $before

    $process = Start-Process -FilePath $exe -WorkingDirectory $portable -PassThru
    $ready = $false
    for ($i=0; $i -lt 70; $i++) {
        Start-Sleep -Milliseconds 500
        $process.Refresh()
        if ($process.HasExited) { throw 'Real foobar host crashed' }
        try {
            $modules = @((Get-Process -Id $process.Id -ErrorAction Stop).Modules)
            $found = @($modules | Where-Object {
                $_.ModuleName -ieq 'foo_metadata_isolation.dll' -and
                $_.FileName.StartsWith($portable,[StringComparison]::OrdinalIgnoreCase)
            })
            if ($found.Count -eq 1) { $ready = $true; break }
        } catch [System.ComponentModel.Win32Exception] { }
    }
    if (-not $ready) { throw 'Test-only DLL not loaded in the actual foobar host' }
    Write-Host "PROBE_COMPONENT_LOAD_$Architecture=PASS"

    function Invoke-IsolatedProbe([string]$token) {
        $report = "$source.metadata-isolation-probe.txt"
        if (Test-Path -LiteralPath $report) { throw "Refusing stale report: $token" }
        $launcher = Start-Process -FilePath $exe -WorkingDirectory $portable -ArgumentList @($token,('"' + $source + '"')) -PassThru
        if (-not $launcher.WaitForExit(20000)) { throw "Probe launcher timeout: $token" }
        if ($launcher.ExitCode -ne 0) { throw "Probe launcher failed: $token" }
        $last = -1L
        $stable = 0
        for ($i=0; $i -lt 1200; $i++) {
            if (Test-Path -LiteralPath $report) {
                $length = (Get-Item -LiteralPath $report).Length
                if ($length -gt 0 -and $length -eq $last) { $stable++ } else { $stable = 0 }
                if ($stable -ge 4) { break }
                $last = $length
            }
            Start-Sleep -Milliseconds 100
            $process.Refresh()
            if ($process.HasExited) { throw "Host exited during $token" }
        }
        if ($stable -lt 4) { throw "Probe report timeout: $token" }
        return $report
    }

    $inventory = Invoke-IsolatedProbe '/metadata-isolation-probe-embedded-cue-inventory'
    $inventoryText = Get-Content -LiteralPath $inventory -Raw
    if ($inventoryText -notmatch '(?m)^inventory_subsong_count=2\s*$') {
        throw "Fixture was not exposed as two embedded virtual tracks: $inventoryText"
    }
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $before -Algorithm SHA256).Hash) {
        throw 'Read-only inventory changed the physical MP3'
    }
    Remove-Item -LiteralPath $inventory -Force
    Write-Host "EMBEDDED_CUE_INVENTORY_$Architecture=PASS"

    $report = Invoke-IsolatedProbe '/metadata-isolation-probe-production-shadow-title'
    & python tests/assert_isolated_virtual_write.py $before $source $report
    if ($LASTEXITCODE -ne 0) { throw 'Independent virtual-write oracle failed' }
    Write-Host "REAL_HOST_VIRTUAL_TITLE_WRITE_$Architecture=PASS"

    # New, completely separate original fixture for each safety outcome.
    # No output from an earlier scenario can influence any later case.
    $cases = @(
        @{ name='partial-style'; token='/metadata-isolation-probe-production-shadow-partial-style' },
        @{ name='strict'; token='/metadata-isolation-probe-production-shadow-partial-style-strict' },
        @{ name='blocking'; token='/metadata-isolation-probe-production-shadow-mixed' },
        @{ name='rollback'; token='/metadata-isolation-probe-production-shadow-rollback' }
    )
    foreach ($case in $cases) {
        $caseDir = Join-Path $root $case.name
        $source = Join-Path $caseDir 'track-01.mp3'
        $before = Join-Path $caseDir 'track-01.mp3.before'
        & python tests/make_isolated_embedded_mp3.py $source
        if ($LASTEXITCODE -ne 0) { throw "Fixture generation failed for $($case.name)" }
        Copy-Item -LiteralPath $source -Destination $before
        $inventory = Invoke-IsolatedProbe '/metadata-isolation-probe-embedded-cue-inventory'
        $inventoryText = Get-Content -LiteralPath $inventory -Raw
        if ($inventoryText -notmatch '(?m)^inventory_subsong_count=2[ \t\r\n]*$') {
            throw "Fixture not exposed as two tracks for $($case.name)"
        }
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $before -Algorithm SHA256).Hash) {
            throw "Read-only inventory modified $($case.name)"
        }
        Remove-Item -LiteralPath $inventory -Force
        $report = Invoke-IsolatedProbe $case.token
        & python tests/assert_isolated_virtual_write.py $before $source $report --scenario $case.name
        if ($LASTEXITCODE -ne 0) {
            throw "Independent scenario oracle failed: $($case.name)"
        }
        Write-Host "REAL_HOST_$($case.name.ToUpperInvariant())_$Architecture=PASS"
    }
    if ($RunLargeBenchmark) {
        # Small and larger physical audio payload, same 111 virtual tracks.
        # Observe scaling without accessing user media or asserting fake latency.
        foreach ($repeats in @(160, 7000)) {
            $caseDir = Join-Path $root ("benchmark-111-" + $repeats)
            $source = Join-Path $caseDir 'track-01.mp3'
            $before = Join-Path $caseDir 'track-01.mp3.before'
            & python tests/make_large_embedded_mp3.py $source --repeats $repeats
            if ($LASTEXITCODE -ne 0) { throw "111-track fixture failed: $repeats" }
            Copy-Item -LiteralPath $source -Destination $before
            $inventory = Invoke-IsolatedProbe '/metadata-isolation-probe-embedded-cue-inventory'
            $inventoryText = Get-Content -LiteralPath $inventory -Raw
            if ($inventoryText -notmatch '(?m)^inventory_subsong_count=111[ \t\r\n]*$') {
                throw "foobar did not expose 111 virtual Cue tracks: repeats=$repeats"
            }
            if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne
                (Get-FileHash -LiteralPath $before -Algorithm SHA256).Hash) {
                throw "111-track read-only inventory mutated source: repeats=$repeats"
            }
            Remove-Item -LiteralPath $inventory -Force
            Write-Host "EMBEDDED_CUE_111_INVENTORY_$Architecture=PASS repeats=$repeats"
            $report = Invoke-IsolatedProbe '/metadata-isolation-probe-projection-benchmark'
            & python tests/assert_large_virtual_benchmark.py $before $source $report
            if ($LASTEXITCODE -ne 0) { throw "111-track oracle failed: repeats=$repeats" }
            Write-Host "REAL_HOST_111_VIRTUAL_COMMIT_$Architecture=PASS repeats=$repeats"
        }
    }

    if ($RunVirtualReplayGain) {
        $caseDir = Join-Path $root 'virtual-replaygain'
        $source = Join-Path $caseDir 'track-01.mp3'
        $before = Join-Path $caseDir 'track-01.mp3.before'
        & python tests/make_isolated_embedded_mp3.py $source
        if ($LASTEXITCODE -ne 0) { throw 'ReplayGain fixture generation failed' }
        Copy-Item -LiteralPath $source -Destination $before
        $inventory = Invoke-IsolatedProbe '/metadata-isolation-probe-embedded-cue-inventory'
        $inventoryText = Get-Content -LiteralPath $inventory -Raw
        $inventoryLines = @($inventoryText.Split([char]10) | ForEach-Object { $_.Trim() })
        if (-not ($inventoryLines -ccontains 'inventory_subsong_count=2')) {
            throw 'ReplayGain fixture did not expose two virtual tracks'
        }
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $before -Algorithm SHA256).Hash) {
            throw 'ReplayGain fixture mutated by read-only inventory'
        }
        Remove-Item -LiteralPath $inventory -Force
        $report = Invoke-IsolatedProbe '/metadata-isolation-probe-production-shadow-replaygain'
        & python tests/assert_isolated_replaygain_write.py $before $source $report
        if ($LASTEXITCODE -ne 0) { throw 'Independent Virtual ReplayGain oracle failed' }
        Write-Host "REAL_HOST_VIRTUAL_REPLAYGAIN_$Architecture=PASS"
    }
    if ($RunPhysicalMetadata) {
        # Positive counterexample: physical tag writes must remain possible.
        $caseDir = Join-Path $root 'physical-title'
        $source = Join-Path $caseDir 'track-01.mp3'
        $before = Join-Path $caseDir 'track-01.mp3.before'
        & python tests/make_isolated_embedded_mp3.py $source
        if ($LASTEXITCODE -ne 0) { throw 'Physical fixture generation failed' }
        Copy-Item -LiteralPath $source -Destination $before
        $inventory = Invoke-IsolatedProbe '/metadata-isolation-probe-embedded-cue-inventory'
        $inventoryText = Get-Content -LiteralPath $inventory -Raw
        $inventoryLines = @($inventoryText.Split([char]10) | ForEach-Object { $_.Trim() })
        if (-not ($inventoryLines -ccontains 'inventory_subsong_count=2')) {
            throw 'Physical fixture did not expose 2 virtual subsongs'
        }
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $before -Algorithm SHA256).Hash) {
            throw 'Physical fixture mutated by read-only inventory'
        }
        Remove-Item -LiteralPath $inventory -Force
        $report = Invoke-IsolatedProbe '/metadata-isolation-probe-physical-title'
        & python tests/assert_isolated_physical_write.py $before $source $report
        if ($LASTEXITCODE -ne 0) { throw 'Independent Physical writer oracle failed' }
        Write-Host "REAL_HOST_PHYSICAL_TITLE_WRITE_$Architecture=PASS"
    }
    if ($RunDirectGuard) {
        # Run last: direct host Guard message may be modal.
        $caseDir = Join-Path $root 'guard-direct'
        $source = Join-Path $caseDir 'track-01.mp3'
        $before = Join-Path $caseDir 'track-01.mp3.before'
        & python tests/make_isolated_embedded_mp3.py $source
        if ($LASTEXITCODE -ne 0) { throw 'Direct Guard fixture generation failed' }
        Copy-Item -LiteralPath $source -Destination $before
        [IO.File]::WriteAllText(($source + '.metadata-isolation-guard-fixture.txt'), 'MI-DISPOSABLE-FOOBAR-GUARD-V1', [Text.UTF8Encoding]::new($false))
        $inventory = Invoke-IsolatedProbe '/metadata-isolation-probe-embedded-cue-inventory'
        $inventoryText = Get-Content -LiteralPath $inventory -Raw
        $inventoryLines = @($inventoryText.Split([char]10) | ForEach-Object { $_.Trim() })
        if (-not ($inventoryLines -ccontains 'inventory_subsong_count=2')) {
            throw 'Direct Guard fixture did not expose exactly two tracks'
        }
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $before -Algorithm SHA256).Hash) {
            throw 'Direct Guard fixture mutated during inventory'
        }
        Remove-Item -LiteralPath $inventory -Force
        $report = Invoke-IsolatedProbe '/metadata-isolation-probe-direct-guard-virtual-title'
        Get-Content -LiteralPath $report | Select-Object -First 60 | ForEach-Object { Write-Host "GUARD_PROBE: $_" }
        & python tests/assert_isolated_write_guard.py $before $source $report
        if ($LASTEXITCODE -ne 0) { throw 'Direct Guard byte preservation failed' }
        Write-Host "REAL_HOST_DIRECT_VIRTUAL_WRITE_GUARD_$Architecture=PASS"
    }

} finally {
    if ($null -ne $process) {
        try {
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force -ErrorAction Stop
            }
        } catch {
            Write-Warning 'Could not stop disposable host process'
        }
    }
    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
    }
}
