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
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr window,Visitor callback,IntPtr state);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr hwnd,StringBuilder text,int capacity);
  public static IntPtr FindDescendantButton(IntPtr window,string caption) {
    IntPtr match=IntPtr.Zero; int count=0;
    EnumChildWindows(window,(hwnd,ignored)=>{
      var cls=new StringBuilder(64);
      GetClassNameW(hwnd,cls,cls.Capacity);
      if(String.Equals(cls.ToString(),"Button",StringComparison.OrdinalIgnoreCase) &&
         String.Equals(Text(hwnd),caption,StringComparison.Ordinal)){
        match=hwnd; count++;
      }
      return true;
    },IntPtr.Zero);
    return count==1?match:IntPtr.Zero;
  }
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
  [DllImport("user32.dll",EntryPoint="SendMessageTimeoutW",CharSet=CharSet.Unicode,SetLastError=true)]
  static extern IntPtr SendMessageTimeoutText(IntPtr hwnd,uint message,IntPtr wParam,
    StringBuilder output,uint flags,uint timeout,out IntPtr result);
  public static string TextByNativeMessage(IntPtr hwnd) {
    if(hwnd==IntPtr.Zero)throw new InvalidOperationException("Timing EDIT control missing");
    var output=new StringBuilder(128);
    IntPtr result;
    if(SendMessageTimeoutText(hwnd,0x000D,new IntPtr(output.Capacity),output,2,3000,out result)==IntPtr.Zero)
      throw new InvalidOperationException("Native EDIT WM_GETTEXT timed out");
    return output.ToString();
  }
  public static string Text(IntPtr hwnd) {
    if(hwnd==IntPtr.Zero) throw new InvalidOperationException("Timing control missing");
    var builder=new StringBuilder(512);
    GetWindowTextW(hwnd,builder,builder.Capacity);
    return builder.ToString();
  }
  // SetWindowTextW cannot modify an EDIT control owned by another process.
  // WM_SETTEXT is a system-defined message: Windows marshals its UTF-16 text
  // across the process boundary without remote memory or injected code.
  [DllImport("user32.dll",EntryPoint="SendMessageTimeoutW",CharSet=CharSet.Unicode,SetLastError=true)]
  static extern IntPtr SendMessageTimeoutSetText(IntPtr hwnd,uint message,IntPtr wparam,
    [MarshalAs(UnmanagedType.LPWStr)] string value,uint flags,uint timeout,out IntPtr result);
  [DllImport("user32.dll",EntryPoint="PostMessageW",SetLastError=true)]
  static extern bool PostMessageW(IntPtr hwnd,uint msg,IntPtr wparam,IntPtr lparam);
  public static void SetText(IntPtr hwnd,string value){
    if(hwnd==IntPtr.Zero)
      throw new InvalidOperationException("Timing UI EDIT control missing");
    IntPtr result;
    if(SendMessageTimeoutSetText(hwnd,0x000C,IntPtr.Zero,value,2,3000,out result)==IntPtr.Zero ||
       result==IntPtr.Zero)
      throw new InvalidOperationException("Timing UI EDIT rejected cross-process WM_SETTEXT");
  }
  public static void ClickAsync(IntPtr hwnd){
    if(hwnd==IntPtr.Zero || !PostMessageW(hwnd,0x00F5,IntPtr.Zero,IntPtr.Zero))
      throw new InvalidOperationException("Timing UI Review button not clickable");
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
  # This hosted UIA provider has no ValuePattern on native EDIT controls.
  # WM_GETTEXT is a system-defined, cross-process marshalled Win32 message.
  $editHwnd=[MetadataIsolationTimingWindowLocator]::GetDlgItem($window,1104)
  $edit=[MetadataIsolationTimingWindowLocator]::TextByNativeMessage($editHwnd)
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

# Second independent dialog attempt: change INDEX 01, reach actual review,
# then explicitly discard. No actual source mutation is permitted.
$launched=Start-Process -FilePath $Executable -WorkingDirectory $Workdir -ArgumentList @(
  '/metadata-isolation-probe-open-real-virtual-timing',('"' + $source + '"')) -PassThru
$window=[IntPtr]::Zero
for($retry=0;$retry -lt 100;$retry++){
  $window=[MetadataIsolationTimingWindowLocator]::Find($FoobarPid,'Metadata Isolation - Virtual timing')
  if($window -ne [IntPtr]::Zero){break}
  Start-Sleep -Milliseconds 200
}
if($window -eq [IntPtr]::Zero){throw 'Real Virtual timing editor did not reopen for review/discard'}
$editHwnd=[MetadataIsolationTimingWindowLocator]::GetDlgItem($window,1104)
if($editHwnd -eq [IntPtr]::Zero){throw 'Real Virtual timing edit field absent'}
[MetadataIsolationTimingWindowLocator]::SetText($editHwnd,'00:00:35')
if([MetadataIsolationTimingWindowLocator]::TextByNativeMessage($editHwnd) -cne '00:00:35'){
  throw 'Real timing editor did not accept changed CD frame value'
}
[MetadataIsolationTimingWindowLocator]::ClickAsync(
  [MetadataIsolationTimingWindowLocator]::GetDlgItem($window,1))
$review=[IntPtr]::Zero
for($retry=0;$retry -lt 160;$retry++){
  $review=[MetadataIsolationTimingWindowLocator]::Find($FoobarPid,'Metadata Isolation')
  if($review -ne [IntPtr]::Zero){break}
  Start-Sleep -Milliseconds 150
}
if($review -eq [IntPtr]::Zero){
  throw 'Actual timing change failed to reach native Commit/Discard review'
}
$reviewRoot=[System.Windows.Automation.AutomationElement]::FromHandle($review)
if($null -eq $reviewRoot){throw 'Actual timing review has no accessible root'}
$buttons=$reviewRoot.FindAll(
  [System.Windows.Automation.TreeScope]::Descendants,
  [System.Windows.Automation.PropertyCondition]::new(
    [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
    [System.Windows.Automation.ControlType]::Button))
$commitFound=$false
$discardButton=$null
foreach($control in $buttons){
  if($control.Current.Name -ceq 'Commit'){$commitFound=$true}
  if($control.Current.Name -ceq 'Discard'){$discardButton=$control}
}
# TaskDialog uses IDOK/IDCANCEL custom buttons. Verify their actual native
# captions even if the hosted UIA provider does not enumerate its children.
# A generic error dialog titled "Metadata Isolation" must not be mistaken for
# a valid review; it will not have both exact custom captions.
$commitHwnd=[MetadataIsolationTimingWindowLocator]::FindDescendantButton($review,'Commit')
$discardHwnd=[MetadataIsolationTimingWindowLocator]::FindDescendantButton($review,'Discard')
if($commitHwnd -eq [IntPtr]::Zero -or $discardHwnd -eq [IntPtr]::Zero){
  # Report only bounded, generic UI labels from the disposable host.
  $all=$reviewRoot.FindAll(
    [System.Windows.Automation.TreeScope]::Descendants,
    [System.Windows.Automation.Condition]::TrueCondition)
  $summary=@(for($i=0;$i -lt [Math]::Min(50,$all.Count);$i++){
    $item=$all.Item($i)
    $name=$item.Current.Name
    if($name -match '^(Commit|Discard|OK|Cancel|Close|Review virtual Cue timing change)$'){
      $item.Current.ControlType.ProgrammaticName+':'+$name
    }
  }) -join ','
  throw ("Actual review has no unique native Commit/Discard buttons; UIA labels='{0}'" -f $summary)
}
$nativeCommit=[MetadataIsolationTimingWindowLocator]::Text($commitHwnd)
$nativeDiscard=[MetadataIsolationTimingWindowLocator]::Text($discardHwnd)
if($nativeCommit -cne 'Commit' -or $nativeDiscard -cne 'Discard'){
  throw 'Native timing review labels changed between discovery and validation'
}
Write-Host 'PRODUCTION_VIRTUAL_TIMING_REVIEW_NATIVE_ACTIONS=PASS'
# This is independent of the hosted UIA provider. Verify actual button
# accessible names and roles through oleacc/MSAA, not Win32 captions alone.
& ./tests/assert_timing_review_native_msaa.ps1 -FoobarPid $FoobarPid -ReviewHwnd $review -CommitHwnd $commitHwnd -DiscardHwnd $discardHwnd

if($commitFound -and $null -ne $discardButton){
  $invoke=$null
  if($discardButton.TryGetCurrentPattern(
    [System.Windows.Automation.InvokePattern]::Pattern,[ref]$invoke)){
    ([System.Windows.Automation.InvokePattern]$invoke).Invoke()
    Write-Host 'PRODUCTION_VIRTUAL_TIMING_REVIEW_UIA_INVOKE=PASS'
  }else{
    [void][MetadataIsolationTimingWindowLocator]::Send($discardHwnd,0x00F5)
    Write-Host 'PRODUCTION_VIRTUAL_TIMING_REVIEW_UIA_INVOKE=PENDING_PROVIDER_UNEXPOSED'
  }
}else{
  # A real native click is evidence for Discard, not evidence for UIA exposure.
  [void][MetadataIsolationTimingWindowLocator]::Send($discardHwnd,0x00F5)
  Write-Host 'PRODUCTION_VIRTUAL_TIMING_REVIEW_UIA_BUTTONS=PENDING_PROVIDER_UNEXPOSED'
}
for($retry=0;$retry -lt 80;$retry++){
  if(-not [MetadataIsolationTimingWindowLocator]::IsWindow($review)){break}
  Start-Sleep -Milliseconds 100
}
if([MetadataIsolationTimingWindowLocator]::IsWindow($review)){
  throw 'Actual Discard command did not close the review'
}
if((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $before){
  throw 'Discarding actual Virtual timing preview modified source MP3'
}
Write-Host 'PRODUCTION_VIRTUAL_TIMING_REVIEW_DISCARD_BYTE_EXACT=PASS'

# Positive production UI path: commit only to the same exact synthetic file.
# The previous Cancel and Discard subtests must both have proved byte identity.
# Do not use a diagnostic commit to manufacture acceptance for the native UI.
if($before -ine 'd0a39dff211bebb856749ee209e55a16f23692ae39b2f9181ebae18997731219'){
  throw 'Live UI Commit refuses unknown or modified synthetic timing source'
}
$commitBefore=$source+'.before-real-ui-commit'
if(Test-Path -LiteralPath $commitBefore){throw 'Live UI Commit refuses stale oracle preimage'}
Copy-Item -LiteralPath $source -Destination $commitBefore -ErrorAction Stop
$launched=Start-Process -FilePath $Executable -WorkingDirectory $Workdir -ArgumentList @(
  '/metadata-isolation-probe-open-real-virtual-timing',('"' + $source + '"')) -PassThru
$window=[IntPtr]::Zero
for($retry=0;$retry -lt 100;$retry++){
  $window=[MetadataIsolationTimingWindowLocator]::Find($FoobarPid,'Metadata Isolation - Virtual timing')
  if($window -ne [IntPtr]::Zero){break}
  Start-Sleep -Milliseconds 200
}
if($window -eq [IntPtr]::Zero){throw 'Actual Virtual timing dialog did not open for positive UI Commit'}
$editHwnd=[MetadataIsolationTimingWindowLocator]::GetDlgItem($window,1104)
if([MetadataIsolationTimingWindowLocator]::TextByNativeMessage($editHwnd) -cne '00:00:30'){
  throw 'Positive UI Commit preimage does not match the original INDEX 01'
}
[MetadataIsolationTimingWindowLocator]::SetText($editHwnd,'00:00:35')
if([MetadataIsolationTimingWindowLocator]::TextByNativeMessage($editHwnd) -cne '00:00:35'){
  throw 'Positive UI Commit editor did not accept requested CD frames'
}
[MetadataIsolationTimingWindowLocator]::ClickAsync(
  [MetadataIsolationTimingWindowLocator]::GetDlgItem($window,1))
$review=[IntPtr]::Zero
for($retry=0;$retry -lt 160;$retry++){
  $review=[MetadataIsolationTimingWindowLocator]::Find($FoobarPid,'Metadata Isolation')
  if($review -ne [IntPtr]::Zero){break}
  Start-Sleep -Milliseconds 150
}
if($review -eq [IntPtr]::Zero){throw 'Positive UI Commit did not reach the real native review'}
$commitButton=[MetadataIsolationTimingWindowLocator]::FindDescendantButton($review,'Commit')
$discardButton=[MetadataIsolationTimingWindowLocator]::FindDescendantButton($review,'Discard')
if($commitButton -eq [IntPtr]::Zero -or $discardButton -eq [IntPtr]::Zero){
  throw 'Positive UI Commit refuses review without unique native Commit/Discard buttons'
}
if((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $before){
  throw 'Original media changed before native Commit action'
}
[MetadataIsolationTimingWindowLocator]::ClickAsync($commitButton)
for($retry=0;$retry -lt 200;$retry++){
  if(-not [MetadataIsolationTimingWindowLocator]::IsWindow($review) -and
     (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $before){break}
  Start-Sleep -Milliseconds 150
}
if([MetadataIsolationTimingWindowLocator]::IsWindow($review)){
  throw 'Native positive Commit did not close the actual review'
}
if((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -eq $before){
  throw 'Native positive Commit produced no change to the synthetic MP3'
}
# Independent raw-byte oracle: no component-provided success flag, no
# simulated postimage, and not even a reserialization of unrelated tag bytes.
& python tests/assert_real_virtual_timing_ui_commit.py $commitBefore $source
if($LASTEXITCODE -ne 0){throw 'Native UI Commit failed independent raw-MP3 preservation oracle'}
Write-Host 'PRODUCTION_VIRTUAL_TIMING_REAL_UI_COMMIT=PASS'


