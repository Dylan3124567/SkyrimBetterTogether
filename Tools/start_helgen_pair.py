"""Two visible clients, separate monitors, natural pre-character-creation saves."""
import configparser
import ctypes as c
from ctypes import wintypes as w
from concurrent.futures import ThreadPoolExecutor
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import traceback

from playtest import send, press, type_text

GAME = Path('A:/SteamClient/steamapps/common/Skyrim Special Edition')
RUNTIME = GAME / 'Data/SkyrimTogetherReborn'
PROFILE = Path.home() / 'Documents/My Games/Skyrim Special Edition'
PROJECT = Path(__file__).resolve().parents[2]
CHECKPOINTS = PROJECT / 'Helgen checkpoints'
user = c.WinDLL('user32', use_last_error=True)
kernel = c.WinDLL('kernel32', use_last_error=True)
user.SetProcessDPIAware()
MONITOR_CALLBACK = c.WINFUNCTYPE(w.BOOL, w.HANDLE, w.HDC, c.POINTER(w.RECT), w.LPARAM)
WINDOW_CALLBACK = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
user.EnumDisplayMonitors.argtypes = [w.HDC, c.c_void_p, MONITOR_CALLBACK, w.LPARAM]
user.EnumWindows.argtypes = [WINDOW_CALLBACK, w.LPARAM]
user.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
user.IsWindowVisible.argtypes = [w.HWND]
user.GetWindowRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
user.GetClientRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
user.SetWindowPos.argtypes = [w.HWND, w.HWND, c.c_int, c.c_int, c.c_int, c.c_int, w.UINT]
user.SetWindowTextW.argtypes = [w.HWND, w.LPCWSTR]
kernel.CreateMutexW.argtypes = [c.c_void_p, w.BOOL, w.LPCWSTR]
kernel.CreateMutexW.restype = w.HANDLE
kernel.CloseHandle.argtypes = [w.HANDLE]


def monitors():
    result = []
    class Info(c.Structure):
        _fields_ = [('size', w.DWORD), ('monitor', w.RECT), ('work', w.RECT), ('flags', w.DWORD)]
    user.GetMonitorInfoW.argtypes = [w.HANDLE, c.POINTER(Info)]
    @MONITOR_CALLBACK
    def visit(handle, dc, rect, data):
        info = Info(); info.size = c.sizeof(info)
        if user.GetMonitorInfoW(handle, c.byref(info)):
            r = info.work
            result.append((r.left, r.top, r.right, r.bottom))
        return True
    if not user.EnumDisplayMonitors(None, None, visit, 0):
        raise c.WinError(c.get_last_error())
    return sorted(result)


def edit_ini(data, changes):
    ini = configparser.ConfigParser(interpolation=None, strict=False)
    ini.optionxform = str
    ini.read_string(data.decode('utf-8-sig'))
    for section, values in changes.items():
        if not ini.has_section(section): ini.add_section(section)
        for key, value in values.items(): ini.set(section, key, str(value))
    output = io.StringIO(); ini.write(output, space_around_delimiters=False)
    return output.getvalue().encode('utf-8')


def place(pid, monitor, label):
    windows = []
    @WINDOW_CALLBACK
    def visit(hwnd, data):
        owner = w.DWORD(); user.GetWindowThreadProcessId(hwnd, c.byref(owner))
        rect = w.RECT(); user.GetClientRect(hwnd, c.byref(rect))
        if owner.value == pid and user.IsWindowVisible(hwnd) and rect.right > 600:
            windows.append(hwnd)
        return True
    user.EnumWindows(visit, 0)
    if len(windows) != 1: raise RuntimeError(f'Expected one game window for {pid}, found {len(windows)}')
    hwnd = windows[0]
    rect = w.RECT(); user.GetWindowRect(hwnd, c.byref(rect))
    width, height = rect.right - rect.left, rect.bottom - rect.top
    left, top, right, bottom = monitor
    if width > right-left or height > bottom-top: raise RuntimeError('Game window does not fit monitor')
    x, y = left+(right-left-width)//2, top+(bottom-top-height)//2
    if not user.SetWindowPos(hwnd, None, x, y, 0, 0, 0x0015):  # no resize, z-order change or focus
        raise c.WinError(c.get_last_error())
    user.SetWindowTextW(hwnd, f'Skyrim Better Together - {label}')
    user.GetWindowRect(hwnd, c.byref(rect))
    actual = [rect.left, rect.top, rect.right, rect.bottom]
    if not (left <= rect.left < rect.right <= right and top <= rect.top < rect.bottom <= bottom):
        raise RuntimeError(f'Window placement failed: {actual}, monitor {monitor}')
    return actual


def wait_game(process, predicate, seconds=120):
    deadline = time.monotonic() + seconds
    last = None
    while time.monotonic() < deadline:
        if process.poll() is not None: raise RuntimeError(f'Client {process.pid} exited: {process.returncode}')
        try:
            last = send(process.pid, 'status', timeout=1)['game']
            if predicate(last): return last
        except (OSError, TimeoutError, KeyError): pass
        time.sleep(.5)
    raise TimeoutError(f'Client {process.pid} did not reach expected screen: {last}')


