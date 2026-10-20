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
  throw 'Real timing UIA test must run on a disposable GitHub Windows host'
}
$root=[IO.Path]::GetFullPath($env:RUNNER_TEMP).TrimEnd('\') + '\'
$source=[IO.Path]::GetFullPath($FixturePath)
if (-not $source.StartsWith($root,[StringComparison]::OrdinalIgnoreCase) -or
    [IO.Path]::GetFileName($source) -cne 'track-01.mp3') {
  throw 'Refusing non-disposable timing media'
}
if (-not (Test-Path -LiteralPath $AdapterPath) -or
    -not (Test-Path -LiteralPath $source)) {
  throw 'Required synthetic timing source or common UIA adapter missing'
}
. $AdapterPath
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class MetadataIsolationTimingWindowLocator {
  public delegate bool Visitor(IntPtr window,IntPtr state);
  [DllImport("user32.dll")] public static extern bool EnumWindows(Visitor callback,IntPtr state);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr hwnd,StringBuilder text,int capacity);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd,int id);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hwnd);
  [DllImport("user32.dll",EntryPoint="SendMessageTimeoutW",SetLastError=true)]
  static extern IntPtr SendMessageTimeout(IntPtr hwnd,uint message,IntPtr wParam,IntPtr lParam,uint flags,uint timeout,out IntPtr result);
  public static long Send(IntPtr hwnd,uint message) {
    IntPtr result;
    if (hwnd==IntPtr.Zero || SendMessageTimeout(hwnd,message,IntPtr.Zero,IntPtr.Zero,2,3000,out result)==IntPtr.Zero)
      throw new InvalidOperationException("Synthetic timing control did not respond");
    return result.ToInt64();
  }
  public static string Text(IntPtr hwnd) {
    if(hwnd==IntPtr.Zero) throw new InvalidOperationException("Timing control missing");
    var builder=new StringBuilder(512);
    GetWindowTextW(hwnd,builder,builder.Capacity);
    return builder.ToString();
  }
  public static IntPtr Find(int pid,string title) {
    IntPtr result=IntPtr.Zero; int matches=0;
    EnumWindows((hwnd,ignored)=>{
      uint owner; GetWindowThreadProcessId(hwnd,out owner);
      if(owner!=(uint)pid || !IsWindowVisible(hwnd))return true;
      var builder=new StringBuilder(256);
      GetWindowTextW(hwnd,builder,builder.Capacity);
      if(String.Equals(builder.ToString(),title,StringComparison.Ordinal)){matches++;result=hwnd;}
      return true;
    },IntPtr.Zero);
    return matches==1?result:IntPtr.Zero;
  }
}
'@
$before=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$marker=$source+'.metadata-isolation-timing-ui-fixture.txt'
[IO.File]::WriteAllText($marker,'MI-DISPOSABLE-FOOBAR-TIMING-UI-V1',[Text.UTF8Encoding]::new($false))
$launched=$null
$window=[IntPtr]::Zero
try{
  $launched=Start-Process -FilePath $Executable -WorkingDirectory $Workdir -ArgumentList @(
    '/metadata-isolation-probe-open-real-virtual-timing',('"' + $source + '"')) -PassThru
  for($retry=0;$retry -lt 100;$retry++){
    $window=[MetadataIsolationTimingWindowLocator]::Find($FoobarPid,'Metadata Isolation - Virtual timing')
    if($window -ne [IntPtr]::Zero){break}
    Start-Sleep -Milliseconds 200
  }
  if($window -eq [IntPtr]::Zero){throw 'The actual Virtual timing dialog did not open on a real foobar host'}
  $result=Test-WindowsUiAccessibility -WindowHandle $window -RequiredNames @(
    'Review...','Cancel','Remove this optional directive')
  if(-not $result.Passed){throw ('Real Virtual timing UIA geometry invalid: '+($result.Issues -join ','))}
  Write-Host 'PRODUCTION_VIRTUAL_TIMING_UIA_GEOMETRY=PASS'
  $track=[MetadataIsolationTimingWindowLocator]::Text(
    [MetadataIsolationTimingWindowLocator]::GetDlgItem($window,1101))
  if($track -cne 'Stable track identity: Cue TRACK 2'){
    throw 'Real timing dialog track identity mismatch'
  }
  $combo=[MetadataIsolationTimingWindowLocator]::GetDlgItem($window,1102)
  if([MetadataIsolationTimingWindowLocator]::Send($combo,0x0146) -ne 101 -or
     [MetadataIsolationTimingWindowLocator]::Send($combo,0x0147) -ne 0){
    throw 'Real timing directive combobox missing its qualified choices'
  }
  $current=[MetadataIsolationTimingWindowLocator]::Text(
    [MetadataIsolationTimingWindowLocator]::GetDlgItem($window,1103))
  # GetWindowTextW cannot read EDIT control text across process boundaries.
  # Use the native Windows UIA ValuePattern, as a real accessibility client.
  $editHwnd=[MetadataIsolationTimingWindowLocator]::GetDlgItem($window,1104)
  $editUi=[System.Windows.Automation.AutomationElement]::FromHandle($editHwnd)
  if($null -eq $editUi){throw 'The real Virtual timing edit field is not accessible'}
  $valuePattern=$editUi.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
  $edit=$valuePattern.Current.Value
  if($current -cne '00:00:30' -or $edit -cne $current){
    throw 'Real timing dialog did not load exact 75-frame INDEX 01 preimage'
  }
  $removal=[MetadataIsolationTimingWindowLocator]::GetDlgItem($window,1105)
  if([MetadataIsolationTimingWindowLocator]::IsWindowEnabled($removal)){
    throw 'Mandatory INDEX 01 incorrectly removable'
  }
  Write-Host 'PRODUCTION_VIRTUAL_TIMING_NATIVE_PREIMAGE_AND_RULES=PASS'
}finally{
  if($window -ne [IntPtr]::Zero -and [MetadataIsolationTimingWindowLocator]::IsWindow($window)){
    $cancel=[MetadataIsolationTimingWindowLocator]::GetDlgItem($window,2)
    if($cancel -ne [IntPtr]::Zero){[void][MetadataIsolationTimingWindowLocator]::Send($cancel,0x00F5)}
  }
}
if($window -eq [IntPtr]::Zero){throw 'The actual timing dialog was never created'}
for($retry=0;$retry -lt 40;$retry++){
  if(-not [MetadataIsolationTimingWindowLocator]::IsWindow($window)){break}
  Start-Sleep -Milliseconds 100
}
if([MetadataIsolationTimingWindowLocator]::IsWindow($window)){
  throw 'Timing editor Cancel did not dismiss the actual modal dialog'
}
Write-Host 'PRODUCTION_VIRTUAL_TIMING_CANCEL_NO_COMMIT=PASS'
if((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $before){
  throw 'Cancelled real Virtual timing dialog modified original media'
}
Write-Host 'PRODUCTION_VIRTUAL_TIMING_SYNTHETIC_MEDIA_UNCHANGED=PASS'
