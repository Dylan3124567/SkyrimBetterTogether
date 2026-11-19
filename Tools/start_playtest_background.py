"""Launch bridge clients on a non-displayed Windows desktop; never switch desktops."""
import ctypes as c
from ctypes import wintypes as w
import os
from pathlib import Path
import sys
import time

class Startup(c.Structure):
    _fields_ = [('cb',w.DWORD),('reserved',w.LPWSTR),('desktop',w.LPWSTR),('title',w.LPWSTR),
                ('x',w.DWORD),('y',w.DWORD),('width',w.DWORD),('height',w.DWORD),
                ('charsx',w.DWORD),('charsy',w.DWORD),('fill',w.DWORD),('flags',w.DWORD),
                ('show',w.WORD),('reserved2',w.WORD),('reservedptr',c.c_void_p),
                ('stdin',w.HANDLE),('stdout',w.HANDLE),('stderr',w.HANDLE)]
class Process(c.Structure):
    _fields_ = [('process',w.HANDLE),('thread',w.HANDLE),('pid',w.DWORD),('tid',w.DWORD)]
user=c.WinDLL('user32',use_last_error=True)
kernel=c.WinDLL('kernel32',use_last_error=True)
user.CreateDesktopW.argtypes=[w.LPCWSTR,w.LPCWSTR,c.c_void_p,w.DWORD,w.DWORD,c.c_void_p]
user.CreateDesktopW.restype=w.HANDLE
user.CloseDesktop.argtypes=[w.HANDLE]
kernel.CreateProcessW.argtypes=[w.LPCWSTR,w.LPWSTR,c.c_void_p,c.c_void_p,w.BOOL,w.DWORD,c.c_void_p,w.LPCWSTR,c.POINTER(Startup),c.POINTER(Process)]
kernel.CloseHandle.argtypes=[w.HANDLE]
kernel.WaitForSingleObject.argtypes=[w.HANDLE,w.DWORD]
kernel.GetExitCodeProcess.argtypes=[w.HANDLE,c.POINTER(w.DWORD)]
root=Path('A:/SteamClient/steamapps/common/Skyrim Special Edition')
runtime=root/'Data/SkyrimTogetherReborn'
baseline='--baseline' in sys.argv
args=[arg for arg in sys.argv[1:] if arg != '--baseline']
instances=int(args[0]) if args else 2
if baseline: root=Path('A:/SkyrimMultiplayerBuild/vanilla-control-game')
launcher=runtime/'SkyrimTogether.exe'
if instances not in (1,2): raise ValueError('Choose one or two instances')
desktop_name='STR-Background-Playtest'
desktop=user.CreateDesktopW(desktop_name,None,None,0,0x1FF,None)
if not desktop: raise c.WinError(c.get_last_error())
os.environ['STR_PLAYTEST']='1'
os.environ['STR_PLAYTEST_BASELINE']='1' if baseline else '0'
processes=[]
try:
    for _ in range(instances):
        startup=Startup();startup.cb=c.sizeof(startup);startup.desktop='WinSta0\\'+desktop_name
        startup.flags=1;startup.show=0
        process=Process()
        command=c.create_unicode_buffer(f'"{launcher}" --exePath "{root / "SkyrimSE.exe"}"')
        if not kernel.CreateProcessW(None,command,None,None,False,0x08000000,None,str(runtime),c.byref(startup),c.byref(process)):
            raise c.WinError(c.get_last_error())
        print(process.pid,flush=True)
        kernel.CloseHandle(process.thread);processes.append((process.pid,process.process))
    while processes:
        for pid,handle in processes[:]:
            if kernel.WaitForSingleObject(handle,0)==0:
                code=w.DWORD();kernel.GetExitCodeProcess(handle,c.byref(code))
                print(f"Exited {pid}: {code.value:#x}",flush=True)
                kernel.CloseHandle(handle);processes.remove((pid,handle))
        time.sleep(1)
finally:
    user.CloseDesktop(desktop)