def prepare(process, name, monitor):
    pid = process.pid
    wait_game(process, lambda g: 'Main Menu' in g['menus'])
    place(pid, monitor, name)
    send(pid, 'claim')
    try:
        press(pid, ['console']); time.sleep(.5)
        type_text(pid, 'load '+name); press(pid, ['enter']); time.sleep(.5)
        game = wait_game(process, lambda g: g['mq101'] >= 70 and not any(m in g['menus'] for m in ('Main Menu', 'Loading Menu')))
        if 'Console' in game['menus']:
            press(pid, ['console']); time.sleep(.5)
        game = wait_game(process, lambda g: g['mq101'] == 75 and 'RaceSex Menu' in g['menus'], seconds=300)
        return dict(pid=pid, window=place(pid, monitor, name), monitor=monitor, game=game)
    finally:
        send(pid, 'stop')  # return keyboard/mouse to the player


def main():
    screens = monitors()
    if len(screens) < 2: raise RuntimeError('Connect two monitors in Extend mode before starting.')
    for name in ('HelgenLeft', 'HelgenRight'):
        if not (CHECKPOINTS / (name+'.ess')).is_file(): raise FileNotFoundError(CHECKPOINTS / (name+'.ess'))
    if '--check' in sys.argv:
        assert edit_ini(b'[General]\nsLocalSavePath=Saves\\\n', {'General': {'sLocalSavePath': 'Saves\\Test\\'}}).count(b'sLocalSavePath=') == 1
        assert not edit_ini(b'[Display]\niSize W=1280\n', {'Display': {'iSize W': 1024}}).startswith(b'\xef\xbb\xbf')
        print(json.dumps(dict(monitors=screens, checkpoints='present', ini_checks='passed')))
        return
    lock = kernel.CreateMutexW(None, False, 'Local\\SkyrimHelgenPairLauncher')
    if not lock: raise c.WinError(c.get_last_error())
    if c.get_last_error() == 183:
        kernel.CloseHandle(lock)
        raise RuntimeError('The two-client launcher is already running. Close its games before starting again.')
    processes, originals = [], {}
    try:
        running = subprocess.check_output(['tasklist', '/FO', 'CSV', '/NH'], creationflags=subprocess.CREATE_NO_WINDOW).decode(errors='replace').lower()
        if any(f'"{name}"' in running for name in ('skyrimse.exe', 'skyrimtogether.exe')):
            raise RuntimeError('Close existing Skyrim games before starting this two-client test.')
        stamp = time.strftime('%Y%m%d-%H%M%S')
        evidence = PROJECT / 'validation' / ('desktop-launch-'+stamp)
        evidence.mkdir(parents=True)
        saves = PROFILE / 'Saves' / ('HelgenPair-'+stamp); saves.mkdir()
        for source in CHECKPOINTS.iterdir():
            if source.suffix in ('.ess', '.skse'): shutil.copy2(source, saves/source.name)
        originals = {PROFILE/name: (PROFILE/name).read_bytes() for name in ('Skyrim.ini', 'SkyrimPrefs.ini')}
        for path, data in originals.items(): (evidence/path.name).write_bytes(data)
        width = min(1280, *(r-l-32 for l,t,r,b in screens[:2]))
        height = min(720, *(b-t-64 for l,t,r,b in screens[:2]))
        (PROFILE/'Skyrim.ini').write_bytes(edit_ini(originals[PROFILE/'Skyrim.ini'], {'General': {'sLocalSavePath': 'Saves\\'+saves.name+'\\', 'bAlwaysActive': 1}}))
        (PROFILE/'SkyrimPrefs.ini').write_bytes(edit_ini(originals[PROFILE/'SkyrimPrefs.ini'], {'Display': {'bFull Screen': 0, 'bBorderless': 0, 'iSize W': width, 'iSize H': height}}))
        env = dict(os.environ, STR_PLAYTEST='1', STR_PLAYTEST_BASELINE='0')
        for _ in range(2):
            processes.append(subprocess.Popen([str(RUNTIME/'SkyrimTogether.exe'), '--exePath', str(GAME/'SkyrimSE.exe')], cwd=RUNTIME, env=env))
        (evidence/'session.json').write_text(json.dumps(dict(pids=[p.pid for p in processes], saves=str(saves)), indent=2))
        with ThreadPoolExecutor(2) as pool:
            results = list(pool.map(prepare, processes, ['HelgenLeft', 'HelgenRight'], screens[:2]))
        (evidence/'ready.json').write_text(json.dumps(results, indent=2))
    except Exception:
        error = traceback.format_exc()
        (PROJECT/'Helgen launcher error.txt').write_text(error)
        user.MessageBoxW(None, str(error.splitlines()[-1])+'\nDetails: '+str(PROJECT/'Helgen launcher error.txt'), 'Helgen launcher', 0x10)
    finally:
        # Keep the save/profile context until both owned games have really closed.
        for process in processes: process.wait()
        for path, data in originals.items(): path.write_bytes(data)
        if originals:
            (evidence/'restored.json').write_text(json.dumps({'restored': all(p.read_bytes() == d for p,d in originals.items())}))
        kernel.CloseHandle(lock)


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        user.MessageBoxW(None, str(error), 'Helgen launcher', 0x10)
        raise
