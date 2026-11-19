"""Exercise the real bridge DLL and pipe without Skyrim; run with DLL path."""
import ctypes as c
from ctypes import wintypes as w
import os
from pathlib import Path
import sys
import tempfile
import threading
import time
from playtest import send, type_text

Tick = c.CFUNCTYPE(None)
Active = c.CFUNCTYPE(c.c_bool)
State = c.CFUNCTYPE(c.c_bool, c.c_bool, w.DWORD, c.c_void_p)
Data = c.CFUNCTYPE(c.c_bool, c.c_bool, w.DWORD, c.c_void_p, c.POINTER(w.DWORD), w.DWORD)
Host = c.CFUNCTYPE(None, c.c_char_p, c.c_void_p, c.c_uint)
class Api(c.Structure):
    _fields_ = [('version', c.c_uint), ('tick', Tick), ('active', Active), ('state', State), ('data', Data), ('capture', c.c_void_p)]
class Event(c.Structure):
    _fields_ = [('offset', w.DWORD), ('value', w.DWORD), ('time', w.DWORD), ('sequence', w.DWORD), ('app', c.c_size_t)]
class Mouse(c.Structure):
    _fields_ = [('x', w.LONG), ('y', w.LONG), ('z', w.LONG), ('buttons', c.c_ubyte * 8)]

@Host
def host(request, output, capacity):
    text = b'{"test_host":true}\0'
    c.memmove(output, text, len(text))

dll = c.CDLL(str(Path(sys.argv[1]).resolve()))
dll.StartBridge.argtypes = [Host, w.LPCWSTR]
dll.StartBridge.restype = c.POINTER(Api)
api = dll.StartBridge(host, tempfile.gettempdir()).contents
assert api.version == 1 and not api.active()
done = threading.Event()
def tick():
    while not done.wait(.005): api.tick()
thread = threading.Thread(target=tick)
thread.start()
pid = os.getpid()
try:
    assert send(pid, 'status')['game']['test_host']
    for invalid in ['input 1 0 0 0 0 17', 'nonsense']:
        try: send(pid, invalid)
        except RuntimeError: pass
        else: raise AssertionError('Command accepted without claim')
    send(pid, 'claim')
    assert api.active()
    for invalid in ['input 5001 0 0 0 0', 'input 1 -2147483648 0 0 0', 'input 1 0 0 0 0 256', 'input 1 0 0 0 0 invalid']:
        try: send(pid, invalid)
        except RuntimeError: pass
        else: raise AssertionError('Invalid input accepted')
    send(pid, 'input 250 12 -8 0 1 17 42')
    keys = (c.c_ubyte * 256)()
    assert api.state(True, 256, keys) and keys[17] == keys[42] == 128
    mouse = Mouse()
    assert api.state(False, c.sizeof(mouse), c.byref(mouse))
    assert (mouse.x, mouse.y, mouse.buttons[0]) == (12, -8, 128)
    assert api.state(False, c.sizeof(mouse), c.byref(mouse)) and mouse.x == mouse.y == 0
    events = (Event * 16)(); count = w.DWORD(16)
    assert api.data(True, c.sizeof(Event), events, c.byref(count), 1) and count.value == 2
    count.value = 16
    assert api.data(True, c.sizeof(Event), events, c.byref(count), 0) and count.value == 2
    time.sleep(.3)
    assert api.state(True, 256, keys) and not any(keys)
    assert api.state(False, c.sizeof(mouse), c.byref(mouse)) and not any(mouse.buttons)
    count.value = 16
    assert api.data(True, c.sizeof(Event), events, c.byref(count), 0) and count.value == 2
    assert all(events[i].value == 0 for i in range(count.value))
    send(pid, 'input 5000 0 0 0 1 17')
    send(pid, 'stop')
    assert api.state(True, 256, keys) and not any(keys)
    count.value = 16
    assert api.data(True, c.sizeof(Event), events, c.byref(count), 0)
    assert any(events[i].offset == 17 and events[i].value == 0 for i in range(count.value))
    time.sleep(.3)
    assert not api.active() and not api.state(True, 256, keys)
    send(pid, 'claim')
    type_text(pid, 'W:')
    count.value = 16
    assert api.data(True, c.sizeof(Event), events, c.byref(count), 0)
    downs = [events[i].offset for i in range(count.value) if events[i].value]
    assert downs == [42, 17, 42, 39], downs  # Shift must precede each character.
    send(pid, 'stop'); time.sleep(.3)
    # Repeated clients must reconnect without resetting input or getting stale replies.
    for _ in range(20): assert not send(pid, 'status')['active']
    send(pid, 'claim')
    time.sleep(30.5)
    assert not send(pid, 'status')['active']
    print('PASS actual DLL/pipe: validation, reconnect, state, buffered peek/consume, mouse, timed release, held-key stop, 30-second lease')
finally:
    done.set(); thread.join()
