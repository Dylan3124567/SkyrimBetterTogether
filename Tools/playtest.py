"""Local Skyrim playtest client. No SendInput, OS cursor moves, or window activation."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
import os
from pathlib import Path
import time
import uuid

kernel = c.WinDLL('kernel32', use_last_error=True)
kernel.CreateFileW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, c.c_void_p, w.DWORD, w.DWORD, w.HANDLE]
kernel.CreateFileW.restype = w.HANDLE
kernel.CloseHandle.argtypes = [w.HANDLE]
kernel.WriteFile.argtypes = [w.HANDLE, c.c_void_p, w.DWORD, c.POINTER(w.DWORD), c.c_void_p]
kernel.ReadFile.argtypes = kernel.WriteFile.argtypes
kernel.PeekNamedPipe.argtypes = [w.HANDLE, c.c_void_p, w.DWORD, c.POINTER(w.DWORD), c.POINTER(w.DWORD), c.POINTER(w.DWORD)]
KEYS = dict(escape=1, enter=28, tab=15, space=57, shift=42, ctrl=29, alt=56, backspace=14,
            up=200, down=208, left=203, right=205, f2=60, console=41)
KEYS.update(zip('qwertyuiopasdfghjklzxcvbnm', [16,17,18,19,20,21,22,23,24,25,30,31,32,33,34,35,36,37,38,44,45,46,47,48,49,50]))

def send(pid, command, timeout=5):
    payload = command.encode('ascii')
    if len(payload) >= 2048 or b'\0' in payload:
        raise ValueError('Command too long or contains NUL')
    end = time.monotonic() + timeout
    while True:
        handle = kernel.CreateFileW(fr'\\.\pipe\SkyrimPlaytest-{pid}', 0xC0000000, 0, None, 3, 0, None)
        if handle != c.c_void_p(-1).value:
            break
        if c.get_last_error() not in (2, 231) or time.monotonic() >= end:
            raise OSError(c.get_last_error(), 'Playtest pipe unavailable; launch with STR_PLAYTEST=1')
        time.sleep(.02)
    try:
        count = w.DWORD()
        if not kernel.WriteFile(handle, payload, len(payload), c.byref(count), None) or count.value != len(payload):
            raise c.WinError(c.get_last_error())
        available = w.DWORD()
        while not available.value:
            if not kernel.PeekNamedPipe(handle, None, 0, None, c.byref(available), None):
                raise c.WinError(c.get_last_error())
            if time.monotonic() >= end:
                raise TimeoutError('Game did not acknowledge command')
            time.sleep(.01)
        buffer = c.create_string_buffer(65536)
        if not kernel.ReadFile(handle, buffer, len(buffer), c.byref(count), None):
            raise c.WinError(c.get_last_error())
        result = json.loads(buffer.raw[:count.value])
        if 'error' in result:
            raise RuntimeError(result['error'])
        return result
    finally:
        kernel.CloseHandle(handle)

def press(pid, keys=(), ms=150, dx=0, dy=0, wheel=0, buttons=0):
    codes = [KEYS[key.lower()] if isinstance(key, str) else key for key in keys]
    return send(pid, ' '.join(map(str, ['input', ms, dx, dy, wheel, buttons, *codes])))

def type_text(pid, text):
    user = c.WinDLL('user32')
    user.VkKeyScanW.argtypes = [w.WCHAR]
    user.VkKeyScanW.restype = c.c_short
    for letter in text:
        mapped = user.VkKeyScanW(letter)
        if mapped == -1 or (mapped >> 8) & ~1:
            raise ValueError(f'Unsupported character on current keyboard layout: {letter!r}')
        scan = user.MapVirtualKeyW(mapped & 255, 0)
        # The game must process Shift before the character's key-down event.
        if mapped & 256:
            press(pid, [42], ms=500)
            time.sleep(.15)
        press(pid, ([42] if mapped & 256 else []) + [scan], ms=100)
        time.sleep(.16)

def console(pid, command, root):
    """Submit a native console batch; success means submitted, not that every command succeeded."""
    if not command.strip() or len(command) > 16384 or '\0' in command:
        raise ValueError('Console batch must contain 1..16384 characters without NUL')
    command.encode('ascii')
    state = send(pid, 'status')
    if any(m in state['game']['menus'] for m in ('Main Menu', 'Loading Menu', 'Console')):
        raise RuntimeError('Load a save and close the existing console before submitting a batch')
    path = root.parent.parent / ('sbt_control_' + uuid.uuid4().hex[:8] + '.txt')
    path.write_text(command + '\n', encoding='ascii')
    send(pid, 'claim')
    try:
        press(pid, ['console']); time.sleep(.3)
        if 'Console' not in send(pid, 'status')['game']['menus']:
            raise RuntimeError('Native console did not open; batch was not submitted')
        type_text(pid, 'bat ' + path.stem)
        send(pid, 'claim')
        press(pid, ['enter']); time.sleep(.5)
        return {'submitted': True, 'batch': str(path), 'console_left_open': True}
    finally:
        if not state['active']:
            send(pid, 'stop')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int)
    parser.add_argument('--root', type=Path, default=Path('A:/SteamClient/steamapps/common/Skyrim Special Edition/Data/SkyrimTogetherReborn'))
    sub = parser.add_subparsers(dest='action', required=True)
    for name in ('list', 'status', 'claim', 'stop', 'screenshot'):
        sub.add_parser(name)
    raw = sub.add_parser('raw'); raw.add_argument('command')
    native = sub.add_parser('console'); native.add_argument('command')
    key = sub.add_parser('key'); key.add_argument('keys', nargs='+'); key.add_argument('--ms', type=int, default=150)
    move = sub.add_parser('move'); move.add_argument('direction', choices=['forward', 'back', 'left', 'right']); move.add_argument('--ms', type=int, default=500)
    for name in ('look', 'mouse'):
        look = sub.add_parser(name); look.add_argument('dx', type=int); look.add_argument('dy', type=int)
    cursor = sub.add_parser('cursor'); cursor.add_argument('x', type=int); cursor.add_argument('y', type=int)
    click = sub.add_parser('click'); click.add_argument('button', choices=['left','right','middle'], default='left', nargs='?')
    text = sub.add_parser('type'); text.add_argument('text')
    args = parser.parse_args()
    if args.action == 'list':
        print(json.dumps([int(name.split('-')[-1]) for name in os.listdir('\\\\.\\pipe\\') if name.startswith('SkyrimPlaytest-')]))
        return
    if not args.pid:
        parser.error('--pid is required')
    action = args.action
    if action in ('status', 'claim', 'stop'):
        result = send(args.pid, action)
    elif action == 'raw': result = send(args.pid, args.command)
    elif action == 'console': result = console(args.pid, args.command, args.root)
    elif action == 'key': result = press(args.pid, args.keys, args.ms)
    elif action == 'move': result = press(args.pid, [dict(forward='w', back='s', left='a', right='d')[args.direction]], args.ms)
    elif action in ('look', 'mouse'): result = press(args.pid, dx=args.dx, dy=args.dy)
    elif action == 'cursor': result = send(args.pid, f'cursor {args.x} {args.y}')
    elif action == 'click': result = press(args.pid, buttons=1 << ['left','right','middle'].index(args.button))
    elif action == 'type': type_text(args.pid, args.text); result = {'ok': True}
    elif action == 'screenshot':
        before = send(args.pid, 'status')['frames']
        send(args.pid, 'capture')
        end = time.monotonic() + 10
        while True:
            state = send(args.pid, 'status')
            if state['frames'] > before: break
            if state['capture_error']: raise RuntimeError(f'Capture failed: {state["capture_error"]}')
            if time.monotonic() >= end: raise TimeoutError('Game is not rendering')
            time.sleep(.1)
        result = {'path': str(args.root / 'playtest' / str(args.pid) / 'frame.bmp'), 'frame': state['frames']}
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
