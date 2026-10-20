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
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window, int controlId);
  [DllImport("user32.dll", EntryPoint="SendMessageTimeoutW", SetLastError=true)]
  private static extern IntPtr SendMessageTimeout(IntPtr window,uint message,IntPtr wParam,IntPtr lParam,uint flags,uint timeout,out IntPtr result);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr window);
  [DllImport("user32.dll", EntryPoint="PostMessageW", SetLastError=true)] public static extern bool PostMessage(IntPtr window,uint message,IntPtr wParam,IntPtr lParam);
  public static IntPtr SendMessage(IntPtr window,uint message,IntPtr wParam,IntPtr lParam) {
    IntPtr result;
    if (window==IntPtr.Zero || SendMessageTimeout(window,message,wParam,lParam,0x0002,2500,out result)==IntPtr.Zero)
      throw new InvalidOperationException("Audited native control did not respond within 2.5s; message="+message.ToString("X"));
    return result;
  }
  public static string Status(IntPtr window) {
    IntPtr label=GetDlgItem(window,1201);
    if (label==IntPtr.Zero) throw new InvalidOperationException("Audit status control missing");
    var text=new StringBuilder(512);
    GetWindowTextW(label,text,text.Capacity);
    return text.ToString();
  }
  public static int ItemCount(IntPtr window) {
    IntPtr list=GetDlgItem(window,1203);
    if (list==IntPtr.Zero) throw new InvalidOperationException("Audit ListView missing");
    return checked((int)SendMessage(list,0x1004,IntPtr.Zero,IntPtr.Zero).ToInt64());
  }
  public static int CurrentFilter(IntPtr window) {
    IntPtr combo=GetDlgItem(window,1202);
    if (combo==IntPtr.Zero) throw new InvalidOperationException("Audit filter missing");
    return checked((int)SendMessage(combo,0x0147,IntPtr.Zero,IntPtr.Zero).ToInt64());
  }
  public static void SelectFilter(IntPtr window,int index) {
    IntPtr combo=GetDlgItem(window,1202);
    if (combo==IntPtr.Zero) throw new InvalidOperationException("Audit filter missing");
    long selected=SendMessage(combo,0x014E,new IntPtr(index),IntPtr.Zero).ToInt64();
    if (selected!=index) throw new InvalidOperationException("Audit filter selection rejected");
    if (!PostMessage(window,0x0111,new IntPtr((1 << 16) | 1202),combo))
      throw new InvalidOperationException("Failed posting native Audit filter changed notification");
  }
  public static void Rescan(IntPtr window) {
    IntPtr button=GetDlgItem(window,1204);
    if (button==IntPtr.Zero || !IsWindowEnabled(button))
      throw new InvalidOperationException("Audit Rescan control unavailable");
    if (!PostMessage(button,0x00F5,IntPtr.Zero,IntPtr.Zero))
      throw new InvalidOperationException("Failed posting native Audit Rescan button click");
  }
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
# The source is intentionally INCOMPLETE: physical DATE is absent while
# TITLE/ARTIST/ALBUM/GENRE and the embedded CUESHEET exist.
function Assert-CompletedAudit {
  param([IntPtr]$Dialog)
  $expected = 'Audited 1 embedded-Cue physical files: 1 problems, 0 OK'
  for($i=0;$i -lt 150;$i++){
    $status=[MetadataIsolationWindowLocator]::Status($Dialog)
    if($status -eq $expected){
      return
    }
    if($status.StartsWith('Audit failed',[StringComparison]::Ordinal)){
      throw 'Actual Audit reports a physical metadata read failure'
    }
    Start-Sleep -Milliseconds 100
  }
  throw ('Actual read-only audit did not complete with expected classification; status class=' +
    $(if($status.StartsWith('Scanning')){'SCANNING'}else{'UNEXPECTED'}))
}
Assert-CompletedAudit $window
Write-Host ('AUDIT_DEFAULT_FILTER_INDEX='+[MetadataIsolationWindowLocator]::CurrentFilter($window))
Write-Host ('AUDIT_DEFAULT_VISIBLE_ROWS='+[MetadataIsolationWindowLocator]::ItemCount($window))
# Independent UIA view of the actual native SysListView32 (no Win32 messages).
$ownedList=[MetadataIsolationWindowLocator]::GetDlgItem($window,1203)
$uiList=[System.Windows.Automation.AutomationElement]::FromHandle($ownedList)
if($null -eq $uiList){throw 'Native Audit ListView is not exposed through Windows UIA'}
$uiRows=$uiList.FindAll(
  [System.Windows.Automation.TreeScope]::Descendants,
  [System.Windows.Automation.PropertyCondition]::new(
    [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
    [System.Windows.Automation.ControlType]::DataItem))
Write-Host ('AUDIT_DEFAULT_UIA_DATAITEMS='+$uiRows.Count)

Write-Host 'PRODUCTION_AUDIT_INCOMPLETE_CLASSIFIER=PASS'
# Read the actual in-process model and ListView after completion.
# Test-only route returns aggregate counters, never media paths/tag values.
$probeReport = $fixture + '.metadata-isolation-probe.txt'
if(Test-Path -LiteralPath $probeReport){throw 'Refusing to read stale Audit internal report'}
$cli = '/metadata-isolation-probe-audit-internal-ui-state'
$launcher=Start-Process -FilePath $Executable -WorkingDirectory $Workdir -ArgumentList @($cli,('"'+$fixture+'"')) -PassThru
if(-not $launcher.WaitForExit(15000) -or $launcher.ExitCode -ne 0){throw 'Audit internal probe launcher failed'}
$reportReady=$false
for($i=0;$i -lt 100;$i++){
  if(Test-Path -LiteralPath $probeReport){
    if((Get-Item -LiteralPath $probeReport).Length -gt 0){$reportReady=$true;break}
  }
  Start-Sleep -Milliseconds 100
}
if(-not $reportReady){throw 'Audit internal state probe did not complete'}
$stateText=Get-Content -LiteralPath $probeReport -Raw
Remove-Item -LiteralPath $probeReport -Force
$fields=@{}
foreach($line in ($stateText -split '\r?\n')){
  if($line -match '^([a-z_]+)=([^\r\n]*)# Each change is applied to the actual production ComboBox and WM_COMMAND,
# never by calling internal C++ application logic or mocking rows.
foreach($entry in @(
  @{Name='All';Index=1;Expected=1},
  @{Name='OK';Index=6;Expected=0},
  @{Name='Incomplete';Index=3;Expected=1},
  @{Name='Empty';Index=2;Expected=0},
  @{Name='Problems';Index=0;Expected=1},
  @{Name='Mismatch';Index=4;Expected=0},
  @{Name='Review';Index=5;Expected=0}
)){
  [MetadataIsolationWindowLocator]::SelectFilter($window,[int]$entry.Index)
  $seen=-1
  for($retry=0;$retry -lt 50;$retry++){
    $seen=[MetadataIsolationWindowLocator]::ItemCount($window)
    if($seen -eq [int]$entry.Expected){break}
    Start-Sleep -Milliseconds 100
  }
  if($seen -ne [int]$entry.Expected){
    throw ('Actual native audit '+$entry.Name+' filter count incorrect; expected '+$entry.Expected+' observed '+$seen)
  }
  Write-Host ('AUDIT_NATIVE_FILTER_'+$entry.Name.ToUpperInvariant()+'=PASS')
}
Write-Host 'PRODUCTION_AUDIT_NATIVE_FILTER_MATRIX=PASS'
[MetadataIsolationWindowLocator]::SelectFilter($window,0)
# BM_CLICK is a real native button event, not a direct test callback.
[MetadataIsolationWindowLocator]::Rescan($window)
$scanObserved=$false
for($i=0;$i -lt 40;$i++){
  $pending=[MetadataIsolationWindowLocator]::Status($window)
  if($pending.StartsWith('Scanning physical metadata') -or
     $pending.StartsWith('Finding embedded-Cue physical sources')){ $scanObserved=$true;break }
  Start-Sleep -Milliseconds 50
}
if(-not $scanObserved){
  throw 'Native Rescan did not trigger an observable read-only scan'
}
Assert-CompletedAudit $window
Write-Host 'PRODUCTION_AUDIT_NATIVE_RESCAN=PASS'
if((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $before){
  throw 'Real Audit UIA test modified synthetic MP3'
}
Write-Host 'PRODUCTION_AUDIT_SYNTHETIC_MEDIA_UNCHANGED=PASS'
){$fields[$Matches[1]]=$Matches[2]}
}
foreach($name in @('audit_model_rows','audit_filtered_rows','audit_native_rows','audit_filter_index','audit_list_visible')){
  if(-not $fields.ContainsKey($name)){throw ('Internal Audit report lacks '+$name)}
  Write-Host ('AUDIT_INTERNAL_'+$name.ToUpperInvariant()+'='+$fields[$name])
}
if($fields['audit_model_rows'] -ne '1' -or $fields['audit_filtered_rows'] -ne '1' -or
   $fields['audit_native_rows'] -ne '1' -or $fields['audit_filter_index'] -ne '0' -or
   $fields['audit_list_visible'] -ne 'true'){
  throw 'Actual in-process Audit model and native ListView disagree on one visible incomplete source'
}
Write-Host 'PRODUCTION_AUDIT_IN_PROCESS_ROW_CONSISTENCY=PASS'
# Each change is applied to the actual production ComboBox and WM_COMMAND,
# never by calling internal C++ application logic or mocking rows.
foreach($entry in @(
  @{Name='All';Index=1;Expected=1},
  @{Name='OK';Index=6;Expected=0},
  @{Name='Incomplete';Index=3;Expected=1},
  @{Name='Empty';Index=2;Expected=0},
  @{Name='Problems';Index=0;Expected=1},
  @{Name='Mismatch';Index=4;Expected=0},
  @{Name='Review';Index=5;Expected=0}
)){
  [MetadataIsolationWindowLocator]::SelectFilter($window,[int]$entry.Index)
  $seen=-1
  for($retry=0;$retry -lt 50;$retry++){
    $seen=[MetadataIsolationWindowLocator]::ItemCount($window)
    if($seen -eq [int]$entry.Expected){break}
    Start-Sleep -Milliseconds 100
  }
  if($seen -ne [int]$entry.Expected){
    throw ('Actual native audit '+$entry.Name+' filter count incorrect; expected '+$entry.Expected+' observed '+$seen)
  }
  Write-Host ('AUDIT_NATIVE_FILTER_'+$entry.Name.ToUpperInvariant()+'=PASS')
}
Write-Host 'PRODUCTION_AUDIT_NATIVE_FILTER_MATRIX=PASS'
[MetadataIsolationWindowLocator]::SelectFilter($window,0)
# BM_CLICK is a real native button event, not a direct test callback.
[MetadataIsolationWindowLocator]::Rescan($window)
$scanObserved=$false
for($i=0;$i -lt 40;$i++){
  $pending=[MetadataIsolationWindowLocator]::Status($window)
  if($pending.StartsWith('Scanning physical metadata') -or
     $pending.StartsWith('Finding embedded-Cue physical sources')){ $scanObserved=$true;break }
  Start-Sleep -Milliseconds 50
}
if(-not $scanObserved){
  throw 'Native Rescan did not trigger an observable read-only scan'
}
Assert-CompletedAudit $window
Write-Host 'PRODUCTION_AUDIT_NATIVE_RESCAN=PASS'
if((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $before){
  throw 'Real Audit UIA test modified synthetic MP3'
}
Write-Host 'PRODUCTION_AUDIT_SYNTHETIC_MEDIA_UNCHANGED=PASS'
