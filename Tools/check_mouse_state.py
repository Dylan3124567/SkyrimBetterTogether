"""Compile the actual unbuffered mouse handler against a fake DirectInput device (VS shell)."""
from pathlib import Path
import subprocess
import sys
import tempfile

source = (Path(__file__).resolve().parents[1] / 'Libraries/TiltedHooks/Code/hooks/src/DInputHook.cpp').read_text()
body = source.split('HRESULT _stdcall StubIDirectInputDevice8A::GetDeviceState', 1)[1].split('HRESULT _stdcall StubIDirectInputDevice8A::GetDeviceData', 1)[0]
code = r'''
#include <windows.h>
#include <dinput.h>
#undef IDirectInputDevice8_GetDeviceState
#include <cassert>
#include <cstring>
struct DInputHook {
    bool enabled = false;
    bool (*VirtualState)(bool, unsigned long, void*) = nullptr;
    bool (*OnLeftMousePress)() = nullptr;
    bool IsEnabled() { return enabled; }
    static DInputHook& Get() { static DInputHook input; return input; }
};
bool down = false;
HRESULT deviceResult = DI_OK;
int clicks = 0;
HRESULT IDirectInputDevice8_GetDeviceState(void*, DWORD size, void* data) {
    if (FAILED(deviceResult)) return deviceResult;
    std::memset(data, 0, size);
    if (size >= sizeof(DIMOUSESTATE)) static_cast<DIMOUSESTATE*>(data)->rgbButtons[0] = down ? 128 : 0;
    return deviceResult;
}
struct StubIDirectInputDevice8A {
    void* m_pDevice = nullptr;
    bool m_keyboard = false, m_mouse = true, m_leftMouseDown = false;
    HRESULT _stdcall GetDeviceState(DWORD, void*);
};
HRESULT _stdcall StubIDirectInputDevice8A::GetDeviceState
''' + body + r'''
int main() {
    StubIDirectInputDevice8A device; DIMOUSESTATE2 state{};
    DInputHook::Get().OnLeftMousePress = [] { ++clicks; return true; };
    assert(device.GetDeviceState(sizeof(state), &state) == DI_OK && clicks == 0);
    down = true;
    assert(device.GetDeviceState(sizeof(state), &state) == DI_OK && clicks == 1 && state.rgbButtons[0] == 0);
    device.GetDeviceState(sizeof(state), &state); assert(clicks == 1);
    down = false; device.GetDeviceState(sizeof(state), &state);
    down = true; device.GetDeviceState(sizeof(state), &state); assert(clicks == 2);
    device.m_leftMouseDown = false; deviceResult = DIERR_INPUTLOST;
    assert(FAILED(device.GetDeviceState(sizeof(state), &state)) && clicks == 2);
    deviceResult = DI_OK; device.m_mouse = false; device.m_keyboard = true;
    device.GetDeviceState(sizeof(state), &state); assert(clicks == 2);
    device.m_mouse = true; DInputHook::Get().enabled = true;
    device.GetDeviceState(sizeof(state), &state); assert(clicks == 2 && state.rgbButtons[0] == 0);
}
'''
with tempfile.TemporaryDirectory(prefix='mouse-state-', dir=sys.argv[1] if len(sys.argv) > 1 else None) as directory:
    root = Path(directory); (root/'check.cpp').write_text(code)
    subprocess.run(['cl', '/nologo', '/EHsc', '/std:c++20', 'check.cpp', '/Fe:check.exe'], cwd=root, check=True)
    subprocess.run([str(root/'check.exe')], cwd=root, check=True)
print('PASS actual mouse handler: click edge, held click, release/reclick, failed read, keyboard isolation and overlay capture')
