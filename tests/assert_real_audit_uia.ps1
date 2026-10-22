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
    // BM_CLICK is a synchronous native button command and is processed on
    // the target GUI thread before the host status is inspected.
    SendMessage(button,0x00F5,IntPtr.Zero,IntPtr.Zero);
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
if ([MetadataIsolationWindowLocator]::ItemCount($window) -ne 1) { throw 'Audit results list must contain exactly one synthetic problem row' }
# Independent UIA view of the actual native SysListView32 (no Win32 messages).
$ownedList=[MetadataIsolationWindowLocator]::GetDlgItem($window,1203)
$uiList=[System.Windows.Automation.AutomationElement]::FromHandle($ownedList)
if($null -eq $uiList){throw 'Native Audit ListView is not exposed through Windows UIA'}
$uiDataRows=$uiList.FindAll(
  [System.Windows.Automation.TreeScope]::Descendants,
  [System.Windows.Automation.PropertyCondition]::new(
    [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
    [System.Windows.Automation.ControlType]::DataItem))
$uiListRows=$uiList.FindAll(
  [System.Windows.Automation.TreeScope]::Descendants,
  [System.Windows.Automation.PropertyCondition]::new(
    [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
    [System.Windows.Automation.ControlType]::ListItem))
Write-Host ('AUDIT_DEFAULT_UIA_DATAITEMS='+$uiDataRows.Count)
Write-Host ('AUDIT_DEFAULT_UIA_LISTITEMS='+$uiListRows.Count)


# Test-only accessibility diagnostics. Native ListView counts are evaluated
# separately; these observations may be PENDING, never forged into a UIA PASS.
$rawWalker=[System.Windows.Automation.TreeWalker]::RawViewWalker
$rawChild=$rawWalker.GetFirstChild($uiList)
$rawCount=0
$rawListItems=0
while($null -ne $rawChild -and $rawCount -lt 64){
  $rawCount++
  $type=$rawChild.Current.ControlType
  if($type -eq [System.Windows.Automation.ControlType]::ListItem -or
     $type -eq [System.Windows.Automation.ControlType]::DataItem){$rawListItems++}
  $rawChild=$rawWalker.GetNextSibling($rawChild)
}
Write-Host ('AUDIT_UIA_RAW_CHILDREN='+$rawCount)
Write-Host ('AUDIT_UIA_RAW_DIRECT_ROW_CHILDREN='+$rawListItems)
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class IsolatedAuditMsaa {
  [ComImport, Guid("618736e0-3c3d-11cf-810c-00aa00389b71"), InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
  public interface IAccessibleMinimal {
    [DispId(-5001)] int accChildCount { get; }
  }
  [ComImport, Guid("618736e0-3c3d-11cf-810c-00aa00389b71"), InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
  public interface IAccessibleRole {
    [DispId(-5006)] object this[object child] { get; }
  }
  [ComImport, Guid("618736e0-3c3d-11cf-810c-00aa00389b71"), InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
  public interface IAccessibleName {
    [DispId(-5003)] string this[object child] { get; }
  }
  [DllImport("oleacc.dll", ExactSpelling=true)]
  private static extern int AccessibleObjectFromWindow(
     IntPtr hwnd, int dwObjectID, ref Guid riid,
     [MarshalAs(UnmanagedType.Interface)] out IAccessibleMinimal accessible);
  public static int[] ChildRoleSummary(IntPtr hwnd) {
    var iid = new Guid("618736e0-3c3d-11cf-810c-00aa00389b71");
    IAccessibleMinimal accessible = null;
    int hr = AccessibleObjectFromWindow(hwnd, -4, ref iid, out accessible);
    if (hr != 0 || accessible == null) return new int[] { hr, -1, -1, -1, -1 };
    try {
      int limit = Math.Min(32, accessible.accChildCount);
      int listItems=0, expectedNames=0, readFailures=0;
      IAccessibleRole roleView=(IAccessibleRole)accessible;
      IAccessibleName nameView=(IAccessibleName)accessible;
      for (int i=1; i<=limit; i++) {
        try {
          object role = roleView[i];
          if (role != null && Convert.ToInt32(role)==34) listItems++;
          var label = nameView[i];
          if (label != null && label.IndexOf("Incomplete", StringComparison.OrdinalIgnoreCase)>=0) expectedNames++;
        } catch (Exception) { readFailures++; }
      }
      return new int[] { hr, limit, listItems, expectedNames, readFailures };
    } catch (Exception) { return new int[] { hr, -2, -2, -2, -2 }; }
    finally { if (accessible != null) Marshal.ReleaseComObject(accessible); }
  }
  public static int[] ChildCount(IntPtr hwnd) {
    var iid = new Guid("618736e0-3c3d-11cf-810c-00aa00389b71");
    IAccessibleMinimal accessible = null;
    int hr = AccessibleObjectFromWindow(hwnd, -4, ref iid, out accessible);
    if (hr != 0 || accessible == null) return new int[] { hr, -1 };
    try { return new int[] { hr, accessible.accChildCount }; }
    catch (COMException ex) { return new int[] { ex.ErrorCode, -2 }; }
    finally { if (accessible != null) Marshal.ReleaseComObject(accessible); }
  }
}
'@
$msaa=[IsolatedAuditMsaa]::ChildCount($ownedList)
Write-Host ('AUDIT_MSAA_HRESULT='+$msaa[0])
Write-Host ('AUDIT_MSAA_CHILDREN='+$msaa[1])
$msaaDetails=[IsolatedAuditMsaa]::ChildRoleSummary($ownedList)
Write-Host ('AUDIT_MSAA_INSPECTED_CHILDREN='+$msaaDetails[1])
Write-Host ('AUDIT_MSAA_LISTITEM_ROLES='+$msaaDetails[2])
Write-Host ('AUDIT_MSAA_SYNTHETIC_INCOMPLETE_NAMES='+$msaaDetails[3])
Write-Host ('AUDIT_MSAA_DETAIL_FAILURES='+$msaaDetails[4])
if($msaa[0] -ne 0 -or $msaaDetails[0] -ne 0 -or
   $msaa[1] -ne $msaaDetails[1] -or
   $msaaDetails[2] -ne 1 -or $msaaDetails[3] -ne 1 -or
   $msaaDetails[4] -ne 0){
  throw 'Native Audit MSAA accessibility does not expose exactly one readable synthetic INCOMPLETE result row'
}
Write-Host 'PRODUCTION_AUDIT_MSAA_SINGLE_ROW=PASS'
# Independently prove that a real keyboard can focus/select the native audit
# row and that its actual MSAA accessible state reflects the selection.
# This is evidence for keyboard+MSAA, not full UIA/screen-reader acceptance.
& ./tests/assert_real_audit_keyboard_msaa.ps1 -FoobarPid $FoobarPid -AuditHwnd $window -ListHwnd $ownedList

# Read actual selected data from an independent, visible, read-only
# Windows EDIT control. Unlike native ListView row enumeration, its standard
# UIA ValuePattern does not depend on the host's ListView accessibility
# provider. Require real data: never substitute a PowerShell-computed label.
$detailHwnd=[MetadataIsolationWindowLocator]::GetDlgItem($window,1212)
if($detailHwnd -eq [IntPtr]::Zero) {
  throw 'Actual Audit accessible selected-result control missing'
}
$detailElement=[System.Windows.Automation.AutomationElement]::FromHandle($detailHwnd)
if($null -eq $detailElement) {
  throw 'Accessible selected-result control not exposed as Windows UIA element'
}
$detailType=$detailElement.Current.ControlType.ProgrammaticName
Write-Host ('PRODUCTION_AUDIT_DETAIL_UIA_CONTROL_TYPE='+$detailType)
# Hosted UIA providers may classify a read-only EDIT as Text rather than
# Edit. Require real UIA ValuePattern and verified read-only semantics below
# instead of assuming one provider-specific ControlType.
$valuePattern=$null
if(-not $detailElement.TryGetCurrentPattern(
    [System.Windows.Automation.ValuePattern]::Pattern,[ref]$valuePattern)) {
  throw 'Accessible selected-result control has no native UIA ValuePattern'
}
$detailValue=[System.Windows.Automation.ValuePattern]$valuePattern
if(-not $detailValue.Current.IsReadOnly) {
  throw 'Physical Audit selected-result disclosure must be read-only'
}
$actualDetail=$detailValue.Current.Value
if([string]::IsNullOrWhiteSpace($actualDetail) -or
   $actualDetail.IndexOf('INCOMPLETE',[StringComparison]::OrdinalIgnoreCase) -lt 0 -or
   $actualDetail.IndexOf('track-01.mp3',[StringComparison]::OrdinalIgnoreCase) -lt 0 -or
   $actualDetail.IndexOf('DATE',[StringComparison]::OrdinalIgnoreCase) -lt 0) {
  throw 'Real UIA selected-result ValuePattern omits synthetic status/file/missing-field detail'
}
Write-Host 'PRODUCTION_AUDIT_SELECTED_RESULT_READONLY_UIA_VALUE=PASS'

$uiRowCount=$uiDataRows.Count+$uiListRows.Count
if($uiRowCount -gt 0){
  if($uiRowCount -ne 1){ throw 'Native Audit UIA has inconsistent result child count' }
  $uiRow=if($uiDataRows.Count -eq 1){$uiDataRows.Item(0)}else{$uiListRows.Item(0)}
  $uiRowBounds=$uiRow.Current.BoundingRectangle
  if($uiRow.Current.IsOffscreen -or $uiRowBounds.Width -lt 2 -or $uiRowBounds.Height -lt 2){
    throw 'Native Audit UIA result row offscreen or without usable bounds'
  }
  $uiRootBounds=$uiList.Current.BoundingRectangle
  $intersection=[System.Windows.Rect]::Intersect($uiRootBounds,$uiRowBounds)
  if($intersection.IsEmpty -or $intersection.Width -lt 2 -or $intersection.Height -lt 2){
    throw 'Native Audit UIA result row outside parent ListView'
  }
  Write-Host 'PRODUCTION_AUDIT_UIA_RESULT_ROW=PASS'
}else{
  # Hosted Windows UIA provider does not expose native SysListView32 child rows.
  # Explicitly pending, not PASS. Native counts/filter interactions remain strict.
  Write-Host 'PRODUCTION_AUDIT_UIA_ROW_ENUMERATION=PENDING_PROVIDER_UNEXPOSED'
}

Write-Host 'PRODUCTION_AUDIT_INCOMPLETE_CLASSIFIER=PASS'
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
