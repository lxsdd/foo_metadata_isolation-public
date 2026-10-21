#requires -Version 7.2
# Read-only discovery of original Masstagger UI on disposable original foobar2000.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][int]$FoobarPid,
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$Workdir,
    [Parameter(Mandatory)][string]$FixturePath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if($env:GITHUB_ACTIONS -cne 'true' -or -not $env:RUNNER_TEMP){
    throw 'Only disposable Windows Actions runners may run original Masstagger UI diagnostics'
}
$runner=[IO.Path]::GetFullPath($env:RUNNER_TEMP).TrimEnd('\')+'\'
$source=[IO.Path]::GetFullPath($FixturePath)
$exepath=[IO.Path]::GetFullPath($Executable)
if(-not $source.StartsWith($runner,[StringComparison]::OrdinalIgnoreCase) -or
   -not $exepath.StartsWith($runner,[StringComparison]::OrdinalIgnoreCase) -or
   [IO.Path]::GetFileName($source) -cne 'track-01.mp3' -or
   [IO.Path]::GetFileName($exepath) -cne 'foobar2000.exe'){
    throw 'Refusing original Masstagger UI discovery on non-disposable paths'
}
$hostProcess=Get-Process -Id $FoobarPid -ErrorAction Stop
if($hostProcess.ProcessName -cne 'foobar2000' -or
   [IO.Path]::GetFullPath($hostProcess.Path) -cne $exepath){
    throw 'Original Masstagger UI diagnostic host identity mismatch'
}
$before=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash

Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class OriginalMasstaggerUiDiscovery {
  public delegate bool Visit(IntPtr hwnd, IntPtr state);
  [DllImport("user32.dll")] static extern bool EnumWindows(Visit visitor,IntPtr state);
  [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr root,Visit visitor,IntPtr state);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowTextW(IntPtr hwnd,StringBuilder value,int max);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassNameW(IntPtr hwnd,StringBuilder value,int max);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
  [DllImport("user32.dll",EntryPoint="PostMessageW")] static extern bool PostMessage(IntPtr hwnd,uint msg,IntPtr wp,IntPtr lp);
  static string Label(IntPtr h) {var s=new StringBuilder(256);GetWindowTextW(h,s,s.Capacity);return s.ToString();}
  static string Class(IntPtr h) {var s=new StringBuilder(120);GetClassNameW(h,s,s.Capacity);return s.ToString();}
  public static string[] WindowTitles(int pid){
    var result=new List<string>();
    EnumWindows((h,ignored)=>{
      uint owner;GetWindowThreadProcessId(h,out owner);
      if(owner==(uint)pid && IsWindowVisible(h)){
        var name=Label(h);
        if(name.Length>0)result.Add(name.Length>80?name.Substring(0,80):name);
      }
      return true;
    },IntPtr.Zero);
    return result.ToArray();
  }
  public static IntPtr FindExactTitle(int pid,string wanted){
    IntPtr found=IntPtr.Zero;int count=0;
    EnumWindows((h,ignored)=>{
      uint owner;GetWindowThreadProcessId(h,out owner);
      if(owner==(uint)pid && IsWindowVisible(h) &&
         String.Equals(Label(h),wanted,StringComparison.Ordinal)){
        count++;found=h;
      }
      return true;
    },IntPtr.Zero);
    return count==1?found:IntPtr.Zero;
  }
  public static IntPtr FindMasstagger(int pid){
    IntPtr found=IntPtr.Zero;int count=0;
    EnumWindows((h,ignored)=>{
      uint owner;GetWindowThreadProcessId(h,out owner);
      if(owner!=(uint)pid || !IsWindowVisible(h))return true;
      string caption=Label(h);
      if(caption.IndexOf("Masstagger",StringComparison.OrdinalIgnoreCase)>=0 ||
         caption.IndexOf("Manage Scripts",StringComparison.OrdinalIgnoreCase)>=0 ||
         caption.IndexOf("Mass Tagger",StringComparison.OrdinalIgnoreCase)>=0){
         count++;found=h;
      }
      return true;
    },IntPtr.Zero);
    return count==1?found:IntPtr.Zero;
  }
  public static string[] Controls(IntPtr root,int pid){
    uint owner;GetWindowThreadProcessId(root,out owner);
    if(root==IntPtr.Zero || owner!=(uint)pid || !IsWindowVisible(root))
      throw new InvalidOperationException("Masstagger window not owned by isolated host");
    var result=new List<string>();
    EnumChildWindows(root,(h,ignored)=>{
      uint childPid;GetWindowThreadProcessId(h,out childPid);
      if(childPid!=(uint)pid)return true;
      var c=Class(h);
      var label=Label(h);
      // Never dump edit contents, file paths, previews or private tags.
      if(c.IndexOf("Edit",StringComparison.OrdinalIgnoreCase)>=0)label="<redacted>";
      else if(c.Equals("Button",StringComparison.OrdinalIgnoreCase) ||
              c.Equals("Static",StringComparison.OrdinalIgnoreCase)) {
        if(label.Length>72)label=label.Substring(0,72);
        foreach(var ch in label) {
          if(ch=='\\' || ch=='/' || ch==':') {label="<redacted>";break;}
        }
      } else label="";
      if(result.Count<96)result.Add(c+":"+label);
      return true;
    },IntPtr.Zero);
    return result.ToArray();
  }
  public static void Close(IntPtr hwnd,int pid){
    uint owner;GetWindowThreadProcessId(hwnd,out owner);
    if(owner!=(uint)pid)throw new InvalidOperationException("Refusing other-process window close");
    if(!PostMessage(hwnd,0x0010,IntPtr.Zero,IntPtr.Zero))
      throw new InvalidOperationException("Cannot close disposable Masstagger UI");
  }
}
'@

$launcher=$null
$dialog=[IntPtr]::Zero
try{
    # Genuine first-run foobar2000 2.26.1 launches its Quick Appearance
    # Setup as a modal window on a NEW disposable profile. It can prevent
    # the documented context command reaching Masstagger. Dismiss ONLY the
    # uniquely titled, real modal in the already PID/path-verified host.
    $intro=[OriginalMasstaggerUiDiscovery]::FindExactTitle($FoobarPid,'Quick Appearance Setup')
    if($intro -ne [IntPtr]::Zero){
        [OriginalMasstaggerUiDiscovery]::Close($intro,$FoobarPid)
        for($attempt=0;$attempt -lt 30;$attempt++){
            Start-Sleep -Milliseconds 150
            if([OriginalMasstaggerUiDiscovery]::FindExactTitle($FoobarPid,'Quick Appearance Setup') -eq [IntPtr]::Zero){break}
        }
        if([OriginalMasstaggerUiDiscovery]::FindExactTitle($FoobarPid,'Quick Appearance Setup') -ne [IntPtr]::Zero){
            throw 'Disposable initial foobar Quick Appearance modal cannot be safely dismissed'
        }
        Write-Host 'MASSTAGGER_DISPOSABLE_HOST_FIRST_RUN_MODAL_DISMISSED=PASS'
    }
    # foobar2000 documented genuine context-menu CLI, not our own probe.
    $launcher=Start-Process -FilePath $exepath -WorkingDirectory $Workdir -PassThru -ArgumentList @('/context_command:"Manage Scripts"',('"' + $source + '"'))
    if(-not $launcher.WaitForExit(25000)){
        throw 'Original foobar context_command launcher did not terminate'
    }
    if($launcher.ExitCode -ne 0){
        throw 'Original foobar context_command launcher failed'
    }
    for($attempt=0;$attempt -lt 100;$attempt++){
        $dialog=[OriginalMasstaggerUiDiscovery]::FindMasstagger($FoobarPid)
        if($dialog -ne [IntPtr]::Zero){break}
        Start-Sleep -Milliseconds 150
    }
    if($dialog -eq [IntPtr]::Zero){
        $tops=[OriginalMasstaggerUiDiscovery]::WindowTitles($FoobarPid)
        Write-Host ('MASSTAGGER_HOST_TOP_LEVEL_TITLES=' + ($tops -join ';'))
        throw 'MASSTAGGER_MANAGE_SCRIPTS_NOT_OBSERVED: original UI did not open via physical context_command'
    }
    $controls=[OriginalMasstaggerUiDiscovery]::Controls($dialog,$FoobarPid)
    Write-Host ('MASSTAGGER_REAL_UI_CONTROL_COUNT=' + $controls.Count)
    foreach($line in $controls){Write-Host ('MASSTAGGER_REAL_UI_CONTROL=' + $line)}
    Write-Host 'MASSTAGGER_EMBEDDED_CUE_GENUINE_MANAGE_SCRIPTS_UI=PASS_OPENED_READ_ONLY'
    Write-Host 'MASSTAGGER_EMBEDDED_CUE_PHYSICAL_NON_NOOP=PENDING_REAL_UI_ACTION'
}finally{
    if($dialog -ne [IntPtr]::Zero){
        [OriginalMasstaggerUiDiscovery]::Close($dialog,$FoobarPid)
        Start-Sleep -Milliseconds 250
    }
    if((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -cne $before){
        throw 'Read-only Masstagger UI discovery unexpectedly modified embedded Cue MP3'
    }
    Write-Host 'MASSTAGGER_READ_ONLY_UI_DISCOVERY_MP3_BYTE_EXACT=PASS'
}
