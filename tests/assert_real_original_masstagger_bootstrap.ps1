#requires -Version 7.2
# CI-only original third-party native Masstagger UI evidence, not a tag-writing substitute.
[CmdletBinding()]
param(
  [Parameter(Mandatory)][int]$FoobarPid,
  [Parameter(Mandatory)][string]$Executable,
  [Parameter(Mandatory)][string]$FixturePath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if($env:GITHUB_ACTIONS -cne 'true' -or -not $env:RUNNER_TEMP){
  throw 'Original Masstagger native UI proof is restricted to disposable Windows Actions'
}
$root=[IO.Path]::GetFullPath($env:RUNNER_TEMP).TrimEnd('\')+'\'
$exe=[IO.Path]::GetFullPath($Executable)
$source=[IO.Path]::GetFullPath($FixturePath)
if(-not $exe.StartsWith($root,[StringComparison]::OrdinalIgnoreCase) -or
   -not $source.StartsWith($root,[StringComparison]::OrdinalIgnoreCase) -or
   [IO.Path]::GetFileName($exe) -cne 'foobar2000.exe' -or
   [IO.Path]::GetFileName($source) -cne 'track-01.mp3' -or
   [IO.Path]::GetFileName([IO.Path]::GetDirectoryName($source)) -cne 'embedded-cue-masstagger'){
  throw 'Refusing non-disposable original UI bootstrap paths'
}
$hostProcess=Get-Process -Id $FoobarPid -ErrorAction Stop
if($hostProcess.ProcessName -cne 'foobar2000' -or
   [IO.Path]::GetFullPath($hostProcess.Path) -cne $exe){
  throw 'Native bootstrap host identity mismatch'
}
$before=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$report=$source+'.metadata-isolation-probe.txt'
if(Test-Path -LiteralPath $report){throw 'Refusing stale native bootstrap diagnostic report'}

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;
public static class NativeOriginalMasstaggerBootstrapUI {
  delegate bool Callback(IntPtr hwnd, IntPtr ignored);
  [DllImport("user32.dll")] static extern bool EnumWindows(Callback callback,IntPtr state);
  [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr hwnd,Callback callback,IntPtr state);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowTextW(IntPtr hwnd,StringBuilder text,int max);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassNameW(IntPtr hwnd,StringBuilder text,int max);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
  [DllImport("user32.dll",EntryPoint="PostMessageW")] static extern bool PostMessage(IntPtr hwnd,uint msg,IntPtr wParam,IntPtr lParam);
  static string Caption(IntPtr h){var s=new StringBuilder(256);GetWindowTextW(h,s,s.Capacity);return s.ToString();}
  static string Class(IntPtr h){var s=new StringBuilder(64);GetClassNameW(h,s,s.Capacity);return s.ToString();}
  public static string[] Titles(int pid){
    var result=new List<string>();
    EnumWindows((h,x)=>{uint p;GetWindowThreadProcessId(h,out p);
      if(p==(uint)pid&&IsWindowVisible(h)){var name=Caption(h);
        if(name.Length>0)result.Add(name.Length>80?name.Substring(0,80):name);}
      return true;},IntPtr.Zero);
    return result.ToArray();
  }
  static IntPtr Find(int pid,string exact,bool masstagger){
    IntPtr found=IntPtr.Zero;int count=0;
    EnumWindows((h,x)=>{
      uint p;GetWindowThreadProcessId(h,out p);
      if(p!=(uint)pid||!IsWindowVisible(h))return true;
      string name=Caption(h);
      bool match=masstagger?(
        name.IndexOf("Masstagger",StringComparison.OrdinalIgnoreCase)>=0 ||
        name.IndexOf("Manage scripts",StringComparison.OrdinalIgnoreCase)>=0 ||
        name.IndexOf("Mass Tagger",StringComparison.OrdinalIgnoreCase)>=0):
        String.Equals(name,exact,StringComparison.Ordinal);
      if(match){found=h;count++;}
      return true;},IntPtr.Zero);
    return count==1?found:IntPtr.Zero;
  }
  public static IntPtr Masstagger(int pid){return Find(pid,"",true);}
  public static bool CloseKnown(int pid,string exact){
    var h=Find(pid,exact,false);if(h==IntPtr.Zero)return false;
    uint p;GetWindowThreadProcessId(h,out p);
    if(p!=(uint)pid||!PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero))
      throw new InvalidOperationException("Verified disposable UI could not close");
    return true;
  }
  public static string[] Controls(IntPtr h,int pid){
    uint p;GetWindowThreadProcessId(h,out p);
    if(h==IntPtr.Zero||p!=(uint)pid||!IsWindowVisible(h))
      throw new InvalidOperationException("Original Masstagger UI owner mismatch");
    var results=new List<string>();
    EnumChildWindows(h,(c,x)=>{uint owner;GetWindowThreadProcessId(c,out owner);
      if(owner!=(uint)pid)return true;
      string kind=Class(c);string label="";
      if(kind=="Button"||kind=="Static"){
        label=Caption(c);
        if(label.Length>64)label="<redacted>";
        foreach(char ch in label)if(ch=='\\'||ch=='/'||ch==':'){label="<redacted>";break;}
      }
      if(results.Count<64)results.Add(kind+":"+label);
      return true;},IntPtr.Zero);
    return results.ToArray();
  }
  public static void Close(IntPtr h,int pid){
    uint p;GetWindowThreadProcessId(h,out p);
    if(p!=(uint)pid||!PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero))
      throw new InvalidOperationException("Cannot close original disposable Masstagger UI");
  }
}
'@

