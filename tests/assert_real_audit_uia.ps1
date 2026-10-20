[CmdletBinding()]
param(
  [Parameter(Mandatory)][int]$FoobarPid,
  [Parameter(Mandatory)][string]$Executable,
  [Parameter(Mandatory)][string]$Workdir,
  [Parameter(Mandatory)][string]$FixturePath,
  [Parameter(Mandatory)][string]$AdapterPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or -not $env:RUNNER_TEMP) {
  throw 'Native UIA test is restricted to disposable GitHub Windows runners'
}
$root=[IO.Path]::GetFullPath($env:RUNNER_TEMP).TrimEnd('\') + '\'
$fixture=[IO.Path]::GetFullPath($FixturePath)
if (-not $fixture.StartsWith($root,[StringComparison]::OrdinalIgnoreCase) -or
    [IO.Path]::GetFileName($fixture) -cne 'track-01.mp3') {
  throw 'Refusing non-disposable UIA media fixture'
}
if (-not (Test-Path -LiteralPath $AdapterPath) -or
    -not (Test-Path -LiteralPath $fixture)) {
  throw 'UIA adapter or synthetic fixture missing'
}
. $AdapterPath

Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class MetadataIsolationWindowLocator {
  public delegate bool Visitor(IntPtr window, IntPtr state);
  [DllImport("user32.dll")] public static extern bool EnumWindows(Visitor visitor, IntPtr state);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr window, StringBuilder output, int maxCount);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
  public static IntPtr FindOwned(int processId, string title) {
    IntPtr found=IntPtr.Zero;
    int count=0;
    EnumWindows((window, ignored) => {
      uint owner;
      GetWindowThreadProcessId(window, out owner);
      if (owner != (uint)processId || !IsWindowVisible(window)) return true;
      var name=new StringBuilder(256);
      GetWindowTextW(window, name, name.Capacity);
      if (String.Equals(name.ToString(),title,StringComparison.Ordinal)) {
        found=window; count++;
      }
      return true;
    },IntPtr.Zero);
    return count == 1 ? found : IntPtr.Zero;
  }
}
'@

$title='Metadata Isolation - Physical Metadata Audit'
$context='Metadata Isolation/Audit physical metadata for selection...'
function Open-Audit {
  param([string]$menu)
  $cli='/context_command:"'+$menu+'"'
  $p=Start-Process -FilePath $Executable -WorkingDirectory $Workdir -ArgumentList @($cli,('"' + $fixture + '"')) -PassThru
  if (-not $p.WaitForExit(20000)) { throw 'Context command launcher timeout' }
  if ($p.ExitCode -ne 0) { throw 'Context command launcher failed' }
}
function Find-Audit {
  for($i=0;$i -lt 70;$i++){
    $window=[MetadataIsolationWindowLocator]::FindOwned($FoobarPid,$title)
    if($window -ne [IntPtr]::Zero){return $window}
    Start-Sleep -Milliseconds 200
  }
  return [IntPtr]::Zero
}
$before=(Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
Open-Audit $context
$window=Find-Audit
if($window -eq [IntPtr]::Zero) {
  # Some foobar builds expose the context command only by leaf label.
  Open-Audit 'Audit physical metadata for selection...'
  $window=Find-Audit
}
if($window -eq [IntPtr]::Zero){throw 'Real foobar Audit HWND not found as exactly one visible process-owned window'}
$result=Test-WindowsUiAccessibility -WindowHandle $window -RequiredNames @(
    'Rescan','Cancel scan','Edit physical metadata...','Reset columns','Close')
if(-not $result.Passed){
  throw ('Real Audit UIA geometry failed: '+($result.Issues -join ','))
}
Write-Host 'PRODUCTION_AUDIT_UIA_GEOMETRY=PASS'
Write-Host ('PRODUCTION_AUDIT_UIA_EXPECTED_ELEMENTS='+$result.ExpectedElements)
Open-Audit $context
$reopened=Find-Audit
if($reopened -eq [IntPtr]::Zero -or $reopened -ne $window){
  throw 'Real Audit dialog duplicated or failed to reuse process-owned HWND'
}
Write-Host 'PRODUCTION_AUDIT_MODELESS_REUSE=PASS'

# Synthetic test-build only. Query post-scan internal UI state in the real host
# using strictly aggregate counters; no paths or metadata values are returned.
$stateReport = $fixture + '.metadata-isolation-probe.txt'
if (Test-Path -LiteralPath $stateReport) { throw 'Stale Audit state report' }
$started = Start-Process -FilePath $Executable -WorkingDirectory $Workdir -ArgumentList @(
  '/metadata-isolation-probe-audit-internal-ui-state', ('"' + $fixture + '"')) -PassThru
if (-not $started.WaitForExit(15000) -or $started.ExitCode -ne 0) {
  throw 'Synthetic Audit internal state probe launcher failed'
}
$ready = $false
for ($i = 0; $i -lt 100; $i++) {
  if (Test-Path -LiteralPath $stateReport) {
    if ((Get-Item -LiteralPath $stateReport).Length -gt 0) { $ready = $true; break }
  }
  Start-Sleep -Milliseconds 100
}
if (-not $ready) { throw 'Audit internal state report unavailable' }
$fields = @{}
foreach ($line in ((Get-Content -LiteralPath $stateReport -Raw) -split '\r?\n')) {
  if ($line -match '^([a-z_]+)=(.*)$') { $fields[$Matches[1]] = $Matches[2] }
}
Remove-Item -LiteralPath $stateReport -Force
$expectedState = @{
  audit_model_rows = '1'
  audit_filtered_rows = '1'
  audit_native_rows = '1'
  audit_filter_index = '0'
  audit_list_visible = 'true'
  write_result = 'audit_internal_state_readonly'
}
foreach ($field in @('audit_model_rows','audit_filtered_rows','audit_native_rows','audit_filter_index','audit_list_visible','write_result')) {
  if (-not $fields.ContainsKey($field)) { throw "Missing in-process Audit evidence: $field" }
  Write-Host ('AUDIT_IN_PROCESS_' + $field.ToUpperInvariant() + '=' + $fields[$field])
  if ($fields[$field] -cne $expectedState[$field]) {
    throw "In-process Audit post-scan invariant fails: $field"
  }
}
Write-Host 'PRODUCTION_AUDIT_IN_PROCESS_ROW_CONSISTENCY=PASS'

if((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $before){
  throw 'Real Audit UIA test modified synthetic MP3'
}
Write-Host 'PRODUCTION_AUDIT_SYNTHETIC_MEDIA_UNCHANGED=PASS'
