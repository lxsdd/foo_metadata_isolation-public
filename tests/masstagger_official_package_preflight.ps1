#requires -Version 7.2
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if($env:GITHUB_ACTIONS -cne 'true' -or [string]::IsNullOrWhiteSpace($env:RUNNER_TEMP)){
  throw 'Official Masstagger discovery is restricted to an ephemeral GitHub Windows runner'
}
$uri='https://www.foobar2000.org/getcomponent/ba815d92944b4add846a924efa784394/foo_masstag.fb2k-component'
$root=Join-Path $env:RUNNER_TEMP ('mi-masstagger-official-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
$archive=Join-Path $root 'foo_masstag-1.9.3-source-only.fb2k-component'
try{
  $downloaded=$false
  for($attempt=1;$attempt -le 3;$attempt++){
    try{
      Invoke-WebRequest -Uri $uri -OutFile $archive -TimeoutSec 35 -ErrorAction Stop
      $downloaded=$true
      break
    }catch{
      Remove-Item -LiteralPath $archive -Force -ErrorAction SilentlyContinue
      Write-Warning ("Official Masstagger download attempt {0}/3 failed: {1}" -f $attempt,$_.Exception.GetType().Name)
      if($attempt -lt 3){Start-Sleep -Seconds (2*$attempt)}
    }
  }
  if(-not $downloaded){throw 'Official Masstagger download blocked; no alternative mirror allowed'}
  $size=(Get-Item -LiteralPath $archive).Length
  if($size -lt 1024 -or $size -gt 16MB){throw 'Unexpected official component package size'}
  Add-Type -AssemblyName System.IO.Compression
  $zip=[IO.Compression.ZipFile]::OpenRead($archive)
  try{
    $entries=@($zip.Entries)
    if($entries.Count -lt 1 -or $entries.Count -gt 40){throw 'Unreasonable Masstagger archive entry count'}
    $names=[System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $dlls=@()
    foreach($entry in $entries){
      $name=$entry.FullName
      if([string]::IsNullOrWhiteSpace($name) -or $name.StartsWith('/') -or
         $name -match '\.\.' -or $name -match '\\' -or
         $name -match '^[A-Za-z]:' -or -not $names.Add($name)){
        throw 'Untrusted official package path, duplicate or traversal'
      }
      if($entry.Length -gt 32MB){throw 'Excessive Masstagger unpacked entry'}
      if($name.ToLowerInvariant().EndsWith('.dll')){$dlls+=,$name}
    }
    if($dlls.Count -lt 1){throw 'Official archive does not contain a DLL'}
    Write-Host ('MASSTAGGER_OFFICIAL_ARCHIVE_ENTRIES='+$entries.Count)
    Write-Host ('MASSTAGGER_OFFICIAL_DLL_PATHS='+($dlls -join ';'))
  }finally{$zip.Dispose()}
  $hash=(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
  Write-Host ('MASSTAGGER_1_9_3_OFFICIAL_PACKAGE_DISCOVERY_SHA256='+$hash)
  Write-Host 'MASSTAGGER_PACKAGE_DISCOVERY=PASS_SOURCE_ONLY_NO_EXECUTION'
  Write-Host 'MASSTAGGER_REAL_TAG_WRITE_AND_CONFIG_RESTORE=PENDING_E2E'
  Write-Host 'MASSTAGGER_COMPONENT_BINARY_PUBLICATION=DISABLED'
}finally{
  Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
