#requires -Version 7.2
[CmdletBinding()]
param(
  [Parameter(Mandatory)][int]$FoobarPid,
  [Parameter(Mandatory)][IntPtr]$ReviewHwnd,
  [Parameter(Mandatory)][string]$FixturePath,
  [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$PreimageSha256
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if($env:GITHUB_ACTIONS -cne 'true' -or -not $env:RUNNER_TEMP){
  throw 'System Enter test is restricted to a disposable GitHub Windows desktop'
}
$root=[IO.Path]::GetFullPath($env:RUNNER_TEMP).TrimEnd('\')+'\'
$path=[IO.Path]::GetFullPath($FixturePath)
if(-not $path.StartsWith($root,[StringComparison]::OrdinalIgnoreCase) -or
   [IO.Path]::GetFileName($path) -cne 'track-01.mp3' -or
   -not (Test-Path -LiteralPath $path -PathType Leaf)){
  throw 'Real Enter test refuses media outside the disposable timing fixture'
}
# Refuse Windows reparse points even inside RUNNER_TEMP; their targets could
# escape the isolated fixture directory.
if (((Get-Item -LiteralPath $path -Force).Attributes -band
     [IO.FileAttributes]::ReparsePoint) -ne 0) {
  throw 'Synthetic keyboard fixture must not be a symlink or reparse point'
}
$expected=$PreimageSha256.ToLowerInvariant()
$actual=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
if($actual -cne $expected -or
   $actual -cne 'd0a39dff211bebb856749ee209e55a16f23692ae39b2f9181ebae18997731219'){
  throw 'Real Enter test refuses a non-canonical or changed fixture'
}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Threading;
public static class MetadataIsolationEnterKey {
  [DllImport("user32.dll")] static extern bool IsWindow(IntPtr hwnd);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
  [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr hwnd);
  [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
  [DllImport("user32.dll",EntryPoint="keybd_event")]
  static extern void KeybdEvent(byte vk,byte scan,uint flags,IntPtr extraInfo);
  public static bool Exists(IntPtr hwnd){return hwnd!=IntPtr.Zero && IsWindow(hwnd);}
  public static void PressEnterOnRealReview(IntPtr hwnd,uint ownerPid){
    if(!Exists(hwnd) || !IsWindowVisible(hwnd))
      throw new InvalidOperationException("Real review window unavailable");
    uint observed;
    if(GetWindowThreadProcessId(hwnd,out observed)==0 || observed!=ownerPid)
      throw new InvalidOperationException("Review is not owned by the isolated foobar PID");
    for(int n=0;n<12 && GetForegroundWindow()!=hwnd;n++){
      SetForegroundWindow(hwnd);
      Thread.Sleep(150);
    }
    if(GetForegroundWindow()!=hwnd)
      throw new InvalidOperationException("Cannot prove real review is foreground; no key injected");
    KeybdEvent(0x0d,0,0,IntPtr.Zero);  // VK_RETURN key down
    Thread.Sleep(80);
    KeybdEvent(0x0d,0,2,IntPtr.Zero);  // VK_RETURN key up
  }
}
'@
[MetadataIsolationEnterKey]::PressEnterOnRealReview($ReviewHwnd,[uint32]$FoobarPid)
Write-Host 'PRODUCTION_VIRTUAL_TIMING_REVIEW_REAL_ENTER_INPUT_DELIVERED=PASS'
$closed=$false
for($i=0;$i -lt 100;$i++){
  if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expected){
    throw 'Real native Enter triggered an unauthorized media change'
  }
  if(-not [MetadataIsolationEnterKey]::Exists($ReviewHwnd)){
    $closed=$true
    break
  }
  Start-Sleep -Milliseconds 100
}
if(-not $closed){throw 'Real native Enter did not dismiss default Discard review'}
Start-Sleep -Milliseconds 500
if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expected){
  throw 'Real native Enter review modified media asynchronously'
}
Write-Host 'PRODUCTION_VIRTUAL_TIMING_REVIEW_REAL_ENTER_DISCARD_BYTE_EXACT=PASS'
