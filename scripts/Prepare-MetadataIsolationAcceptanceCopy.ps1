#requires -Version 5.1
<#
.SYNOPSIS
Creates an independent byte-verified local test copy of ONE user-selected
embedded-Cue audio file, without changing the original.
.DESCRIPTION
Does not upload files, install components, start foobar, modify ACLs on the
source, or authorize write testing. Requires the owner to supply both paths.
The clone is a disposable local test INPUT, not an approved target for writes
until a separately agreed real-host acceptance procedure is followed.
#>
[CmdletBinding(SupportsShouldProcess=$true,ConfirmImpact='Medium')]
param(
  [Parameter(Mandatory)][ValidateNotNullOrEmpty()][string]$SourceMix,
  [Parameter(Mandatory)][ValidateNotNullOrEmpty()][string]$DestinationRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$allowed=@('.mp3','.flac','.wv','.ogg','.opus')
$source=(Resolve-Path -LiteralPath $SourceMix -ErrorAction Stop).Path
$sourceItem=Get-Item -LiteralPath $source -Force -ErrorAction Stop
if($sourceItem.PSIsContainer -or $allowed -notcontains $sourceItem.Extension.ToLowerInvariant() -or
   $sourceItem.Length -le 0){
  throw 'Expected a single existing nonempty audio file (.mp3/.flac/.wv/.ogg/.opus), not a directory'
}
$root=(Resolve-Path -LiteralPath $DestinationRoot -ErrorAction Stop).Path
$rootItem=Get-Item -LiteralPath $root -Force -ErrorAction Stop
if(-not $rootItem.PSIsContainer){throw 'DestinationRoot must be an existing directory'}
# All source and destination directory links/reparse points are rejected:
# a caller-provided path must not escape the intended location unexpectedly.
foreach($target in @($sourceItem,$rootItem)){
  if(($target.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){
    throw 'Reparse-point/symlink source or destination is not supported'
  }
  # FileInfo has Directory, not Parent; DirectoryInfo has Parent.
  $cur=if($target -is [IO.FileInfo]){$target.Directory}else{$target}
  while($null -ne $cur){
    if(($cur.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){
      throw 'Reparse-point/symlink source or destination hierarchy is not supported'
    }
    $cur=$cur.Parent
  }
}
$sourceParent=[IO.Path]::GetFullPath($sourceItem.DirectoryName).TrimEnd('\','/')
$destPath=[IO.Path]::GetFullPath($rootItem.FullName).TrimEnd('\','/')
if($destPath.Equals($sourceParent,[StringComparison]::OrdinalIgnoreCase) -or
   $destPath.StartsWith($sourceParent+'\',[StringComparison]::OrdinalIgnoreCase)){
  throw 'DestinationRoot must be independent of the source audio directory'
}
$originalLength=$sourceItem.Length
$originalWriteUtc=$sourceItem.LastWriteTimeUtc
$hashBefore=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
$cloneDir=Join-Path $root ('mi-111-track-review-'+[guid]::NewGuid().ToString('N'))
if(Test-Path -LiteralPath $cloneDir){throw 'Generated clone path unexpectedly exists'}
if(-not $PSCmdlet.ShouldProcess($cloneDir,'Create an independent byte-verified audio file copy')){
  return
}
$created=$false
try{
  $null=New-Item -ItemType Directory -Path $cloneDir -ErrorAction Stop
  $created=$true
  $clonePath=Join-Path $cloneDir $sourceItem.Name
  Copy-Item -LiteralPath $source -Destination $clonePath -ErrorAction Stop
  $cloneItem=Get-Item -LiteralPath $clonePath -Force
  if($cloneItem.Length -ne $originalLength){throw 'Copied media length mismatch'}
  $cloneHash=(Get-FileHash -LiteralPath $clonePath -Algorithm SHA256).Hash.ToLowerInvariant()
  $hashAfter=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
  $sourceItem.Refresh()
  if($hashBefore -cne $cloneHash -or $hashBefore -cne $hashAfter -or
     $sourceItem.Length -ne $originalLength -or
     $sourceItem.LastWriteTimeUtc -ne $originalWriteUtc){
    throw 'Original or independent clone changed; refusing to qualify media'
  }
  # The manifest intentionally excludes the original file location/name and
  # all private tags. Nothing is sent over the network or written to Git.
  $manifest=[ordered]@{
    format='metadata-isolation-owner-clone-preflight-v1'
    source_file_size_bytes=$originalLength
    sha256_original_and_clone=$hashBefore
    source_rechecked_unchanged=$true
    cloned_original_audio_files=1
    real_cue_track_count='UNVERIFIED'
    write_acceptance='NOT_AUTHORIZED'
    authenticated_discogger='NOT_TESTED'
    authenticated_masstagger='NOT_TESTED'
    full_real_host_qa='PENDING'
  }
  $manifestPath=Join-Path $cloneDir 'review-manifest.json'
  $manifest | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
  if((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -cne $hashBefore){
    throw 'Original changed after preflight manifest creation'
  }
  Write-Host 'OWNER_MIX_INDEPENDENT_BYTE_EXACT_CLONE=PASS (local only; no user media uploaded)'
  Write-Host 'OWNER_MIX_REAL_TRACK_COUNT_AND_WRITES=PENDING_EXPLICIT_OWNER_ACCEPTANCE'
  [pscustomobject]@{ClonePath=$clonePath;ManifestPath=$manifestPath;Sha256=$hashBefore;Bytes=$originalLength}
}catch{
  if($created -and (Test-Path -LiteralPath $cloneDir)){
    Remove-Item -LiteralPath $cloneDir -Recurse -Force -ErrorAction SilentlyContinue
  }
  throw
}
