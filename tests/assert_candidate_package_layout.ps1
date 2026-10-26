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
    & ./ci/verify-metadata-isolation-component.ps1 -ComponentPath $path -DllName $rootDll | Out-Null
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

  # Same-machine modified DLL payloads must fail even if ZIP/PE layout passes.
  $builtWin=Join-Path $root 'expected-win32.dll'
  $builtX64=Join-Path $root 'expected-x64.dll'
  [IO.File]::WriteAllBytes($builtWin,$win)
  [IO.File]::WriteAllBytes($builtX64,$x64)
  $verified=New-TestPackage $normal
  # The shared structural verifier must still infer exactly one root DLL
  # when the product wrapper is called without an explicit component name.
  $inferred=& ./ci/verify-metadata-isolation-component.ps1 -ComponentPath $verified
  if($inferred.Status -cne 'PASS' -or $inferred.DllName -cne $rootDll -or
     $inferred.PayloadMatchesBuild) {
    throw 'Shared verifier delegation or unique DLL inference failed'
  }
  Write-Host 'CANDIDATE_SHARED_VERIFIER_INFERENCE=PASS'
  $proof=& ./ci/verify-metadata-isolation-component.ps1 -ComponentPath $verified -DllName $rootDll -ExpectedWin32DllPath $builtWin -ExpectedX64DllPath $builtX64
  if(-not $proof.PayloadMatchesBuild -or
     $proof.Win32DllSha256 -cne (Get-FileHash -LiteralPath $builtWin -Algorithm SHA256).Hash.ToLowerInvariant() -or
     $proof.X64DllSha256 -cne (Get-FileHash -LiteralPath $builtX64 -Algorithm SHA256).Hash.ToLowerInvariant()) {
    throw 'Matched built DLL SHA-256 evidence missing'
  }

  function Reject-ExpectedDll([string]$case,[string]$expectedWin,[string]$expectedX64) {
    $rejected=$false
    try {
      & ./ci/verify-metadata-isolation-component.ps1 -ComponentPath $verified -DllName $rootDll -ExpectedWin32DllPath $expectedWin -ExpectedX64DllPath $expectedX64 | Out-Null
    } catch { $rejected=$true }
    if(-not $rejected){throw ("Archive accepted forged or incomplete built-DLL provenance: "+$case)}
  }
  $wrongWin=[byte[]]$win.Clone()
  $wrongWin[80]=0x7f
  $wrongX64=[byte[]]$x64.Clone()
  $wrongX64[80]=0x7f
  $badWin=Join-Path $root 'modified-same-machine-win32.dll'
  $badX64=Join-Path $root 'modified-same-machine-x64.dll'
  [IO.File]::WriteAllBytes($badWin,$wrongWin)
  [IO.File]::WriteAllBytes($badX64,$wrongX64)
  Reject-ExpectedDll 'forged Win32 bytes' $badWin $builtX64
  Reject-ExpectedDll 'forged x64 bytes' $builtWin $badX64
  Reject-ExpectedDll 'forged both architecture bytes' $badWin $badX64
  $rejected=$false
  try {
    & ./ci/verify-metadata-isolation-component.ps1 -ComponentPath $verified -DllName $rootDll -ExpectedWin32DllPath $builtWin | Out-Null
  } catch { $rejected=$true }
  if(-not $rejected){throw 'Archive accepted one-sided expected build provenance'}
  Write-Host 'CANDIDATE_ZIP_BUILT_DLL_DIGEST_MUTATIONS=PASS (1 matched, 4 rejected)'

} finally {
  if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
}
