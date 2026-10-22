[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Win32','x64')][string]$Architecture,
    [Parameter(Mandatory)][string]$ComponentDll,
    [switch]$RunLargeBenchmark,
    [switch]$RunFullLength111Mix,
    [switch]$RunDirectGuard,
    [switch]$RunPhysicalMetadata,
    [switch]$RunVirtualReplayGain,
    [switch]$RunCueTiming,
    [switch]$RunUiContracts,
    [switch]$RunRealAuditUia,
    [switch]$RunRealTimingUia,
    [string]$CompanionDiscoggerDll,
    [string]$CompanionDiscoggerConfigDb
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
$installer = $null
$process = $null
$priorDiscoggerQaDir = [Environment]::GetEnvironmentVariable('DISCOGGER_QA_MEDIA_DIR','Process')
$discoggerQaDir = $null
$discoggerQaMp3 = $null
$discoggerQaBefore = $null

try {
    $null = New-Item -ItemType Directory -Path $root
    $component = (Resolve-Path -LiteralPath $ComponentDll -ErrorAction Stop).Path
    if ([IO.Path]::GetFileName($component) -cne 'foo_metadata_isolation.dll') {
        throw 'Unexpected test-only DLL name'
    }
    # Use only the already-existing owner-hosted byte-identical ORIGINAL
    # foobar setup asset, verified independently against the exact Microsoft
    # winget SHA-256 pin. The transport code is immutable, from the separate
    # reviewed public infrastructure revision that passed both real hosts.
    # No new installer uploads, caches shared across jobs, modified builds,
    # third-party download mirrors or user-machine filesystem access.
    $transport = Join-Path (Get-Location).Path '.ci_installer/.github/actions/foobar-host-smoke/pinned-installer-cache.ps1'
    if (-not (Test-Path -LiteralPath $transport -PathType Leaf)) {
        throw 'Immutable shared official-installer transport source missing'
    }
    . $transport
    $filename = if ($Architecture -eq 'Win32') {
        'foobar2000_v2.26.1.exe'
    } else {
        'foobar2000-x64_v2.26.1.exe'
    }
    $trustedAsset = 'https://github.com/lxsdd/dev-infrastructure-public/releases/download/foobar-ci-upstream-v2.26.1/' + $filename
    $installer = Get-FoobarPinnedInstaller -Architecture $Architecture -Version '2.26.1' -Url $spec.url -Sha256 $spec.hash -CacheRoot $env:RUNNER_TEMP -MirrorUrl $trustedAsset
    if ((Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash -ine $spec.hash) {
        throw 'Original foobar installer transport failed immutable SHA-256 verification'
    }
    Write-Host "REAL_E2E_FOOBAR_PINNED_ORIGINAL_INSTALLER_$Architecture=PASS"
    $installed = Start-Process -FilePath $installer -ArgumentList @('/S',"/D=$portable") -PassThru -Wait
    if ($installed.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $exe)) {
        throw 'Isolated official foobar install failed'
    }
    $null = New-Item -ItemType File -Path (Join-Path $portable 'portable_mode_enabled')
    $components = Join-Path $portable 'components'
    $null = New-Item -ItemType Directory -Force -Path $components
    Copy-Item -LiteralPath $component -Destination (Join-Path $components 'foo_metadata_isolation.dll')

    $discoggerCoLoad = -not [string]::IsNullOrWhiteSpace($CompanionDiscoggerDll)
    if ($discoggerCoLoad) {
        if ([string]::IsNullOrWhiteSpace($CompanionDiscoggerConfigDb)) {
            throw 'Modified Discogger co-load requires its independently sourced SQLite sidecar'
        }
        $draftDll = (Resolve-Path -LiteralPath $CompanionDiscoggerDll -ErrorAction Stop).Path
        $draftDb = (Resolve-Path -LiteralPath $CompanionDiscoggerConfigDb -ErrorAction Stop).Path
        if ([IO.Path]::GetFileName($draftDll) -cne 'foo_discogger.dll' -or
            [IO.Path]::GetFileName($draftDb) -cne 'foo_discogger.dll.db' -or
            -not $draftDll.StartsWith($env:RUNNER_TEMP.TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
            -not $draftDb.StartsWith((Resolve-Path -LiteralPath (Join-Path (Get-Location) 'external/discogger-draft')).Path+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
            throw 'Co-load expects the pinned modified Discogger DLL from the disposable runner and the exact checked-out source SQLite sidecar'
        }
        $dbSignature = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($draftDb),0,16)
        if ($dbSignature -cne ("SQLite format 3"+[char]0)) {
            throw 'Modified Discogger source-side configuration DB is not SQLite'
        }
        Copy-Item -LiteralPath $draftDll -Destination (Join-Path $components 'foo_discogger.dll')
        $userDir = if($Architecture -eq 'Win32'){'user-components'}else{'user-components-x64'}
        foreach($profileRoot in @($portable,(Join-Path $portable 'profile'))) {
            $sidecarPath = Join-Path $profileRoot ($userDir+'/foo_discogger/foo_discogger.dll.db')
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $sidecarPath) | Out-Null
            Copy-Item -LiteralPath $draftDb -Destination $sidecarPath
            New-Item -ItemType Directory -Force -Path (Join-Path $profileRoot 'configuration') | Out-Null
        }
        Write-Host ('DISCOGGER_MODIFIED_CI_SOURCE_SHA=' + $env:DISCOGGER_MODIFIED_SOURCE_SHA)
        Write-Host ('DISCOGGER_MODIFIED_CI_BINARY_SHA256=' + (Get-FileHash -LiteralPath $draftDll -Algorithm SHA256).Hash)

        # Discogger's genuine SDK file_info_manager runner-only writer needs
        # its own separate synthetic MP3. It cannot reach user files or OAuth.
        $discoggerQaDir = Join-Path $root 'discogger-physical-sdk-qa'
        $null = New-Item -ItemType Directory -Path $discoggerQaDir
        $discoggerQaMp3 = Join-Path $discoggerQaDir 'synthetic-metadata.mp3'
        & python 'external/discogger-draft/tests/create-synthetic-mp3.py' $discoggerQaMp3
        if ($LASTEXITCODE -ne 0) { throw 'Discogger pinned synthetic MP3 generator failed' }
        $discoggerQaBefore = Join-Path $discoggerQaDir 'synthetic-metadata.mp3.before'
        Copy-Item -LiteralPath $discoggerQaMp3 -Destination $discoggerQaBefore
        [Environment]::SetEnvironmentVariable('DISCOGGER_QA_MEDIA_DIR',$discoggerQaDir,'Process')
    }

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
    if ($discoggerCoLoad) {
        $discoggerReady = $false
        for ($try = 0; $try -lt 50; $try++) {
            $process.Refresh()
            if ($process.HasExited) { throw 'Two-component foobar host terminated' }
            try {
                $seen = @((Get-Process -Id $process.Id -ErrorAction Stop).Modules |
                    Where-Object { $_.ModuleName -ieq 'foo_discogger.dll' -and
                        $_.FileName.StartsWith($portable,[StringComparison]::OrdinalIgnoreCase) })
                if ($seen.Count -eq 1) { $discoggerReady = $true; break }
            } catch [System.ComponentModel.Win32Exception] {}
            Start-Sleep -Milliseconds 250
        }
        if (-not $discoggerReady) { throw 'Original host did not load the modified Discogger DLL' }
        # A loaded DLL does not mean its asynchronous startup CONFIG.load()
        # has completed. The original Discogger GUI host test waits before
        # checking this SQLite side effect; use a bounded non-mutating poll.
        $configReady = $false
        for($configPoll=0;$configPoll -lt 80;$configPoll++){
            $process.Refresh()
            if($process.HasExited){throw 'Two-component foobar exited during Discogger initialization'}
            foreach($profileRoot in @($portable,(Join-Path $portable 'profile'))) {
                $actualDb = Join-Path $profileRoot 'configuration/foo_discogger.dll.db'
                if(Test-Path -LiteralPath $actualDb -PathType Leaf) {
                    $dbStream = $null
                    try {
                        $dbStream=[IO.File]::Open($actualDb,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
                        if($dbStream.Length -ge 16) {
                            $head=New-Object byte[] 16
                            if($dbStream.Read($head,0,16) -eq 16 -and
                                [Text.Encoding]::ASCII.GetString($head) -ceq ("SQLite format 3"+[char]0)) {
                                $configReady=$true
                            }
                        }
                    } catch [IO.IOException] {
                        # Writer still publishing the runner-local SQLite DB.
                    } finally {
                        if($null -ne $dbStream){$dbStream.Dispose()}
                    }
                }
            }
            if($configReady){break}
            Start-Sleep -Milliseconds 250
        }
        if(-not $configReady) {
            throw 'Modified Discogger failed bounded portable SQLite initialization; no co-load PASS'
        }
        Write-Host "MODIFIED_DISCOGGER_AND_METADATA_ISOLATION_HOST_COLOAD_$Architecture=PASS"
    }

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

    if ($discoggerCoLoad) {
        # Only inspect original production context menu entries. Do not run
        # Discogger, trigger OAuth, save art, or perform tag commits.
        $menuReport = Invoke-IsolatedProbe '/metadata-isolation-probe-context-tree-physical'
        $menuText = Get-Content -LiteralPath $menuReport -Raw
        if ($menuText -notmatch '(?m)^write_result=context_tree_listed\s*$' -or
            $menuText -notmatch '(?im)^context_command=.*(Discogger|Write tags\.\.\.)') {
            throw 'Modified Discogger physical tagging command missing from original host context menu'
        }
        Remove-Item -LiteralPath $menuReport -Force
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $before -Algorithm SHA256).Hash) {
            throw 'Modified Discogger context enumeration altered synthetic embedded-Cue media'
        }
        Write-Host "MODIFIED_DISCOGGER_PHYSICAL_CONTEXT_READ_ONLY_$Architecture=PASS"

        # Actual Discogger-owned physical write, not the Metadata Isolation
        # diagnostic writer: exercise the checked-out DLL's runner-only native
        # SDK file_info_manager, with both actual components loaded.
        # This is not an authenticated Discogs search or original GUI write.
        $qaCompletion = Join-Path $discoggerQaDir 'qa-mp3-completed.txt'
        if (Test-Path -LiteralPath $qaCompletion) {
            throw 'Discogger co-load physical write completion marker already exists'
        }
        $qaLauncher = Start-Process -FilePath $exe -WorkingDirectory $portable -ArgumentList '/command:"Discogger QA MP3 write"' -PassThru
        if (-not $qaLauncher.WaitForExit(15000) -or $qaLauncher.ExitCode -ne 0) {
            throw 'Original foobar failed to dispatch real runner-only Discogger SDK writer'
        }
        $qaConfirmed = $false
        for ($qaPoll = 0; $qaPoll -lt 160; $qaPoll++) {
            if (Test-Path -LiteralPath $qaCompletion -PathType Leaf) {
                $qaCount = (Get-Content -LiteralPath $qaCompletion -Raw).Trim()
                if ($qaCount -ceq '1') {
                    $qaConfirmed = $true
                    break
                }
                if ($qaCount.Length -gt 0) {
                    throw 'Discogger SDK physical completion marker is invalid'
                }
            }
            $process.Refresh()
            if ($process.HasExited) {
                throw 'Original foobar exited before Discogger SDK physical writer completed'
            }
            Start-Sleep -Milliseconds 250
        }
        if (-not $qaConfirmed) {
            throw 'No genuine Discogger SDK physical completion callback; no positive PASS'
        }
        & python 'external/discogger-draft/tests/verify-runner-physical-tags.py' $discoggerQaBefore $discoggerQaMp3
        if ($LASTEXITCODE -ne 0) {
            throw 'Independent original Discogger MP3 physical tag/APIC/audio preservation oracle failed'
        }
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $before -Algorithm SHA256).Hash) {
            throw 'Discogger-owned test write unexpectedly changed the separate embedded-Cue source'
        }
        Write-Host "MODIFIED_DISCOGGER_NATIVE_SDK_PHYSICAL_MP3_WRITE_COLOAD_$Architecture=PASS"
    }

    # This calls the real C++ production assess_physical_metadata() from the
    # isolated original foobar host, not a reimplemented Python classifier.
    # Exercise the REVIEW-versus-INCOMPLETE/EMPTY regression on an unchanged
    # disposable embedded-CUE MP3 before any synthetic write scenarios.
    $classifierReport = Invoke-IsolatedProbe '/metadata-isolation-probe-physical-audit-classifier'
    & python tests/assert_physical_audit_classifier.py $classifierReport $before $source
    if ($LASTEXITCODE -ne 0) { throw 'Real native physical audit classifier regression oracle failed' }
    Remove-Item -LiteralPath $classifierReport -Force
    Write-Host "REAL_HOST_PHYSICAL_AUDIT_CLASSIFIER_$Architecture=PASS"

    # Exercise the actual optimized C++ map_to_physical_targets() in the
    # original host. Every scenario stays read-only and restores the existing
    # report-file contract before the next isolated probe.
    $mappingCases = @(
        @{ scenario='single'; token='/metadata-isolation-probe-map-physical' },
        @{ scenario='duplicate'; token='/metadata-isolation-probe-map-physical-duplicate' },
        @{ scenario='case-alias'; token='/metadata-isolation-probe-map-physical-case-alias' },
        @{ scenario='all-subsongs'; token='/metadata-isolation-probe-map-all-handles-physical' },
        @{ scenario='remote-rejected'; token='/metadata-isolation-probe-map-physical-remote' },
        @{ scenario='remote-first-rejected'; token='/metadata-isolation-probe-map-physical-remote-first' }
    )
    foreach ($mappingCase in $mappingCases) {
        $mapReport = Invoke-IsolatedProbe $mappingCase.token
        & python tests/assert_physical_target_mapping.py $mapReport $before $source --scenario $mappingCase.scenario
        if ($LASTEXITCODE -ne 0) {
            throw "Original foobar physical-source mapper regression: $($mappingCase.scenario)"
        }
        Remove-Item -LiteralPath $mapReport -Force
    }
    Write-Host "REAL_HOST_PHYSICAL_TARGET_MAPPING_MATRIX_$Architecture=PASS"

    # A separate source-only fixture matrix exercises real path comparison
    # and first-appearance order for 111 distinct local physical files.
    # Copies never leave RUNNER_TEMP and none of the mapper actions writes.
    $originalSource = $source
    $originalBefore = $before
    $manyDir = Join-Path $root 'physical-111-distinct'
    $null = New-Item -ItemType Directory -Path $manyDir
    $source = Join-Path $manyDir 'track-01.mp3'
    $before = Join-Path $manyDir 'track-01.mp3.before'
    Copy-Item -LiteralPath $originalSource -Destination $source
    Copy-Item -LiteralPath $source -Destination $before
    for ($index = 2; $index -le 111; $index++) {
        $newLeaf = ('track-{0:D2}.mp3' -f $index)
        Copy-Item -LiteralPath $source -Destination (Join-Path $manyDir $newLeaf)
    }
    $manyReport = Invoke-IsolatedProbe '/metadata-isolation-probe-map-111-distinct-physical'
    & python tests/assert_physical_target_mapping.py $manyReport $before $source --scenario many-distinct
    if ($LASTEXITCODE -ne 0) { throw 'Native 111-source physical mapping regression oracle failed' }
    Remove-Item -LiteralPath $manyReport -Force
    $source = $originalSource
    $before = $originalBefore
    Write-Host "REAL_HOST_PHYSICAL_111_DISTINCT_TARGETS_$Architecture=PASS"

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

    if ($RunFullLength111Mix) {
        # No owner's copyrighted media. Deterministic 320 kbit/s synthetic
        # MPEG-1 Layer III, approx. 84 minutes / 193 MiB, 111 CUE tracks.
        # Full-file audio hashes are streamed by the independent oracle.
        $caseDir = Join-Path $root 'full-length-111-synthetic-320k'
        $source = Join-Path $caseDir 'track-01.mp3'
        $before = Join-Path $caseDir 'track-01.mp3.before'
        & python tests/make_full_length_111_mix.py $source
        if ($LASTEXITCODE -ne 0) { throw 'Full-length synthetic 111-track Mix generation failed' }
        $size = (Get-Item -LiteralPath $source).Length
        if ($size -lt 180000000 -or $size -gt 240000000) {
            throw 'Full-length synthetic Mix outside verified size bounds'
        }
        Copy-Item -LiteralPath $source -Destination $before -ErrorAction Stop
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -cne
            (Get-FileHash -LiteralPath $before -Algorithm SHA256).Hash) {
            throw 'Full-length synthetic Mix baseline copy changed'
        }
        $inventory = Invoke-IsolatedProbe '/metadata-isolation-probe-embedded-cue-inventory'
        $inventoryText = Get-Content -LiteralPath $inventory -Raw
        if ($inventoryText -notmatch '(?m)^inventory_subsong_count=111[ \t\r\n]*$') {
            throw 'Real foobar host did not expose 111 full-length virtual tracks'
        }
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -cne
            (Get-FileHash -LiteralPath $before -Algorithm SHA256).Hash) {
            throw 'Full-length 111-track read-only host inventory changed source'
        }
        Remove-Item -LiteralPath $inventory -Force
        Write-Host "REAL_HOST_FULL_LENGTH_111_INVENTORY_$Architecture=PASS;Bytes=$size"
        $report = Invoke-IsolatedProbe '/metadata-isolation-probe-projection-benchmark'
        & python tests/assert_full_length_111_mix.py $before $source $report
        if ($LASTEXITCODE -ne 0) {
            throw "Full-length 111-track audio/CUE/ID3 streaming oracle failed ($Architecture)"
        }
        Write-Host "REAL_HOST_FULL_LENGTH_111_VIRTUAL_COMMIT_$Architecture=PASS"
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
    if ($RunCueTiming) {
        foreach ($test in @('index01-change','index01-invalid')) {
            $caseDir = Join-Path $root ('cue-timing-' + $test)
            $source = Join-Path $caseDir 'track-01.mp3'
            $before = Join-Path $caseDir 'track-01.mp3.before'
            & python tests/make_isolated_timing_mp3.py $source
            if ($LASTEXITCODE -ne 0) { throw "Timing fixture creation failed: $test" }
            Copy-Item -LiteralPath $source -Destination $before
            $inventory = Invoke-IsolatedProbe '/metadata-isolation-probe-embedded-cue-inventory'
            $text = Get-Content -LiteralPath $inventory -Raw
            $lines = @($text.Split([char]10) | ForEach-Object { $_.Trim() })
            if (-not ($lines -ccontains 'inventory_subsong_count=3')) {
                throw "foobar did not expose 3 embedded Cue subsongs: $test"
            }
            if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne
                (Get-FileHash -LiteralPath $before -Algorithm SHA256).Hash) {
                throw "Cue inventory changed source: $test"
            }
            Remove-Item -LiteralPath $inventory -Force
            $report = Invoke-IsolatedProbe ('/metadata-isolation-probe-production-timing-' + $test)
            Get-Content -LiteralPath $report | Select-Object -First 70 | ForEach-Object { Write-Host "TIMING_PROBE: $_" }
            & python tests/assert_isolated_timing_write.py $before $source $report --scenario $test
            if ($LASTEXITCODE -ne 0) { throw "Timing postimage validation failed: $test" }
            Write-Host ("REAL_HOST_CUE_TIMING_" + $Architecture + "=" + $test + ":PASS")
        }
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

    if ($RunUiContracts) {
        $caseDir = Join-Path $root 'ui-contracts'
        $source = Join-Path $caseDir 'track-01.mp3'
        $before = Join-Path $caseDir 'track-01.mp3.before'
        & python tests/make_isolated_embedded_mp3.py $source
        if ($LASTEXITCODE -ne 0) { throw 'UI contract fixture generation failed' }
        Copy-Item -LiteralPath $source -Destination $before
        foreach ($scenario in @('logging','tooltip-light','tooltip-dark')) {
            $command = switch ($scenario) {
                'logging' { '/metadata-isolation-probe-logging-contract' }
                'tooltip-light' { '/metadata-isolation-probe-tooltip-contract' }
                'tooltip-dark' { '/metadata-isolation-probe-tooltip-contract-dark' }
            }
            $report = Invoke-IsolatedProbe $command
            if ($scenario -like "tooltip-*") { Get-Content -LiteralPath $report | Select-Object -First 45 | ForEach-Object { Write-Host "TOOLTIP_PROBE: $_" } }
            & python tests/assert_isolated_ux_contracts.py $report --scenario $scenario
            if ($LASTEXITCODE -ne 0) { throw "Native UI contract failed: $scenario" }
            Remove-Item -LiteralPath $report -Force
            if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne
                (Get-FileHash -LiteralPath $before -Algorithm SHA256).Hash) {
                throw "UI contract altered disposable audio: $scenario"
            }
            Write-Host ('REAL_HOST_UI_' + $Architecture + '=' + $scenario + ':PASS')
        }
    }

    if ($RunRealAuditUia) {
        $caseDir = Join-Path $root 'real-audit-ui'
        $source = Join-Path $caseDir 'track-01.mp3'
        & python tests/make_isolated_embedded_mp3.py $source
        if ($LASTEXITCODE -ne 0) { throw 'Real Audit UI fixture generation failed' }
        $adapter = Join-Path (Get-Location).Path '.ci_shared/scripts/windows-ui-accessibility.ps1'
        & ./tests/assert_real_audit_uia.ps1 -FoobarPid $process.Id -Executable $exe -Workdir $portable -FixturePath $source -AdapterPath $adapter
        Write-Host "REAL_HOST_PRODUCTION_AUDIT_UIA_$Architecture=PASS"
    }

    if ($RunRealTimingUia) {
        $caseDir = Join-Path $root 'real-virtual-timing-ui'
        $source = Join-Path $caseDir 'track-01.mp3'
        & python tests/make_isolated_timing_mp3.py $source
        if ($LASTEXITCODE -ne 0) { throw 'Real Virtual timing UI fixture creation failed' }
        $adapter = Join-Path (Get-Location).Path '.ci_shared/scripts/windows-ui-accessibility.ps1'
        & ./tests/assert_real_virtual_timing_uia.ps1 -FoobarPid $process.Id -Executable $exe -Workdir $portable -FixturePath $source -AdapterPath $adapter
        Write-Host ('REAL_HOST_PRODUCTION_VIRTUAL_TIMING_UIA_' + $Architecture + '=PASS')
    }

} finally {
    [Environment]::SetEnvironmentVariable('DISCOGGER_QA_MEDIA_DIR',$priorDiscoggerQaDir,'Process')
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
