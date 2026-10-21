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
    # First verify that genuine external foobar CLI dispatch reaches the
    # existing read-only in-component inventory probe on this synthetic CUE.
    $report=$source+'.metadata-isolation-probe.txt'
    if(Test-Path -LiteralPath $report){throw 'Refusing stale diagnostic probe report'}
    $inventoryLauncher=Start-Process -FilePath $exepath -WorkingDirectory $Workdir -PassThru -ArgumentList @('/metadata-isolation-probe-embedded-cue-inventory',('"' + $source + '"'))
    if(-not $inventoryLauncher.WaitForExit(25000) -or $inventoryLauncher.ExitCode -ne 0){
        throw 'Real foobar Cue inventory launcher failed'
    }
    for($attempt=0;$attempt -lt 55 -and -not (Test-Path -LiteralPath $report);$attempt++){
        Start-Sleep -Milliseconds 120
    }
    if(-not (Test-Path -LiteralPath $report)){
        Write-Host ('MASSTAGGER_INVENTORY_CLI_WINDOWS=' + ([OriginalMasstaggerUiDiscovery]::WindowTitles($FoobarPid) -join ';'))
        throw 'BLOCKED_PRE_ROUTING: external foobar CLI did not run synthetic Cue inventory'
    }
    $inventory=Get-Content -LiteralPath $report -Raw
    if($inventory -notmatch '(?m)^inventory_subsong_count=2\s*$'){
        throw 'Real foobar Cue inventory did not expose two subsongs'
    }
    Remove-Item -LiteralPath $report -Force
    Write-Host 'MASSTAGGER_FOOBAR_CUE_CLI_DISPATCH=PASS'

    # Discover original Masstagger registration through native foobar
    # context tree of one mapped physical handle. Read only, no UI action.
    $treeLauncher=Start-Process -FilePath $exepath -WorkingDirectory $Workdir -PassThru -ArgumentList @('/metadata-isolation-probe-context-tree-physical',('"' + $source + '"'))
    if(-not $treeLauncher.WaitForExit(25000) -or $treeLauncher.ExitCode -ne 0){
        throw 'Real native SDK context-tree CLI launcher failed'
    }
    for($attempt=0;$attempt -lt 55 -and -not (Test-Path -LiteralPath $report);$attempt++){
        Start-Sleep -Milliseconds 120
    }
    if(-not (Test-Path -LiteralPath $report)){
        Write-Host ('MASSTAGGER_CONTEXT_TREE_CLI_WINDOWS=' + ([OriginalMasstaggerUiDiscovery]::WindowTitles($FoobarPid) -join ';'))
        throw 'BLOCKED_PRE_ROUTING: native SDK physical context-tree report absent'
    }
    $tree=Get-Content -LiteralPath $report -Raw
    if($tree -notmatch '(?m)^mapping_target_count=1\s*$' -or
       $tree -notmatch '(?m)^mapping_rejected_count=0\s*$' -or
       $tree -notmatch '(?m)^write_result=context_tree_listed\s*$'){
        throw 'SDK context tree did not map synthetic Cue to one physical file'
    }
    $commandMatches=@($tree -split "[\r\n]+" | Where-Object {
        $_ -match '^context_command=.*[Mm]anage scripts'
    })
    Write-Host ('MASSTAGGER_ORIGINAL_PHYSICAL_SDK_COMMAND_MATCHES=' + $commandMatches.Count)
    Remove-Item -LiteralPath $report -Force
    if($commandMatches.Count -ne 1){
        throw 'BLOCKED_ORIGINAL_MASSTAGGER_COMMAND_NOT_FOUND: expected one native Manage scripts'
    }
    Write-Host 'MASSTAGGER_ORIGINAL_PHYSICAL_SDK_CONTEXT_DISCOVERY=PASS'

    # Reuse the EXISTING Metadata Isolation physical-context routing test:
    # map Cue subsongs to metadb_handle(path,0), load physical metadata,
    # locate the ORIGINAL third-party 'Manage scripts' command by its exact
    # SDK context-menu name, then execute the original Masstagger UI.
    # The test helper does not implement tag writes or imitate Masstagger.
    $report=$source+'.metadata-isolation-probe.txt'
    if(Test-Path -LiteralPath $report){throw 'Refusing stale physical Masstagger probe report'}
    $launcher=Start-Process -FilePath $exepath -WorkingDirectory $Workdir -PassThru -ArgumentList @('/metadata-isolation-probe-run-masstagger-physical',('"' + $source + '"'))
    if(-not $launcher.WaitForExit(25000)){
        throw 'Existing physical-context SDK probe launcher did not terminate'
    }
    if($launcher.ExitCode -ne 0){
        throw 'Existing physical-context SDK probe launcher failed'
    }
    for($attempt=0;$attempt -lt 100;$attempt++){
        $dialog=[OriginalMasstaggerUiDiscovery]::FindMasstagger($FoobarPid)
        if($dialog -ne [IntPtr]::Zero){break}
        Start-Sleep -Milliseconds 150
    }
    if($dialog -eq [IntPtr]::Zero){
        $tops=[OriginalMasstaggerUiDiscovery]::WindowTitles($FoobarPid)
        Write-Host ('MASSTAGGER_HOST_TOP_LEVEL_TITLES=' + ($tops -join ';'))
        if(Test-Path -LiteralPath $report){
            $summary=@(Get-Content -LiteralPath $report | Where-Object {
                $_ -match '^(mapping_(input|target|rejected)_count|physical_info_load_status|write_result|context_command_executed)='
            })
            Write-Host ('MASSTAGGER_PHYSICAL_SDK_REPORT=' + ($summary -join ';'))
        }else{
            Write-Host 'MASSTAGGER_PHYSICAL_SDK_REPORT=NOT_EMITTED'
        }
        foreach($name in @('Playback error','Error')){
            $errorDialog=[OriginalMasstaggerUiDiscovery]::FindExactTitle($FoobarPid,$name)
            if($errorDialog -ne [IntPtr]::Zero){
                $labels=[OriginalMasstaggerUiDiscovery]::Controls($errorDialog,$FoobarPid)
                foreach($entry in $labels){
                    if($entry -match '^(Static|Button):'){
                        Write-Host ('MASSTAGGER_HOST_DIALOG=' + $name + ';' + $entry)
                    }
                }
            }
        }
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
# Original third-party native dialog may be truly modal: its SDK command's
# report can be emitted only AFTER Close. Verify it outside the finally block.
if($dialog -eq [IntPtr]::Zero){throw 'Never obtained original Masstagger native window'}
for($attempt=0;$attempt -lt 50 -and -not (Test-Path -LiteralPath $report);$attempt++){
    Start-Sleep -Milliseconds 150
}
if(-not (Test-Path -LiteralPath $report)){
    throw 'Real Masstagger UI observed but genuine SDK command did not complete'
}
$reportText=Get-Content -LiteralPath $report -Raw
if($reportText -notmatch '(?m)^mapping_target_count=1\s*$' -or
   $reportText -notmatch '(?m)^mapping_rejected_count=0\s*$' -or
   $reportText -notmatch '(?m)^context_command_executed=Manage scripts\s*$'){
    throw 'Original Masstagger physical context SDK command not proven'
}
Write-Host 'MASSTAGGER_REAL_PHYSICAL_CONTEXT_SDK_COMMAND=PASS'
