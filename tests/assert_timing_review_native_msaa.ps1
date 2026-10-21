#requires -Version 7.2
[CmdletBinding()]
param(
  [Parameter(Mandatory)][int]$FoobarPid,
  [Parameter(Mandatory)][IntPtr]$ReviewHwnd,
  [Parameter(Mandatory)][IntPtr]$CommitHwnd,
  [Parameter(Mandatory)][IntPtr]$DiscardHwnd
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if($env:GITHUB_ACTIONS -cne 'true' -or [string]::IsNullOrWhiteSpace($env:RUNNER_TEMP)) {
  throw 'Live TaskDialog MSAA proof is allowed only inside ephemeral Windows CI'
}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class MetadataIsolationReviewMsaa {
  [ComImport, Guid("618736e0-3c3d-11cf-810c-00aa00389b71"),
   InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
  public interface IAccessibleName {
    [DispId(-5003)] string this[object child] {get;}
  }
  [ComImport, Guid("618736e0-3c3d-11cf-810c-00aa00389b71"),
   InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
  public interface IAccessibleRole {
    [DispId(-5006)] object this[object child] {get;}
  }
  [DllImport("oleacc.dll", ExactSpelling=true)]
  static extern int AccessibleObjectFromWindow(
    IntPtr hwnd, int objectId, ref Guid iid,
    [MarshalAs(UnmanagedType.Interface)] out IAccessibleName accessible);
  [DllImport("user32.dll")]
  static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
  [DllImport("user32.dll")]
  static extern bool IsWindowEnabled(IntPtr hwnd);
  [DllImport("user32.dll")]
  static extern bool IsWindowVisible(IntPtr hwnd);
  [DllImport("user32.dll", EntryPoint="SendMessageTimeoutW")]
  static extern IntPtr SendMessageTimeout(IntPtr hwnd, uint msg, IntPtr w,
    IntPtr l, uint flags, uint timeout, out IntPtr result);
  public static void RequireOwner(IntPtr hwnd,uint expectedPid) {
    if(hwnd==IntPtr.Zero) throw new InvalidOperationException("Native review handle missing");
    uint actualPid;
    if(GetWindowThreadProcessId(hwnd,out actualPid)==0 || actualPid!=expectedPid)
      throw new InvalidOperationException("MSAA target is not the disposable foobar host");
  }
  public static void RequireButton(IntPtr hwnd,string expectedName,uint expectedPid) {
    RequireOwner(hwnd,expectedPid);
    if(!IsWindowVisible(hwnd) || !IsWindowEnabled(hwnd))
      throw new InvalidOperationException("Real review action is hidden or disabled");
    var iid=new Guid("618736e0-3c3d-11cf-810c-00aa00389b71");
    IAccessibleName accessible=null;
    int hr=AccessibleObjectFromWindow(hwnd,-4,ref iid,out accessible);
    if(hr!=0 || accessible==null)
      throw new InvalidOperationException("Native TaskDialog button lacks MSAA client object");
    try {
      string label=accessible[0];
      object role=((IAccessibleRole)accessible)[0];
      if(!String.Equals(label,expectedName,StringComparison.Ordinal) ||
         role==null || Convert.ToInt32(role)!=43) {
        throw new InvalidOperationException("Native action has wrong MSAA accessible name or button role");
      }
    } finally {
      if(accessible!=null) Marshal.ReleaseComObject(accessible);
    }
  }
  public static int DefaultButton(IntPtr hwnd,uint pid) {
    RequireOwner(hwnd,pid);
    IntPtr result;
    if(SendMessageTimeout(hwnd,0x0400,IntPtr.Zero,IntPtr.Zero,2,3000,out result)==IntPtr.Zero)
      throw new InvalidOperationException("Native review did not respond to DM_GETDEFID");
    long value=result.ToInt64();
    if(((value>>16)&0xffff)!=0x534b) return 0; // DC_HASDEFID absent
    return (int)(value&0xffff);
  }
}
'@
$ownerPid=[uint32]$FoobarPid
[MetadataIsolationReviewMsaa]::RequireOwner($ReviewHwnd,$ownerPid)
[MetadataIsolationReviewMsaa]::RequireButton($CommitHwnd,'Commit',$ownerPid)
[MetadataIsolationReviewMsaa]::RequireButton($DiscardHwnd,'Discard',$ownerPid)
Write-Host 'PRODUCTION_VIRTUAL_TIMING_REVIEW_MSAA_BUTTON_NAMES_AND_ROLES=PASS'
$default=[MetadataIsolationReviewMsaa]::DefaultButton($ReviewHwnd,$ownerPid)
if($default -eq 1){
  throw 'Unsafe default: Commit is selected as the native TaskDialog default action'
}
if($default -eq 2){
  Write-Host 'PRODUCTION_VIRTUAL_TIMING_REVIEW_DEFAULT_DISCARD=PASS'
}else{
  # Some hosted TaskDialogs do not expose DM_GETDEFID to out-of-process
  # callers. This never becomes a false PASS or changes the default.
  Write-Host 'PRODUCTION_VIRTUAL_TIMING_REVIEW_DEFAULT_DISCARD=PENDING_NATIVE_NOT_EXPOSED'
}
