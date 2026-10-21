#requires -Version 7.2
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if($env:GITHUB_ACTIONS -cne 'true' -or [string]::IsNullOrWhiteSpace($env:RUNNER_TEMP)){
    throw 'Original Masstagger preflight is restricted to ephemeral Windows CI'
}
$allowedHost='www.foobar2000.org'
$urls=@(
    'https://www.foobar2000.org/getcomponent/ba815d92944b4add846a924efa784394/foo_masstag.fb2k-component',
    'https://www.foobar2000.org/getfile/foo_masstag.fb2k-component'
)
$root=Join-Path $env:RUNNER_TEMP ('mi-original-masstagger-'+[Guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $root -ErrorAction Stop)
$archive=Join-Path $root 'foo_masstag-1.9.3.fb2k-component'
try {
    $obtained=$false
    foreach($url in $urls){
        $uri=[uri]$url
        if($uri.Scheme -cne 'https' -or $uri.Host -cne $allowedHost){
            throw 'Nonofficial Masstagger URL not allowed'
        }
        for($attempt=1;$attempt -le 3;$attempt++){
            Remove-Item -LiteralPath $archive -Force -ErrorAction SilentlyContinue
            try {
                # No third-party mirrors, artifacts, caches or local user storage:
                # both candidate endpoints belong to the same foobar2000 host.
                $resp=Invoke-WebRequest -Uri $url -OutFile $archive -TimeoutSec 35 -MaximumRedirection 0 -ErrorAction Stop
                $bytes=(Get-Item -LiteralPath $archive).Length
                if($bytes -lt 1024 -or $bytes -gt 16MB){
                    throw "Original Masstagger response has invalid byte length $bytes"
                }
                $obtained=$true
                Write-Host ("MASSTAGGER_OFFICIAL_ORIGIN_PATH={0}" -f $uri.AbsolutePath)
                break
            }catch{
                Write-Warning ("Original Masstagger endpoint {0}, attempt {1}/3 unavailable ({2})" -f $uri.AbsolutePath,$attempt,$_.Exception.GetType().Name)
                Remove-Item -LiteralPath $archive -Force -ErrorAction SilentlyContinue
                if($attempt -lt 3){Start-Sleep -Seconds (2*$attempt)}
            }
        }
        if($obtained){break}
    }
    if(-not $obtained){
        throw 'MASSTAGGER_OFFICIAL_SOURCE_UNAVAILABLE: only original foobar2000.org endpoints were attempted'
    }
    Add-Type -AssemblyName System.IO.Compression
    $zip=[IO.Compression.ZipFile]::OpenRead($archive)
    try{
        $entries=@($zip.Entries)
        if($entries.Count -lt 1 -or $entries.Count -gt 64){
            throw 'Masstagger original package has unexpected number of ZIP members'
        }
        $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        $dlls=@()
        foreach($entry in $entries){
            $name=$entry.FullName
            if([string]::IsNullOrWhiteSpace($name) -or $name.StartsWith('/') -or
               $name -match '\\' -or $name -match '(^|/)\.\.?(/|$)' -or
               $name -match '^[A-Za-z]:' -or -not $seen.Add($name)){
                throw 'Unsafe or duplicate original Masstagger archive member'
            }
            if($entry.Length -lt 0 -or $entry.Length -gt 32MB){
                throw 'Unexpected original Masstagger member length'
            }
            if($name.EndsWith('.dll',[StringComparison]::OrdinalIgnoreCase)){
                $dlls+=,$name
            }
        }
        if($dlls.Count -lt 1){
            throw 'Downloaded package has no DLL, cannot treat HTML as component'
        }
        Write-Host ("MASSTAGGER_OFFICIAL_ARCHIVE_ENTRIES={0}" -f $entries.Count)
        Write-Host ("MASSTAGGER_OFFICIAL_DLL_PATHS={0}" -f ($dlls -join ';'))
    }finally{
        $zip.Dispose()
    }
    $hash=(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host ("MASSTAGGER_1_9_3_ORIGINAL_ZIP_SHA256={0}" -f $hash)
    Write-Host 'MASSTAGGER_OFFICIAL_BINARY_DISCOVERY=PASS_UNEXECUTED'
    Write-Host 'MASSTAGGER_BINARIES_PUBLISHED=0'
    Write-Host 'MASSTAGGER_ACTUAL_TAG_WRITE_E2E=PENDING_INDEPENDENT_PIN_AND_HOST'
}finally{
    Remove-Item -LiteralPath $root -Force -Recurse -ErrorAction SilentlyContinue
}
