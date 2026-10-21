[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
if($env:GITHUB_ACTIONS -ne 'true' -or -not $env:RUNNER_TEMP) {
  throw 'Archive mutation tests require disposable GitHub Windows runners'
}
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root=Join-Path $env:RUNNER_TEMP ('mi-package-test-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$rootDll='foo_metadata_isolation.dll'

function New-TestPe([UInt16]$machine) {
  $bytes=[byte[]]::new(96)
  $bytes[0]=0x4d; $bytes[1]=0x5a
  [BitConverter]::GetBytes([int]64).CopyTo($bytes,0x3c)
  $bytes[64]=0x50; $bytes[65]=0x45
  [BitConverter]::GetBytes($machine).CopyTo($bytes,68)
  return ,$bytes
}
function New-TestPackage([object[]]$entries) {
  $package=Join-Path $root ([Guid]::NewGuid().ToString('N')+'.fb2k-component')
  $zip=[IO.Compression.ZipFile]::Open($package,[IO.Compression.ZipArchiveMode]::Create)
  try {
    foreach($item in $entries) {
      $entry=$zip.CreateEntry([string]$item[0])
      if($null -ne $item[1]) {
        $stream=$entry.Open()
        try {
          $buffer=[byte[]]$item[1]
          $stream.Write($buffer,0,$buffer.Length)
        } finally { $stream.Dispose() }
      }
    }
  } finally { $zip.Dispose() }
  return $package
}
function Check-Package([object[]]$entries,[bool]$shouldPass,[string]$label) {
  $path=New-TestPackage $entries
  $ok=$true
  try {
    & ./ci/verify-fb2k-component.ps1 -ComponentPath $path -DllName $rootDll | Out-Null
  } catch { $ok=$false }
  if($ok -ne $shouldPass) {
    throw ("Package validation mismatch: {0}, wanted={1}, actual={2}" -f $label,$shouldPass,$ok)
  }
}
$win=New-TestPe 0x014c
$x64=New-TestPe 0x8664
$normal=@(@($rootDll,$win),@("x64/$rootDll",$x64))
try {
  Check-Package $normal $true 'two correctly named architectures'
  Check-Package @(@($rootDll,$win),@('x64/',$null),@("x64/$rootDll",$x64)) $true 'optional directory'
  Check-Package @(@($rootDll,$win)) $false 'missing x64'
  Check-Package @(@($rootDll,$x64),@("x64/$rootDll",$win)) $false 'swapped PE machine'
  Check-Package @(@($rootDll,$win),@("x64/$rootDll",$x64),@('notes.txt',[byte[]](1,2,3))) $false 'additional payload'
  Check-Package @(@($rootDll,$win),@("x64/$rootDll",$x64),@("$rootDll",$win)) $false 'duplicated root entry'
  Check-Package @(@($rootDll,$win),@("x64/$rootDll",$x64),@("X64/$rootDll",$x64)) $false 'case collision'
  Check-Package @(@($rootDll,$win),@("x64/$rootDll",$x64),@('../rogue.dll',$win)) $false 'traversal entry'
  Check-Package @(@($rootDll,$win),@("x64/$rootDll",$x64),@("x64\rogue.dll",$win)) $false 'backslash alias'
  Write-Host 'CANDIDATE_ZIP_LAYOUT_MUTATIONS=PASS (2 valid, 7 rejected)'
} finally {
  if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
}
