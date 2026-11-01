#requires -Version 7.2
[CmdletBinding()]
param(
  [Parameter(Mandatory)][int]$FoobarPid,
  [Parameter(Mandatory)][IntPtr]$AuditHwnd,
  [Parameter(Mandatory)][IntPtr]$ListHwnd
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if($env:GITHUB_ACTIONS -cne 'true' -or -not $env:RUNNER_TEMP){
  throw 'Native Audit keyboard/MSAA test can run only in ephemeral Windows CI'
}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Threading;
public static class MetadataIsolationAuditKeyboard {
  const uint WM_NEXTDLGCTL=0x0028;
  const uint LVM_GETITEMCOUNT=0x1004;
  const uint LVM_GETITEMSTATE=0x102C;
  const int LVIS_FOCUSED=1, LVIS_SELECTED=2;
  const byte VK_HOME=0x24;
  [StructLayout(LayoutKind.Sequential)]
  public struct RECT {
    public int left, top, right, bottom;
  }
  [StructLayout(LayoutKind.Sequential)]
  public struct GUITHREADINFO {
    public uint cbSize, flags;
    public IntPtr hwndActive, hwndFocus, hwndCapture, hwndMenuOwner, hwndMoveSize, hwndCaret;
    public RECT rcCaret;
  }
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
  [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr hwnd);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
  [DllImport("user32.dll")] static extern bool IsWindowEnabled(IntPtr hwnd);
  [DllImport("user32.dll")] static extern bool GetGUIThreadInfo(uint threadId,ref GUITHREADINFO info);
  [DllImport("user32.dll",EntryPoint="SendMessageTimeoutW",SetLastError=true)]
  static extern IntPtr SendMessageTimeout(IntPtr hwnd,uint msg,IntPtr wp,
    IntPtr lp,uint flags,uint timeout,out IntPtr result);
  [DllImport("user32.dll",EntryPoint="keybd_event")]
  static extern void KeybdEvent(byte key,byte scan,uint flags,IntPtr extraInfo);
  [ComImport, Guid("618736e0-3c3d-11cf-810c-00aa00389b71"),
    InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
  public interface IAccessibleRole {
    [DispId(-5001)] int ChildCount { get; }
    [DispId(-5006)] object this[object child] { get; }
  }
  [ComImport, Guid("618736e0-3c3d-11cf-810c-00aa00389b71"),
    InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
  public interface IAccessibleName {
    [DispId(-5003)] string this[object child] { get; }
  }
  [ComImport, Guid("618736e0-3c3d-11cf-810c-00aa00389b71"),
    InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
  public interface IAccessibleState {
    [DispId(-5007)] object this[object child] { get; }
  }
  [DllImport("oleacc.dll",ExactSpelling=true)]
  static extern int AccessibleObjectFromWindow(IntPtr hwnd,int objId,
    ref Guid iid,[MarshalAs(UnmanagedType.Interface)] out IAccessibleRole value);
  static IntPtr Send(IntPtr hwnd,uint msg,IntPtr wp,IntPtr lp) {
    IntPtr result;
    if(SendMessageTimeout(hwnd,msg,wp,lp,2,3500,out result)==IntPtr.Zero)
      throw new InvalidOperationException("Native Audit control timed out");
    return result;
  }
  static void RequireFoobar(IntPtr hwnd,uint pid) {
    if(hwnd==IntPtr.Zero || !IsWindowVisible(hwnd) || !IsWindowEnabled(hwnd))
      throw new InvalidOperationException("Expected real visible/enabled native Audit HWND");
    uint observed;
    if(GetWindowThreadProcessId(hwnd,out observed)==0 || observed!=pid)
      throw new InvalidOperationException("Native Audit owner PID does not match isolated foobar");
  }
  public static void FocusAndSelectOne(IntPtr audit,IntPtr list,uint expectedPid) {
    RequireFoobar(audit,expectedPid);
    RequireFoobar(list,expectedPid);
    uint listPid,auditPid;
    uint thread=GetWindowThreadProcessId(list,out listPid);
    GetWindowThreadProcessId(audit,out auditPid);
    if(Send(list,LVM_GETITEMCOUNT,IntPtr.Zero,IntPtr.Zero).ToInt64()!=1)
      throw new InvalidOperationException("Only the one known synthetic audit row is allowed");
    for(int i=0;i<12 && GetForegroundWindow()!=audit;i++){
      SetForegroundWindow(audit);
      Thread.Sleep(120);
    }
    if(GetForegroundWindow()!=audit)
      throw new InvalidOperationException("Cannot prove Audit dialog foreground before keyboard injection");
    // Ask the *native* dialog manager to transfer keyboard focus to the
    // ListView. No direct HWND subclass hooks or fake accessibility results.
    Send(audit,WM_NEXTDLGCTL,list,new IntPtr(1));
    GUITHREADINFO info=new GUITHREADINFO();
    info.cbSize=(uint)Marshal.SizeOf(typeof(GUITHREADINFO));
    if(!GetGUIThreadInfo(thread,ref info) || info.hwndFocus!=list)
      throw new InvalidOperationException("Native Audit ListView did not gain keyboard focus");
    KeybdEvent(VK_HOME,0,0,IntPtr.Zero);
    Thread.Sleep(70);
    KeybdEvent(VK_HOME,0,2,IntPtr.Zero);
    Thread.Sleep(170);
    long nativeState=Send(list,LVM_GETITEMSTATE,IntPtr.Zero,new IntPtr(3)).ToInt64();
    if((nativeState&(LVIS_FOCUSED|LVIS_SELECTED))!=(LVIS_FOCUSED|LVIS_SELECTED))
      throw new InvalidOperationException("Real HOME key did not focus/select synthetic Audit row");
    var iid=new Guid("618736e0-3c3d-11cf-810c-00aa00389b71");
    IAccessibleRole acc=null;
    int hr=AccessibleObjectFromWindow(list,-4,ref iid,out acc);
    if(hr!=0 || acc==null)
      throw new InvalidOperationException("Native Audit ListView MSAA object unavailable");
    try {
      int matched=0,selected=0,focused=0;
      IAccessibleName name=(IAccessibleName)acc;
      IAccessibleState state=(IAccessibleState)acc;
      for(int i=1;i<=Math.Min(acc.ChildCount,32);i++){
        object role=acc[i];
        if(role==null || Convert.ToInt32(role)!=34)continue;
        string label=name[i];
        if(label==null || label.IndexOf("Incomplete",StringComparison.OrdinalIgnoreCase)<0)
          throw new InvalidOperationException("Native Audit accessibility row has missing state/name");
        matched++;
        object bits=state[i];
        if(bits==null) throw new InvalidOperationException("MSAA row state unavailable");
        int flags=Convert.ToInt32(bits);
        if((flags&2)!=0)selected++; // STATE_SYSTEM_SELECTED
        if((flags&4)!=0)focused++;  // STATE_SYSTEM_FOCUSED
      }
      if(matched!=1 || selected!=1 || focused!=1)
        throw new InvalidOperationException("MSAA keyboard focus/selection differs from native list: "+
            "row="+matched+" selected="+selected+" focused="+focused);
    } finally { Marshal.ReleaseComObject(acc); }
  }
}
'@
[MetadataIsolationAuditKeyboard]::FocusAndSelectOne($AuditHwnd,$ListHwnd,[uint32]$FoobarPid)
Write-Host 'PRODUCTION_AUDIT_NATIVE_HOME_KEY_ROW_FOCUS_AND_SELECTION=PASS'
Write-Host 'PRODUCTION_AUDIT_MSAA_KEYBOARD_SELECTED_FOCUSED_NAMED_ROW=PASS'
Write-Host 'PRODUCTION_AUDIT_FULL_SCREENREADER_UIA_ROWS=PENDING_PROVIDER_UNEXPOSED'