$dialog=[IntPtr]::Zero
$observed=$false
try{
  for($i=0;$i -lt 90;$i++){
    $hostProcess.Refresh()
    if($hostProcess.HasExited){throw 'Original foobar host exited during native bootstrap'}
    [void][NativeOriginalMasstaggerBootstrapUI]::CloseKnown($FoobarPid,'Quick Appearance Setup')
    $dialog=[NativeOriginalMasstaggerBootstrapUI]::Masstagger($FoobarPid)
    if($dialog -ne [IntPtr]::Zero){$observed=$true;break}
    if(Test-Path -LiteralPath $report){break}
    Start-Sleep -Milliseconds 220
  }
  if(-not $observed){
    Write-Host ('MASSTAGGER_NATIVE_BOOTSTRAP_HOST_WINDOWS=' +
      ([NativeOriginalMasstaggerBootstrapUI]::Titles($FoobarPid) -join ';'))
    Write-Host 'MASSTAGGER_GENUINE_BOOTSTRAP_UI=PENDING_NO_ORIGINAL_WINDOW'
  }else{
    $controls=[NativeOriginalMasstaggerBootstrapUI]::Controls($dialog,$FoobarPid)
    Write-Host ('MASSTAGGER_GENUINE_BOOTSTRAP_UI_CONTROL_COUNT=' + $controls.Count)
    foreach($control in $controls){Write-Host ('MASSTAGGER_GENUINE_BOOTSTRAP_UI_CONTROL=' + $control)}
    Write-Host 'MASSTAGGER_GENUINE_BOOTSTRAP_UI=ORIGINAL_WINDOW_OBSERVED'
  }
}finally{
  if($dialog -ne [IntPtr]::Zero){
    [NativeOriginalMasstaggerBootstrapUI]::Close($dialog,$FoobarPid)
  }
  if((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -cne $before){
    throw 'Native original Masstagger UI read-only discovery modified embedded Cue MP3'
  }
  Write-Host 'MASSTAGGER_NATIVE_BOOTSTRAP_CUE_MP3_BYTE_EXACT=PASS'
}
for($i=0;$i -lt 55 -and -not (Test-Path -LiteralPath $report);$i++){
  Start-Sleep -Milliseconds 150
}
if(-not (Test-Path -LiteralPath $report)){
  Write-Host 'MASSTAGGER_NATIVE_BOOTSTRAP_SDK_REPORT=PENDING_NOT_EMITTED'
}else{
  $result=Get-Content -LiteralPath $report -Raw
  $safe=@( $result -split "[\r\n]+" | Where-Object {
    $_ -match '^(native_original_masstagger_bootstrap|native_original_context_executed|write_result)='
  })
  Write-Host ('MASSTAGGER_NATIVE_BOOTSTRAP_SDK_REPORT=' + ($safe -join ';'))
  if($observed -and
     $result -match '(?m)^native_original_context_executed=Manage scripts\s*$' -and
     $result -match '(?m)^write_result=original_ui_returned\s*$'){
    Write-Host 'MASSTAGGER_NATIVE_BOOTSTRAP_REAL_ORIGINAL_MANAGE_SCRIPTS_UI=PASS_READ_ONLY'
  }else{
    Write-Host 'MASSTAGGER_NATIVE_BOOTSTRAP_REAL_ORIGINAL_MANAGE_SCRIPTS_UI=PENDING'
  }
}

# The CI-only native startup probe must leave an independently observable
# bounded phase trace even when the original modal UI cannot open. No file
# paths, tag text, private presets or exception messages are ever printed.
$phaseTrace=Join-Path $env:RUNNER_TEMP 'mi-original-masstagger-bootstrap-trace.txt'
$allowed=@(
  'INIT_ENTERED','NO_FIXTURE_ENV','INVALID_ENV_LENGTH',
  'INVALID_CANONICAL_PATH','REFUSED_PATH_SCOPE','REFUSED_FILE_ATTRIBUTES',
  'SCHEDULED_MAIN_THREAD','CALLBACK_ENTERED','INVALID_SYNTHETIC_SUBSONGS',
  'ORIGINAL_COMMAND_NOT_FOUND','ORIGINAL_COMMAND_EXECUTE_ENTERED',
  'ORIGINAL_COMMAND_EXECUTE_RETURNED','BOOTSTRAP_EXCEPTION',
  'REPORT_WRITTEN','REPORT_WRITE_FAILED'
)
if(-not (Test-Path -LiteralPath $phaseTrace)){
  Write-Host 'MASSTAGGER_NATIVE_BOOTSTRAP_PHASES=NOT_EMITTED'
  throw 'CI-only native bootstrap startup trace absent; not qualified as a runtime probe'
}
$phases=@(Get-Content -LiteralPath $phaseTrace | Where-Object {$_ -cin $allowed})
Write-Host ('MASSTAGGER_NATIVE_BOOTSTRAP_PHASES=' + ($phases -join ','))
if($phases.Count -eq 0 -or $phases[0] -cne 'INIT_ENTERED'){
  throw 'CI-only native bootstrap did not reach its guarded initialization entry'
}
if(@($phases | Where-Object {$_ -cin @('NO_FIXTURE_ENV','INVALID_ENV_LENGTH','INVALID_CANONICAL_PATH','REFUSED_PATH_SCOPE','REFUSED_FILE_ATTRIBUTES')}).Count -gt 0){
  throw 'CI-only native bootstrap rejected its disposable fixture before SDK dispatch'
}
if(@($phases | Where-Object {$_ -ceq 'SCHEDULED_MAIN_THREAD'}).Count -ne 1){
  throw 'CI-only native bootstrap was not scheduled on foobar main thread'
}

Write-Host 'MASSTAGGER_NATIVE_BOOTSTRAP_POSITIVE_PHYSICAL_COMMIT=PENDING_EXPLICIT_UI_ACTION'
