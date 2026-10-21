#requires -Version 7.2
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if($env:GITHUB_ACTIONS -cne 'true' -or -not $env:RUNNER_TEMP){
  throw 'Owner mix copy regression must only use synthetic CI fixtures'
}
$root=Join-Path $env:RUNNER_TEMP ('mi-copy-selftest-'+[guid]::NewGuid().ToString('N'))
$sourceDir=Join-Path $root 'synthetic-originals'
$dest=Join-Path $root 'synthetic-clones'
New-Item -ItemType Directory -Path $sourceDir,$dest -Force | Out-Null
try{
  # This byte buffer is not a valid consumer track or a user-originating file.
  $source=Join-Path $sourceDir 'synthetic-111-track-preflight.mp3'
  $data=[byte[]]::new(8192)
  for($i=0;$i -lt $data.Length;$i++){$data[$i]=[byte](($i*17 + 91) -band 255)}
  [IO.File]::WriteAllBytes($source,$data)
  $before=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
  $result=@(& ./scripts/Prepare-MetadataIsolationAcceptanceCopy.ps1 -SourceMix $source -DestinationRoot $dest -Confirm:$false)
  if($result.Count -ne 1){throw 'Clone helper did not return one independent clone result'}
  $copy=$result[0]
  if($copy.Sha256 -cne $before -or -not (Test-Path -LiteralPath $copy.ClonePath)){
    throw 'Synthetic clone SHA evidence missing'
  }
  $manifest=Get-Content -LiteralPath $copy.ManifestPath -Raw | ConvertFrom-Json
  if($manifest.sha256_original_and_clone -cne $before -or
     $manifest.write_acceptance -cne 'NOT_AUTHORIZED' -or
     $manifest.real_cue_track_count -cne 'UNVERIFIED'){
    throw 'Synthetic clone manifest falsely authorized media writes or real track count'
  }
  $raw=Get-Content -LiteralPath $copy.ManifestPath -Raw
  if($raw.Contains($sourceDir) -or $raw.Contains([IO.Path]::GetFileName($source))){
    throw 'Clone manifest leaked original media path/filename'
  }
  if((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -cne $before){
    throw 'Clone helper modified original synthetic media'
  }
  $copyItem=Get-Item -LiteralPath $copy.ClonePath
  if($copyItem.Length -ne $data.Length){throw 'Cloned bytes have wrong length'}
  Write-Host 'OWNER_MIX_COPY_SYNTHETIC_BYTE_EXACT=PASS'

  $blocked=$false
  try{
    ./scripts/Prepare-MetadataIsolationAcceptanceCopy.ps1 -SourceMix $source -DestinationRoot $sourceDir -Confirm:$false | Out-Null
  }catch{$blocked=$true}
  if(-not $blocked){throw 'Clone helper accepted destination equal to the original media directory'}

  $bad=Join-Path $sourceDir 'not-a-mix.txt'
  [IO.File]::WriteAllBytes($bad,[byte[]](1,2,3))
  $blocked=$false
  try{
    ./scripts/Prepare-MetadataIsolationAcceptanceCopy.ps1 -SourceMix $bad -DestinationRoot $dest -Confirm:$false | Out-Null
  }catch{$blocked=$true}
  if(-not $blocked){throw 'Clone helper accepted unsupported file type'}
  Write-Host 'OWNER_MIX_COPY_NEGATIVE_CASES=PASS (unsafe destination; unsupported extension)'
  Write-Host 'OWNER_MIX_TEST_USES_ONLY_SYNTHETIC_RUNNER_BYTES=PASS'
}finally{
  Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
