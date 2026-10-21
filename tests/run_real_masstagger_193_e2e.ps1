#requires -Version 7.2
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Win32','x64')][string]$Architecture,
    [Parameter(Mandatory)][string]$MetadataIsolationDll
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if($env:GITHUB_ACTIONS -cne 'true' -or -not $env:RUNNER_TEMP){
    throw 'Real Masstagger E2E is restricted to disposable Windows CI'
}
if(@(Get-Process foobar2000 -ErrorAction SilentlyContinue).Count -gt 0){
    throw 'Unrelated foobar2000 process detected; refuse testing'
}
$hashMasstagger='06a6abc97ede4516308acf803512549fbb714d6b388ba549c2212cb9fa417289'
$officialMasstagger='https://www.foobar2000.org/getcomponent/ba815d92944b4add846a924efa784394/foo_masstag.fb2k-component'
$foobar=@{
  Win32=@{url='https://www.foobar2000.org/files/foobar2000_v2.26.1.exe';hash='849BC4424326DA81049746E9A2255A3A707E56B76CC69ED11714ED23C69ECE52';filename='foobar2000_v2.26.1.exe';machine=0x014c;member='foo_masstag.dll'}
  x64=@{url='https://www.foobar2000.org/files/foobar2000-x64_v2.26.1.exe';hash='72FE15A88B085EE4E49D68114E2BB9655000C2E0FDF40DCC128BBEF8BCAF3600';filename='foobar2000-x64_v2.26.1.exe';machine=0x8664;member='x64/foo_masstag.dll'}
}[$Architecture]
$root=Join-Path $env:RUNNER_TEMP ('mi-masstagger-real-'+$Architecture+'-'+[guid]::NewGuid().ToString('N'))
$portable=Join-Path $root 'foobar'
$exe=Join-Path $portable 'foobar2000.exe'
$archive=Join-Path $root 'foo_masstag-original.fb2k-component'
$process=$null
try {
    [void](New-Item -ItemType Directory -Path $root)
    $ourDll=(Resolve-Path -LiteralPath $MetadataIsolationDll).Path
    if([IO.Path]::GetFileName($ourDll) -cne 'foo_metadata_isolation.dll'){
        throw 'Unexpected Metadata Isolation DLL input'
    }
    # Original current official package, independently SHA-pinned by
    # Windows original-source discovery job 39644486859. No third parties.
    Invoke-WebRequest -Uri $officialMasstagger -OutFile $archive -MaximumRedirection 0 -TimeoutSec 45
    if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -cne $hashMasstagger){
        throw 'Masstagger original package fails independent full SHA-256 pin'
    }
    Add-Type -AssemblyName System.IO.Compression
    $zip=[IO.Compression.ZipFile]::OpenRead($archive)
    try{
        $expected=@('foo_masstag.dll','x64/foo_masstag.dll','arm64ec/foo_masstag.dll')
        $names=@($zip.Entries | ForEach-Object {$_.FullName})
        if((@($names | Sort-Object) -join ';') -cne (@($expected | Sort-Object) -join ';')){
            throw 'Original Masstagger 1.9.3 archive contains unexpected/missing entries'
        }
        $member=$zip.GetEntry($foobar.member)
        if($null -eq $member -or $member.Length -gt 16MB -or $member.Length -lt 1024){
            throw 'Expected architecture-specific original DLL absent'
        }
        $targetDll=Join-Path $root 'foo_masstag.dll'
        $inputStream=$member.Open()
        try{
            $output=[IO.File]::Open($targetDll,[IO.FileMode]::CreateNew)
            try{$inputStream.CopyTo($output)}finally{$output.Dispose()}
        }finally{$inputStream.Dispose()}
        $dllBytes=[IO.File]::ReadAllBytes($targetDll)
        if($dllBytes.Length -lt 512 -or $dllBytes[0] -ne 0x4d -or $dllBytes[1] -ne 0x5a){
            throw 'Selected official component is not a Windows PE image'
        }
        $peOff=[BitConverter]::ToInt32($dllBytes,0x3c)
        if($peOff -lt 64 -or $peOff + 6 -gt $dllBytes.Length -or
           $dllBytes[$peOff] -ne 0x50 -or $dllBytes[$peOff+1] -ne 0x45 -or
           $dllBytes[$peOff+2] -ne 0 -or $dllBytes[$peOff+3] -ne 0){
            throw 'Invalid Masstagger Portable Executable signature'
        }
        if([BitConverter]::ToUInt16($dllBytes,$peOff+4) -ne $foobar.machine){
            throw 'Selected Masstagger DLL architecture mismatch'
        }
        Write-Host ("MASSTAGGER_ORIGINAL_193_PIN_AND_PE_$Architecture=PASS")
    }finally{$zip.Dispose()}
    . (Join-Path (Get-Location).Path '.ci_installer/.github/actions/foobar-host-smoke/pinned-installer-cache.ps1')
    $officialInstallerAsset='https://github.com/lxsdd/dev-infrastructure-public/releases/download/foobar-ci-upstream-v2.26.1/'+$foobar.filename
    $installer=Get-FoobarPinnedInstaller -Architecture $Architecture -Version '2.26.1' -Url $foobar.url -Sha256 $foobar.hash -CacheRoot $env:RUNNER_TEMP -MirrorUrl $officialInstallerAsset
    if((Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash -ine $foobar.hash){
        throw 'Original player installer hash mismatch'
    }
    $install=Start-Process -FilePath $installer -ArgumentList @('/S',"/D=$portable") -Wait -PassThru
    if($install.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $exe)){
        throw 'Disposable original player install failed'
    }
    [void](New-Item -ItemType File -Path (Join-Path $portable 'portable_mode_enabled'))
    $components=Join-Path $portable 'components'
    [void](New-Item -ItemType Directory -Path $components -Force)
    Copy-Item -LiteralPath $ourDll -Destination (Join-Path $components 'foo_metadata_isolation.dll')
    Copy-Item -LiteralPath $targetDll -Destination (Join-Path $components 'foo_masstag.dll')
    $mediaDir=Join-Path $root 'media'
    [void](New-Item -ItemType Directory -Path $mediaDir)
    $plain=Join-Path $mediaDir 'track-01.mp3'
    & python tests/make_plain_masstagger_mp3.py $plain
    if($LASTEXITCODE -ne 0){throw 'Independent copyright-free physical fixture failed'}
    $before=Join-Path $mediaDir 'track-01.before.mp3'
    Copy-Item -LiteralPath $plain -Destination $before
    $beforeHash=(Get-FileHash -LiteralPath $plain -Algorithm SHA256).Hash
    $process=Start-Process -FilePath $exe -WorkingDirectory $portable -PassThru
    $seenMetadata=$false
    $seenMasstagger=$false
    for($i=0;$i -lt 85;$i++){
        Start-Sleep -Milliseconds 450
        $process.Refresh()
        if($process.HasExited){throw 'Isolated foobar host crashed during component startup'}
        try{
            $mods=@((Get-Process -Id $process.Id).Modules)
            $seenMetadata=@($mods | Where-Object {$_.ModuleName -ieq 'foo_metadata_isolation.dll' -and $_.FileName.StartsWith($portable,[StringComparison]::OrdinalIgnoreCase)}).Count -eq 1
            $seenMasstagger=@($mods | Where-Object {$_.ModuleName -ieq 'foo_masstag.dll' -and $_.FileName.StartsWith($portable,[StringComparison]::OrdinalIgnoreCase)}).Count -eq 1
            if($seenMetadata -and $seenMasstagger){break}
        } catch [System.ComponentModel.Win32Exception] {}
    }
    if(-not $seenMetadata -or -not $seenMasstagger){
        throw "Actual co-load failed: isolation=$seenMetadata masstagger=$seenMasstagger"
    }
    Write-Host ("REAL_HOST_MASSTAGGER_193_AND_METADATA_ISOLATION_COLOAD_$Architecture=PASS")
    if((Get-FileHash -LiteralPath $plain -Algorithm SHA256).Hash -cne $beforeHash){
        throw 'Actual two-component startup modified synthetic source'
    }
    Write-Host ("REAL_HOST_MASSTAGGER_COLOAD_NO_MEDIA_WRITE_$Architecture=PASS")
    # Actual Masstagger command-line API, historically documented as /tag:...
    # This is a non-no-op real third-party action, NOT our own write probe.
    # The synthetic file has NO embedded Cue; a physical COMM addition is safe
    # and proves the independent Masstagger writer really executed.
    $token='MI_SYNTHETIC_MASSTAGGER_193'
    $launch=Start-Process -FilePath $exe -WorkingDirectory $portable -ArgumentList @("/tag:comment=$token",('"' + $plain + '"')) -PassThru
    if(-not $launch.WaitForExit(25000)){throw 'Actual Masstagger CLI launcher did not terminate'}
    if($launch.ExitCode -ne 0){throw 'Actual Masstagger CLI launcher returned failure'}
    $changed=$false
    for($i=0;$i -lt 80;$i++){
        Start-Sleep -Milliseconds 200
        $process.Refresh()
        if($process.HasExited){throw 'Host crashed during Masstagger real tag-write'}
        if((Get-FileHash -LiteralPath $plain -Algorithm SHA256).Hash -cne $beforeHash){
            $changed=$true
            break
        }
    }
    if(-not $changed){
        throw 'MASSTAGGER_REAL_TAG_WRITE_NOT_OBSERVED: CLI may be unsupported or not dispatched; do not claim PASS'
    }
    & python tests/assert_masstagger_plain_tag_write.py $before $plain $token
    if($LASTEXITCODE -ne 0){throw 'Independent Masstagger frame, PRIV, audio integrity oracle failed'}
    Write-Host ("REAL_HOST_MASSTAGGER_193_PHYSICAL_NON_NOOP_TAG_WRITE_$Architecture=PASS")
    # True third-party command applied to a new *embedded-CUE* MP3, the
    # key physical-vs-virtual safety case this component exists to protect.
    # Never use any owner music or a foobar configuration from a real device.
    $cueDir=Join-Path $root 'embedded-cue-masstagger'
    [void](New-Item -ItemType Directory -Path $cueDir)
    $cueSource=Join-Path $cueDir 'track-01.mp3'
    & python tests/make_isolated_embedded_mp3.py $cueSource
    if($LASTEXITCODE -ne 0){throw 'Embedded Cue synthetic fixture failed'}
    $cueBefore=Join-Path $cueDir 'track-01.before'
    Copy-Item -LiteralPath $cueSource -Destination $cueBefore
    $cueSha=(Get-FileHash -LiteralPath $cueSource -Algorithm SHA256).Hash
    $cueExpectedComment='MI_SYNTHETIC_CUE_PHYSICAL_193'
    $cueLauncher=Start-Process -FilePath $exe -WorkingDirectory $portable -ArgumentList @("/tag:comment=$cueExpectedComment",('"' + $cueSource + '"')) -PassThru
    if(-not $cueLauncher.WaitForExit(25000)){
        throw 'Genuine Masstagger embedded Cue command launcher timed out'
    }
    if($cueLauncher.ExitCode -ne 0){
        throw 'Genuine Masstagger embedded Cue command launcher exited unsuccessfully'
    }
    $cueChanged=$false
    for($attempt=0;$attempt -lt 100;$attempt++){
        Start-Sleep -Milliseconds 180
        $process.Refresh()
        if($process.HasExited){throw 'Two-component host crashed during embedded Cue tagging'}
        if((Get-FileHash -LiteralPath $cueSource -Algorithm SHA256).Hash -cne $cueSha){
            $cueChanged=$true
            break
        }
    }
    if(-not $cueChanged){
        throw 'MASSTAGGER_EMBEDDED_CUE_REAL_WRITE_NOT_OBSERVED (safe reject possible, but positive user workflow NOT QUALIFIED)'
    }
    & python tests/assert_masstagger_embedded_cue_physical.py $cueBefore $cueSource $cueExpectedComment
    if($LASTEXITCODE -ne 0){
        throw 'Actual Masstagger Cue physical-vs-virtual postimage oracle failed'
    }
    Write-Host ("REAL_HOST_MASSTAGGER_193_EMBEDDED_CUE_PHYSICAL_COMMENT_$Architecture=PASS")

}finally{
    if($null -ne $process){
        try{if(-not $process.HasExited){Stop-Process -Id $process.Id -Force -ErrorAction Stop}}catch{Write-Warning 'Disposable host could not be stopped'}
    }
    Remove-Item -LiteralPath $root -Force -Recurse -ErrorAction SilentlyContinue
}
