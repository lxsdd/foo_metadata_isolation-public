#requires -Version 7.2
# CI-only original third-party native Masstagger UI evidence, not a tag-writing substitute.
[CmdletBinding()]
param(
  [Parameter(Mandatory)][int]$FoobarPid,
  [Parameter(Mandatory)][string]$Executable,
  [Parameter(Mandatory)][string]$FixturePath,
  [switch]$SyntheticPhysicalCommit,
  [string]$BeforeFixturePath
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
if($SyntheticPhysicalCommit){
  if([string]::IsNullOrWhiteSpace($BeforeFixturePath)){throw 'Missing synthetic baseline for physical UI test'}
  $baselinePath=[IO.Path]::GetFullPath($BeforeFixturePath)
  if(-not $baselinePath.StartsWith($root,[StringComparison]::OrdinalIgnoreCase) -or
     [IO.Path]::GetDirectoryName($baselinePath) -cne [IO.Path]::GetDirectoryName($source) -or
     [IO.Path]::GetFileName($baselinePath) -cne 'track-01.before' -or
     -not (Test-Path -LiteralPath $baselinePath) -or
     (Get-FileHash -LiteralPath $baselinePath -Algorithm SHA256).Hash -cne $before){
    throw 'Refusing unverified synthetic-only Masstagger physical UI baseline'
  }
}
$report=$source+'.metadata-isolation-probe.txt'
if(Test-Path -LiteralPath $report){
  # The fixture was verified absent before foobar launch; on a fast host,
  # its CI-only initquit handler may finish before this watcher starts.
  Write-Host 'MASSTAGGER_NATIVE_BOOTSTRAP_STARTUP_REPORT_ALREADY_EMITTED=TRUE'
}

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
  public static IntPtr ExactTitle(int pid,string name){return Find(pid,name,false);}
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

  [DllImport("user32.dll",EntryPoint="SendMessageW")]
  static extern IntPtr SendMessage(IntPtr hwnd,uint message,IntPtr wp,IntPtr lp);
  [DllImport("user32.dll")]
  static extern int GetMenuItemCount(IntPtr menu);
  [DllImport("user32.dll",CharSet=CharSet.Unicode,EntryPoint="GetMenuStringW")]
  static extern int GetMenuString(IntPtr menu,uint index,StringBuilder text,int capacity,uint flags);
  public static bool ClickExactAdd(IntPtr root,int pid){
    uint owner;GetWindowThreadProcessId(root,out owner);
    if(root==IntPtr.Zero||owner!=(uint)pid||!IsWindowVisible(root))
      throw new InvalidOperationException("Unverified original Masstagger owner");
    IntPtr add=IntPtr.Zero;int count=0;
    EnumChildWindows(root,(h,x)=>{
      uint p;GetWindowThreadProcessId(h,out p);
      if(p==(uint)pid&&Class(h)=="Button"&&Caption(h)=="&Add"){
        add=h;count++;
      }
      return true;},IntPtr.Zero);
    if(count!=1)return false;
    if(!PostMessage(add,0x00F5,IntPtr.Zero,IntPtr.Zero))
      throw new InvalidOperationException("Original Add button did not receive click");
    return true;
  }
  public static string[] BoundedOriginalPopupItems(int pid){
    var list=new List<string>();
    EnumWindows((h,x)=>{
      uint owner;GetWindowThreadProcessId(h,out owner);
      if(owner!=(uint)pid||!IsWindowVisible(h)||Class(h)!="#32768")return true;
      IntPtr menu=SendMessage(h,0x01E1,IntPtr.Zero,IntPtr.Zero);
      if(menu==IntPtr.Zero)return true;
      int count=GetMenuItemCount(menu);
      if(count<0)return true;
      for(uint i=0;i<(uint)Math.Min(count,40);i++){
        var raw=new StringBuilder(200);
        if(GetMenuString(menu,i,raw,raw.Capacity,0x0400)<=0)continue;
        string value=raw.ToString().Split('\t')[0].Trim();
        if(value.Length==0)continue;
        bool safe=value.Length<=80;
        foreach(char ch in value){
          if(!(Char.IsLetterOrDigit(ch)||" &+.,()_-'".IndexOf(ch)>=0)){
            safe=false;break;
          }
        }
        list.Add(safe?value:"<redacted>");
      }
      return true;},IntPtr.Zero);
    return list.ToArray();
  }
  [StructLayout(LayoutKind.Sequential)]
  struct MenuRect {public int Left,Top,Right,Bottom;}
  [StructLayout(LayoutKind.Sequential)]
  struct MenuPoint {public int X,Y;}
  [DllImport("user32.dll")]
  static extern bool GetMenuItemRect(IntPtr hwnd,IntPtr menu,uint item,out MenuRect rect);
  [DllImport("user32.dll")]
  static extern IntPtr WindowFromPoint(MenuPoint point);
  [DllImport("user32.dll")]
  static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")]
  static extern bool SetCursorPos(int x,int y);
  [DllImport("user32.dll")]
  static extern bool GetCursorPos(out MenuPoint point);
  [DllImport("user32.dll")]
  static extern void mouse_event(uint flags,uint dx,uint dy,uint data,UIntPtr extra);
  public static bool DispatchOriginalSetValueAction(IntPtr root,int pid){
    uint owner;GetWindowThreadProcessId(root,out owner);
    if(root==IntPtr.Zero||owner!=(uint)pid||!IsWindowVisible(root))
      throw new InvalidOperationException("Unverified original Masstagger UI for action selection");
    IntPtr selectedMenu=IntPtr.Zero;
    MenuPoint target=new MenuPoint();
    int matches=0;
    EnumWindows((h,x)=>{
      uint p;GetWindowThreadProcessId(h,out p);
      if(p!=(uint)pid||!IsWindowVisible(h)||Class(h)!="#32768")return true;
      IntPtr menu=SendMessage(h,0x01E1,IntPtr.Zero,IntPtr.Zero);
      if(menu==IntPtr.Zero)return true;
      int count=GetMenuItemCount(menu);
      for(uint i=0;i<(uint)Math.Min(count,40);i++){
        var label=new StringBuilder(200);
        if(GetMenuString(menu,i,label,label.Capacity,0x0400)<=0)continue;
        if(!String.Equals(label.ToString().Split('\t')[0].Trim(),
                          "Set value...",StringComparison.Ordinal))continue;
        MenuRect rect;
        if(!GetMenuItemRect(IntPtr.Zero,menu,i,out rect) ||
           rect.Right<=rect.Left||rect.Bottom<=rect.Top)continue;
        selectedMenu=h;
        target=new MenuPoint{X=rect.Left+(rect.Right-rect.Left)/2,
                             Y=rect.Top+(rect.Bottom-rect.Top)/2};
        matches++;
      }
      return true;},IntPtr.Zero);
    if(matches!=1)return false;
    // Never inject input into a different foreground process or beneath
    // a foreign overlay; all windows must belong to verified disposable foobar.
    IntPtr foreground=GetForegroundWindow();
    if(foreground==IntPtr.Zero)return false;
    uint fgPid;GetWindowThreadProcessId(foreground,out fgPid);
    if(fgPid!=(uint)pid)return false;
    IntPtr hit=WindowFromPoint(target);
    if(hit==IntPtr.Zero)return false;
    uint hitPid;GetWindowThreadProcessId(hit,out hitPid);
    if(hitPid!=(uint)pid)return false;
    MenuPoint previous;
    if(!GetCursorPos(out previous)||!SetCursorPos(target.X,target.Y))return false;
    try {
      foreground=GetForegroundWindow();
      GetWindowThreadProcessId(foreground,out fgPid);
      if(fgPid!=(uint)pid)return false;
      // Exactly one real click into the original 'Set value...' menu item,
      // in a disposable runner session. Never click original Run or any file.
      mouse_event(0x0002,0,0,0,UIntPtr.Zero);
      mouse_event(0x0004,0,0,0,UIntPtr.Zero);
    }finally {
      SetCursorPos(previous.X,previous.Y);
    }
    return true;
  }
  [DllImport("user32.dll")] static extern IntPtr GetParent(IntPtr hwnd);
  [DllImport("user32.dll",CharSet=CharSet.Unicode,EntryPoint="SendMessageW")]
  static extern IntPtr SendMessageSetText(IntPtr hwnd,uint msg,IntPtr wp,string text);
  [DllImport("user32.dll",CharSet=CharSet.Unicode,EntryPoint="SendMessageW")]
  static extern IntPtr SendMessageGetText(IntPtr hwnd,uint msg,IntPtr wp,StringBuilder text);
  static bool ExactEditText(IntPtr edit,string expected){
    var text=new StringBuilder(160);
    SendMessageGetText(edit,0x000D,new IntPtr(text.Capacity),text);
    return String.Equals(text.ToString(),expected,StringComparison.Ordinal);
  }
  public static string ConfigureSyntheticAndChoose(IntPtr root,int pid,bool commit){
    uint p;GetWindowThreadProcessId(root,out p);
    if(root==IntPtr.Zero||p!=(uint)pid||!IsWindowVisible(root)||
       Caption(root)!="Set Value")
      throw new InvalidOperationException("Synthetic Set Value requires exact genuine owned modal");
    var combos=new List<IntPtr>();var edits=new List<IntPtr>();
    var cancel=new List<IntPtr>();var ok=new List<IntPtr>();
    EnumChildWindows(root,(h,x)=>{
      uint owner;GetWindowThreadProcessId(h,out owner);
      if(owner!=(uint)pid)return true;
      string c=Class(h);
      if(c=="ComboBox")combos.Add(h);
      if(c=="Edit")edits.Add(h);
      if(c=="Button"&&Caption(h)=="Cancel")cancel.Add(h);
      if(c=="Button"&&Caption(h)=="OK")ok.Add(h);
      return true;},IntPtr.Zero);
    if(combos.Count!=1||edits.Count!=2||cancel.Count!=1||ok.Count!=1)
      return "PENDING_UNEXPECTED_CONTROL_LAYOUT";
    IntPtr field=IntPtr.Zero,value=IntPtr.Zero;
    foreach(var edit in edits){
      if(GetParent(edit)==combos[0])field=edit;
      else if(GetParent(edit)==root)value=edit;
    }
    if(field==IntPtr.Zero||value==IntPtr.Zero||field==value)
      return "PENDING_FIELD_VALUE_PARENT_NOT_VERIFIED";
    // CI-only synthetic UI input, never a private preset, filename or tag.
    const string name="COMMENT";
    string data=commit?"MI_SYNTHETIC_MASSTAGGER_REAL_UI_193":"MI_SYNTHETIC_MASSTAGGER_CANCEL_193";
    if(SendMessageSetText(field,0x000C,IntPtr.Zero,name)==IntPtr.Zero||
       SendMessageSetText(value,0x000C,IntPtr.Zero,data)==IntPtr.Zero||
       !ExactEditText(field,name)||!ExactEditText(value,data))
      return "PENDING_ORIGINAL_MODAL_TEXT_NOT_CONFIRMED";
    IntPtr action=commit?ok[0]:cancel[0];
    if(!PostMessage(action,0x00F5,IntPtr.Zero,IntPtr.Zero))
      throw new InvalidOperationException("Original synthetic Set Value action click failed");
    return commit?"PASS_SYNTHETIC_FIELD_VALUE_ENTERED_OK_CLICKED":
                  "PASS_SYNTHETIC_FIELD_VALUE_ENTERED_CANCEL_CLICKED";
  }
  public static bool ClickOriginalRun(IntPtr root,int pid){
    uint p;GetWindowThreadProcessId(root,out p);
    if(root==IntPtr.Zero||p!=(uint)pid||!IsWindowVisible(root)||
       Caption(root).IndexOf("Masstagger",StringComparison.OrdinalIgnoreCase)<0)
      throw new InvalidOperationException("Unverified original Masstagger Run target");
    IntPtr run=IntPtr.Zero;int count=0;
    EnumChildWindows(root,(h,x)=>{
      uint owner;GetWindowThreadProcessId(h,out owner);
      if(owner==(uint)pid&&Class(h)=="Button"&&Caption(h)=="Run"){
        run=h;count++;
      }
      return true;},IntPtr.Zero);
    if(count!=1)return false;
    if(!PostMessage(run,0x00F5,IntPtr.Zero,IntPtr.Zero))
      throw new InvalidOperationException("Cannot click verified original Masstagger Run");
    return true;
  }
  public static void CancelOriginalPopup(IntPtr root,int pid){
    uint owner;GetWindowThreadProcessId(root,out owner);
    if(root==IntPtr.Zero||owner!=(uint)pid)
      throw new InvalidOperationException("Original popup cancel owner mismatch");
    PostMessage(root,0x001F,IntPtr.Zero,IntPtr.Zero);
    EnumWindows((h,x)=>{
      uint p;GetWindowThreadProcessId(h,out p);
      if(p==(uint)pid&&Class(h)=="#32768")
        PostMessage(h,0x001F,IntPtr.Zero,IntPtr.Zero);
      return true;},IntPtr.Zero);
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

    # Exploratory, disposable UI-only click: Add never executes Run.
    # Read only original action-type labels; do not select an action, save a
    # preset or touch the synthetic MP3. Menu is cancelled before closing.
    if([NativeOriginalMasstaggerBootstrapUI]::ClickExactAdd($dialog,$FoobarPid)){
      $popup=@()
      for($menuPoll=0;$menuPoll -lt 25;$menuPoll++){
        $popup=@([NativeOriginalMasstaggerBootstrapUI]::BoundedOriginalPopupItems($FoobarPid))
        if($popup.Count -gt 0){break}
        Start-Sleep -Milliseconds 100
      }
      if($popup.Count -gt 0){
        Write-Host ('MASSTAGGER_ORIGINAL_ADD_ACTION_MENU_COUNT=' + $popup.Count)
        foreach($item in $popup){Write-Host ('MASSTAGGER_ORIGINAL_ADD_ACTION_MENU_ITEM=' + $item)}
        Write-Host 'MASSTAGGER_ORIGINAL_ADD_ACTION_MENU=OBSERVED_READ_ONLY'
        # Baseline must precede dispatch; an immediate modal must not be
        # mistaken for an already-existing window.
        $baseline=@([NativeOriginalMasstaggerBootstrapUI]::Titles($FoobarPid))

        if(@($popup | Where-Object {$_ -ceq 'Set value...'}).Count -eq 1 -and
           [NativeOriginalMasstaggerBootstrapUI]::DispatchOriginalSetValueAction($dialog,$FoobarPid)){
          # Verify the result of the exact original Set value menu click.
          # Observe only window titles and redacted child control labels.
          Start-Sleep -Milliseconds 650
          $titles=@([NativeOriginalMasstaggerBootstrapUI]::Titles($FoobarPid))
          Write-Host ('MASSTAGGER_ORIGINAL_SET_VALUE_CONFIGURATION_TOP_LEVEL=' + ($titles -join ';'))
          $updatedControls=@([NativeOriginalMasstaggerBootstrapUI]::Controls($dialog,$FoobarPid))
          Write-Host ('MASSTAGGER_ORIGINAL_SET_VALUE_AFTER_DISPATCH_CONTROL_COUNT=' + $updatedControls.Count)
          foreach($entry in $updatedControls){Write-Host ('MASSTAGGER_ORIGINAL_SET_VALUE_AFTER_DISPATCH_CONTROL=' + $entry)}
          $configuration=[NativeOriginalMasstaggerBootstrapUI]::ExactTitle($FoobarPid,'Set Value')
          if($configuration -ne [IntPtr]::Zero){
            # Genuine original third-party modal. Inspect only class names and
            # bounded safe static/button captions; never inspect edit contents.
            $configurationControls=@([NativeOriginalMasstaggerBootstrapUI]::Controls($configuration,$FoobarPid))
            Write-Host ('MASSTAGGER_ORIGINAL_SET_VALUE_MODAL_CONTROL_COUNT=' + $configurationControls.Count)
            foreach($safeControl in $configurationControls){
              Write-Host ('MASSTAGGER_ORIGINAL_SET_VALUE_MODAL_CONTROL=' + $safeControl)
            }
            Write-Host 'MASSTAGGER_ORIGINAL_SET_VALUE_MODAL=PASS_OPENED_READ_ONLY'
            $dryRun=[NativeOriginalMasstaggerBootstrapUI]::ConfigureSyntheticAndChoose($configuration,$FoobarPid,$SyntheticPhysicalCommit.IsPresent)
            Write-Host ('MASSTAGGER_ORIGINAL_SET_VALUE_SYNTHETIC_DRY_RUN=' + $dryRun)
            $expectedUIResult=if($SyntheticPhysicalCommit){'PASS_SYNTHETIC_FIELD_VALUE_ENTERED_OK_CLICKED'}else{'PASS_SYNTHETIC_FIELD_VALUE_ENTERED_CANCEL_CLICKED'}
            if($dryRun -cne $expectedUIResult){
              [NativeOriginalMasstaggerBootstrapUI]::Close($configuration,$FoobarPid)
            }
            for($closedPoll=0;$closedPoll -lt 25;$closedPoll++){
              if([NativeOriginalMasstaggerBootstrapUI]::ExactTitle($FoobarPid,'Set Value') -eq [IntPtr]::Zero){break}
              Start-Sleep -Milliseconds 100
            }
            if([NativeOriginalMasstaggerBootstrapUI]::ExactTitle($FoobarPid,'Set Value') -ne [IntPtr]::Zero){
              throw 'Genuine disposable Set Value modal did not close without accepting edits'
            }
            if($SyntheticPhysicalCommit){
              Write-Host 'MASSTAGGER_ORIGINAL_SET_VALUE_MODAL_SUBMITTED=PASS_SYNTHETIC_ONLY'
              if($dryRun -ceq 'PASS_SYNTHETIC_FIELD_VALUE_ENTERED_OK_CLICKED'){
                if([NativeOriginalMasstaggerBootstrapUI]::ClickOriginalRun($dialog,$FoobarPid)){
                  Write-Host 'MASSTAGGER_ORIGINAL_GENUINE_RUN_CLICK=PASS_SENT'
                  # Keep genuine original Manage scripts UI alive while the
                  # third-party action may be applying the synthetic tag.
                  for($runPoll=0;$runPoll -lt 45;$runPoll++){
                    Start-Sleep -Milliseconds 200
                    try{$observedHash=(Get-FileHash -LiteralPath $source -Algorithm SHA256 -ErrorAction Stop).Hash}catch{continue}
                    if($observedHash -cne $before){break}
                  }
                  Write-Host ('MASSTAGGER_ORIGINAL_GENUINE_RUN_UI_WINDOWS=' +
                    ([NativeOriginalMasstaggerBootstrapUI]::Titles($FoobarPid) -join ';'))
                }else{
                  Write-Host 'MASSTAGGER_ORIGINAL_GENUINE_RUN_CLICK=PENDING_NO_UNIQUE_RUN'
                }
              }
            }else{
              Write-Host 'MASSTAGGER_ORIGINAL_SET_VALUE_MODAL_CANCEL=PASS'
            }
          }else{
            Write-Host 'MASSTAGGER_ORIGINAL_SET_VALUE_MODAL=PENDING_NOT_OBSERVED'
          }
          Write-Host ('MASSTAGGER_ORIGINAL_SET_VALUE_ACTION=' + $(if($SyntheticPhysicalCommit){'REAL_ORIGINAL_UI_RUN_SENT_POSTIMAGE_ORACLE_REQUIRED'}else{'CANCEL_ONLY'}))
          # No field text is entered here; the native action's configuration
          # modal is deliberately closed without accepting or running it.
          [void][NativeOriginalMasstaggerBootstrapUI]::CloseKnown($FoobarPid,'Set value')
          [void][NativeOriginalMasstaggerBootstrapUI]::CloseKnown($FoobarPid,'Set value...')
        }else{
          Write-Host 'MASSTAGGER_ORIGINAL_SET_VALUE_ACTION=PENDING_NO_VERIFIED_MENU_COMMAND'
        }

      }else{
        Write-Host 'MASSTAGGER_ORIGINAL_ADD_ACTION_MENU=PENDING_NO_POPUP_ITEMS'
      }
      [NativeOriginalMasstaggerBootstrapUI]::CancelOriginalPopup($dialog,$FoobarPid)
    }else{
      Write-Host 'MASSTAGGER_ORIGINAL_ADD_ACTION_MENU=PENDING_NO_UNIQUE_ADD_BUTTON'
    }

  }
}finally{
  if($dialog -ne [IntPtr]::Zero){
    [NativeOriginalMasstaggerBootstrapUI]::Close($dialog,$FoobarPid)
  }
  if($SyntheticPhysicalCommit){
    $changed=$false
    for($writePoll=0;$writePoll -lt 60;$writePoll++){
      Start-Sleep -Milliseconds 200
      try{$post=(Get-FileHash -LiteralPath $source -Algorithm SHA256 -ErrorAction Stop).Hash}catch{continue}
      if($post -cne $before){$changed=$true;break}
    }
    if($changed){
      Start-Sleep -Milliseconds 750
      & python tests/assert_masstagger_embedded_cue_physical.py $baselinePath $source 'MI_SYNTHETIC_MASSTAGGER_REAL_UI_193'
      if($LASTEXITCODE -ne 0){
        throw 'Original Masstagger synthetic UI commit failed independent ID3/CUE/PRIV/MPEG oracle'
      }
      Write-Host 'MASSTAGGER_ORIGINAL_EMBEDDED_CUE_UI_PHYSICAL_NONNOOP=PASS_INDEPENDENT_ORACLE'
    }else{
      Write-Host 'MASSTAGGER_ORIGINAL_EMBEDDED_CUE_UI_PHYSICAL_NONNOOP=PENDING_NO_WRITE_OBSERVED'
    }
    # The preexisting plain/Cue CLI regression stages require the exact
    # original disposable Cue. Restore only the SHA-verified runner fixture.
    Copy-Item -LiteralPath $baselinePath -Destination $source -Force
    if((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -cne $before){
      throw 'Synthetic Cue fixture restore after genuine UI test failed'
    }
  }else{
    if((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -cne $before){
      throw 'Native original Masstagger UI read-only discovery modified embedded Cue MP3'
    }
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

# This is a narrow real third-party physical-write gate, not overall release
# acceptance. It is PASS only after the independent ID3/CUE/PRIV/MPEG oracle.
if($SyntheticPhysicalCommit -and $changed){
  Write-Host 'MASSTAGGER_NATIVE_BOOTSTRAP_POSITIVE_PHYSICAL_COMMIT=PASS_INDEPENDENT_ORACLE'
}else{
  Write-Host 'MASSTAGGER_NATIVE_BOOTSTRAP_POSITIVE_PHYSICAL_COMMIT=PENDING_EXPLICIT_UI_ACTION'
}

# The next, independently existing CLI/context-tree probe reuses the SAME
# synthetic per-file report name. Clear only the verified report of this
# disposable startup probe, never a user file or an unrecognized report.
if(Test-Path -LiteralPath $report){
  if($result -notmatch '(?m)^native_original_masstagger_bootstrap=true\s*$' -or
     $result -notmatch '(?m)^native_original_context_executed=Manage scripts\s*$' -or
     $result -notmatch '(?m)^write_result=original_ui_returned\s*$'){
    throw 'Refusing to remove unqualified native bootstrap report'
  }
  Remove-Item -LiteralPath $report -Force
  Write-Host 'MASSTAGGER_NATIVE_BOOTSTRAP_VERIFIED_REPORT_CLEANED=PASS'
}
