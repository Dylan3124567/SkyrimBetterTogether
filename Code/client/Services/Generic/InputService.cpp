#include <TiltedOnlinePCH.h>

#include <Services/InputService.h>
#include <Services/OverlayService.h>

#include <OverlayApp.hpp>

#include <DInputHook.hpp>
#include <WindowsHook.hpp>

#include <include/internal/cef_types.h>
#include <Services/ImguiService.h>
#include <Services/DiscordService.h>
#include <World.h>
#include <Playtest.h>
#include <Systems/UiRules.h>

#include <thread>

#include "Games/Skyrim/Interface/MenuControls.h"
#include <Interface/MenuCursor.h>
#include <BSGraphics/BSGraphicsRenderer.h>

static OverlayService* s_pOverlay = nullptr;
static UINT s_currentACP = CP_ACP;

void ForceKillAllInput()
{
    MenuControls::GetInstance()->SetToggle(false);
}

uint32_t GetCefModifiers(uint16_t aVirtualKey)
{
    uint32_t modifiers = EVENTFLAG_NONE;

    if (GetKeyState(VK_MENU) & 0x8000)
    {
        modifiers |= EVENTFLAG_ALT_DOWN;
    }

    if (GetKeyState(VK_CONTROL) & 0x8000)
    {
        modifiers |= EVENTFLAG_CONTROL_DOWN;
    }

    if (GetKeyState(VK_SHIFT) & 0x8000)
    {
        modifiers |= EVENTFLAG_SHIFT_DOWN;
    }

    if (GetKeyState(VK_LBUTTON) & 0x8000)
    {
        modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
    }

    if (GetKeyState(VK_RBUTTON) & 0x8000)
    {
        modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
    }

    if (GetKeyState(VK_MBUTTON) & 0x8000)
    {
        modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
    }

    if (GetKeyState(VK_CAPITAL) & 1)
    {
        modifiers |= EVENTFLAG_CAPS_LOCK_ON;
    }

    if (GetKeyState(VK_NUMLOCK) & 1)
    {
        modifiers |= EVENTFLAG_NUM_LOCK_ON;
    }

    if (aVirtualKey)
    {
        if (aVirtualKey == VK_RCONTROL || aVirtualKey == VK_RMENU || aVirtualKey == VK_RSHIFT)
        {
            modifiers |= EVENTFLAG_IS_RIGHT;
        }
        else if (aVirtualKey == VK_LCONTROL || aVirtualKey == VK_LMENU || aVirtualKey == VK_LSHIFT)
        {
            modifiers |= EVENTFLAG_IS_LEFT;
        }
        else if (aVirtualKey >= VK_NUMPAD0 && aVirtualKey <= VK_DIVIDE)
        {
            modifiers |= EVENTFLAG_IS_KEY_PAD;
        }
    }

    return modifiers;
}

// remember to update this when updating toggle keys
bool IsToggleKey(int aKey) noexcept
{
    return aKey == VK_RCONTROL || aKey == VK_F2;
}

bool IsDisableKey(int aKey) noexcept
{
    return aKey == VK_ESCAPE;
}

void SetUIActive(OverlayService& aOverlay, bool aActive)
{
    aOverlay.SetActive(aActive);

    // Ensures the game is actually loaded, in case the initial event was sent too early
    aOverlay.SetVersion(BUILD_COMMIT);
    aOverlay.GetOverlayApp()->ExecuteAsync("enterGame");

    // This is to disable the Windows cursor
    while (ShowCursor(FALSE) >= 0)
        ;
}

void ProcessKeyboard(uint16_t aKey, uint16_t aScanCode, cef_key_event_type_t aType, bool aE0, bool aE1)
{
    if (aType != KEYEVENT_CHAR)
    {
        if (!aKey || aKey == 255)
        {
            return;
        }

        if (aKey == VK_SHIFT)
        {
            aKey = static_cast<uint16_t>(MapVirtualKey(aScanCode, MAPVK_VSC_TO_VK_EX));
        }
        else if (aKey == VK_NUMLOCK)
        {
            aScanCode = static_cast<uint16_t>(MapVirtualKey(aKey, MAPVK_VK_TO_VSC) | 0x100);
        }

        if (aE1)
        {
            if (aKey == VK_PAUSE)
            {
                aScanCode = 0x45;
            }
            else
            {
                aScanCode = static_cast<uint16_t>(MapVirtualKey(aKey, MAPVK_VK_TO_VSC));
            }
        }

        if (aE0)
        {
            switch (aKey)
            {
            case VK_CONTROL: aKey = VK_RCONTROL; break;
            case VK_MENU: aKey = VK_RMENU; break;
            case VK_RETURN: aKey = VK_SEPARATOR; break;
            }
        }
        else
        {
            switch (aKey)
            {
            case VK_CONTROL: aKey = VK_LCONTROL; break;
            case VK_MENU: aKey = VK_LMENU; break;
            case VK_INSERT: aKey = VK_NUMPAD0; break;
            case VK_DELETE: aKey = VK_DECIMAL; break;
            case VK_HOME: aKey = VK_NUMPAD7; break;
            case VK_END: aKey = VK_NUMPAD1; break;
            case VK_PRIOR: aKey = VK_NUMPAD9; break;
            case VK_NEXT: aKey = VK_NUMPAD3; break;
            case VK_LEFT: aKey = VK_NUMPAD4; break;
            case VK_RIGHT: aKey = VK_NUMPAD6; break;
            case VK_UP: aKey = VK_NUMPAD8; break;
            case VK_DOWN: aKey = VK_NUMPAD2; break;
            case VK_CLEAR: aKey = VK_NUMPAD5; break;
            }
        }
    }

    auto& overlay = *s_pOverlay;

    const auto pApp = overlay.GetOverlayApp();
    if (!pApp)
        return;

    const auto pClient = pApp->GetClient();
    if (!pClient)
        return;

    const auto pRenderer = pClient->GetOverlayRenderHandler();
    if (!pRenderer)
        return;

    const auto active = overlay.GetActive();

    spdlog::debug("ProcessKey, type: {}, key: {}, active: {}", aType, aKey, active);

    if (aType != KEYEVENT_CHAR && (IsToggleKey(aKey) || (IsDisableKey(aKey) && active)))
    {
        if (!overlay.GetInGame())
        {
            TiltedPhoques::DInputHook::Get().SetEnabled(false);
        }
        else if (aType == KEYEVENT_KEYUP)
        {
            SetUIActive(overlay, !active);
        }
    }
    else if (active)
    {
        pApp->InjectKey(aType, GetCefModifiers(aKey), aKey, aScanCode);
    }
}

void ProcessMouseMove(uint16_t aX, uint16_t aY)
{
    auto& overlay = *s_pOverlay;

    const auto pApp = overlay.GetOverlayApp();
    if (!pApp)
        return;

    const auto pClient = pApp->GetClient();
    if (!pClient)
        return;

    const auto pRenderer = pClient->GetOverlayRenderHandler();
    if (!pRenderer)
        return;

    const auto active = overlay.GetActive();

    if (active)
    {
        pApp->InjectMouseMove(aX, aY, GetCefModifiers(0));
    }
}

bool InputService::OpenMenuShortcutAt(uint16_t aX, uint16_t aY)
{
    if (!s_pOverlay || !s_pOverlay->IsMenuShortcut(aX, aY))
        return false;
    SetUIActive(*s_pOverlay, true);
    auto* app = s_pOverlay->GetOverlayApp();
    app->InjectMouseMove(aX, aY, 0);
    app->InjectMouseButton(aX, aY, MBT_LEFT, false, 0);
    app->InjectMouseButton(aX, aY, MBT_LEFT, true, 0);
    return true;
}

static bool OpenMenuShortcut()
{
    auto* window = BSGraphics::GetMainWindow();
    const auto* cursor = MenuCursor::Get();
    if (!window || !window->IsForeground() || Playtest::Active() || !s_pOverlay || !s_pOverlay->GetInGame() ||
        !cursor || cursor->width <= 0 || cursor->height <= 0)
        return false;
    CefRect viewport;
    s_pOverlay->GetOverlayApp()->GetClient()->GetOverlayRenderHandler()->GetViewRect(nullptr, viewport);
    const float x = cursor->x * viewport.width / cursor->width;
    const float y = cursor->y * viewport.height / cursor->height;
    if (x < 0 || y < 0 || x > UINT16_MAX || y > UINT16_MAX ||
        !InputService::OpenMenuShortcutAt(static_cast<uint16_t>(x), static_cast<uint16_t>(y)))
        return false;
    return true;
}

void ProcessMouseButton(uint16_t aX, uint16_t aY, cef_mouse_button_type_t aButton, bool aDown)
{
    auto& overlay = *s_pOverlay;

    const auto pApp = overlay.GetOverlayApp();
    if (!pApp)
        return;

    const auto pClient = pApp->GetClient();
    if (!pClient)
        return;

    const auto pRenderer = pClient->GetOverlayRenderHandler();
    if (!pRenderer)
        return;

    const auto active = overlay.GetActive();

    if (!active && aButton == MBT_LEFT && aDown && InputService::OpenMenuShortcutAt(aX, aY))
    {
        return;
    }

    if (active)
    {
        pApp->InjectMouseButton(aX, aY, aButton, !aDown, GetCefModifiers(0));
    }
}

void ProcessMouseWheel(uint16_t aX, uint16_t aY, int16_t aZ)
{
    auto& overlay = *s_pOverlay;

    const auto pApp = overlay.GetOverlayApp();
    if (!pApp)
        return;

    const auto pClient = pApp->GetClient();
    if (!pClient)
        return;

    const auto pRenderer = pClient->GetOverlayRenderHandler();
    if (!pRenderer)
        return;

    const auto active = overlay.GetActive();

    if (active)
    {
        pApp->InjectMouseWheel(aX, aY, aZ, GetCefModifiers(0));
    }
}

UINT GetRealACP()
{
    // Get the keyboard layout for the current thread.
    HKL keybdLayout = GetKeyboardLayout(0);

    // Extract the language ID from it, contained in its low-order word.
    int langID = LOWORD(keybdLayout);

    // Call the GetLocaleInfo function to retrieve the default ANSI code page
    // associated with that language ID.
    UINT acp = CP_ACP;
    GetLocaleInfo(MAKELCID(langID, SORT_DEFAULT),
        LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
        (LPTSTR) &acp,
        sizeof(acp) / sizeof(TCHAR));
    return acp;
}

static constexpr UINT_PTR kCursorTimerId = 0x5A7;

using TShowCursor = int(WINAPI*)(BOOL);
static TShowCursor RealShowCursor = nullptr;

static bool GameIsForeground()
{
    auto* window = BSGraphics::GetMainWindow();
    return window && window->IsForeground();
}

// Skyrim and CEF render their own cursors; suppress the detached Windows pointer while the game is focused.
// Returning 0 preserves Skyrim's visible-cursor state without making ShowCursor's OS pointer visible.
static int WINAPI HookShowCursor(BOOL aShow)
{
    if (!RealShowCursor)
        return 0;
    if (!aShow)
        return RealShowCursor(aShow);

    if (!GameIsForeground())
        return RealShowCursor(aShow);

    RealShowCursor(FALSE);
    return 0;
}

// Skyrim stops delivering WM_SETCURSOR once the pointer is hidden, so the only reliable way to notice a
// cursor that came back is to poll. ShowCursor is thread-local: this must run on the window thread.
static void NormalizeCursor()
{
    if (!GameIsForeground())
        return;

    CURSORINFO info{sizeof(CURSORINFO)};
    if (!GetCursorInfo(&info))
        return;

    const bool shown = (info.flags & CURSOR_SHOWING) != 0;
    if (!shown)
        return;

    while (RealShowCursor(FALSE) >= 0)
        ;
}

static bool s_cursorClipped = false;
static bool s_movingWindow = false;

static void ReleaseCursorClip()
{
    if (!s_cursorClipped)
        return;

    // Unconditional calls would pull the clip out from under a second instance that has focus.
    s_cursorClipped = false;
    ClipCursor(nullptr);
}

// The game reads relative mouse input, so the hidden Windows pointer otherwise roams over every monitor, hovering
// whatever is there, and a click out there lands on that window. Windows and DirectInput both drop the clip on their
// own (focus changes, Unacquire), so this runs on the cursor timer and restores it rather than setting it once.
static void UpdateCursorClip(HWND aHwnd)
{
    if (!UiRules::ShouldClipCursor(GameIsForeground(), IsIconic(aHwnd) != FALSE, s_movingWindow, Playtest::Active()))
    {
        ReleaseCursorClip();
        return;
    }

    RECT client;
    if (!GetClientRect(aHwnd, &client) || IsRectEmpty(&client))
        return;
    MapWindowPoints(aHwnd, nullptr, reinterpret_cast<POINT*>(&client), 2);

    RECT current;
    if (s_cursorClipped && GetClipCursor(&current) && EqualRect(&current, &client))
        return;

    s_cursorClipped = ClipCursor(&client) != FALSE;
}

static bool AffectsCursorClip(UINT aMessage, WPARAM aWParam)
{
    switch (aMessage)
    {
    case WM_TIMER: return aWParam == kCursorTimerId;
    case WM_ACTIVATE:
    case WM_ACTIVATEAPP:
    case WM_KILLFOCUS:
    case WM_SETFOCUS:
    case WM_SIZE:
    case WM_MOVE:
    case WM_DISPLAYCHANGE:
    case WM_ENTERSIZEMOVE:
    case WM_EXITSIZEMOVE: return true;
    default: return false;
    }
}

// Runs on its own thread: a low-level hook on the game thread would freeze the whole desktop's keyboard during a hitch.
static LRESULT CALLBACK SystemKeyHook(int aCode, WPARAM aWParam, LPARAM aLParam)
{
    static DWORD s_altDownTime = 0;

    if (aCode != HC_ACTION)
        return CallNextHookEx(nullptr, aCode, aWParam, aLParam);

    const auto* pKey = reinterpret_cast<const KBDLLHOOKSTRUCT*>(aLParam);
    const bool keyDown = aWParam == WM_KEYDOWN || aWParam == WM_SYSKEYDOWN;
    if (pKey->vkCode == VK_LMENU || pKey->vkCode == VK_RMENU)
    {
        if (!keyDown)
            s_altDownTime = 0;
        else if (!s_altDownTime)
            s_altDownTime = pKey->time;
    }

    bool block = false;
    if (GameIsForeground() && s_pOverlay && !s_pOverlay->GetActive() && !Playtest::Active())
    {
        const bool altDown = (pKey->flags & LLKHF_ALTDOWN) != 0;
        const bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        const DWORD altHeld = s_altDownTime ? pKey->time - s_altDownTime : 0;
        block = UiRules::ShouldBlockSystemKey(pKey->vkCode, keyDown, altDown, ctrlDown, altHeld);
    }

    // The rest of the chain includes DirectInput's own hook, which still has to see the key for the game.
    const LRESULT result = CallNextHookEx(nullptr, aCode, aWParam, aLParam);
    return block ? 1 : result;
}

static void RunSystemKeyHook()
{
    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, &SystemKeyHook, GetModuleHandleW(nullptr), 0);
    if (!hook)
    {
        spdlog::warn("Could not install the system key hook, error {}", GetLastError());
        return;
    }

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
    }
    UnhookWindowsHookEx(hook);
}

LRESULT CALLBACK InputService::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static bool s_cursorTimer = false;
    if (!s_cursorTimer)
    {
        SetTimer(hwnd, kCursorTimerId, 15, nullptr);
        s_cursorTimer = true;
    }

    if (uMsg == WM_ENTERSIZEMOVE)
        s_movingWindow = true;
    else if (uMsg == WM_EXITSIZEMOVE)
        s_movingWindow = false;

    if (uMsg == WM_DESTROY)
        ReleaseCursorClip();
    else if (AffectsCursorClip(uMsg, wParam))
        UpdateCursorClip(hwnd);

    // Virtual input is delivered directly; physical input must not move its cursor.
    if (Playtest::Active())
        return 0;
    const auto pApp = s_pOverlay->GetOverlayApp();
    if (!pApp)
        return 0;

    const auto pClient = pApp->GetClient();
    if (!pClient)
        return 0;

    const auto pRenderer = pClient->GetOverlayRenderHandler();
    if (!pRenderer)
        return 0;

    auto& discord = World::Get().ctx().at<DiscordService>();
    discord.WndProcHandler(hwnd, uMsg, wParam, lParam);

    const bool active = s_pOverlay->GetActive();
    if (active)
    {
        auto& imgui = World::Get().ctx().at<ImguiService>();
        imgui.WndProcHandler(hwnd, uMsg, wParam, lParam);
    }

    if (uMsg == WM_TIMER && wParam == kCursorTimerId)
    {
        NormalizeCursor();
        return 0;
    }

    // Skyrim and CEF draw their own cursor; keep the detached Windows pointer hidden over the game window.
    if (uMsg == WM_SETCURSOR && GameIsForeground())
    {
        ::SetCursor(nullptr);
        while (RealShowCursor(FALSE) >= 0)
            ;
        return 1;
    }

    POINT position;

    GetCursorPos(&position);
    ScreenToClient(hwnd, &position);

    ProcessMouseMove(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y));

    // Windowed/absolute mouse input can click between game polls, before raw input is enabled.
    if (uMsg == WM_LBUTTONDOWN && !active && position.x >= 0 && position.y >= 0 &&
        OpenMenuShortcutAt(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y)))
    {
        return 1;
    }

    if (uMsg == WM_INPUT)
    {
        RAWINPUT input;
        UINT size = sizeof(RAWINPUT);

        GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER));

        if (active)
        {
            auto& imgui = World::Get().ctx().at<ImguiService>();
            imgui.RawInputHandler(input);
        }

        if (input.header.dwType == RIM_TYPEKEYBOARD)
        {
            const auto keyboard = input.data.keyboard;

            ProcessKeyboard(keyboard.VKey, keyboard.MakeCode, keyboard.Flags & RI_KEY_BREAK ? KEYEVENT_KEYUP : KEYEVENT_KEYDOWN, keyboard.Flags & RI_KEY_E0, keyboard.Flags & RI_KEY_E1);
        }
        else if (input.header.dwType == RIM_TYPEMOUSE)
        {
            const auto mouse = input.data.mouse;

            if (mouse.usButtonFlags & RI_MOUSE_WHEEL)
            {
                ProcessMouseWheel(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y), (int16_t)mouse.usButtonData);
            }

            if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
            {
                ProcessMouseButton(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y), MBT_LEFT, true);
            }

            if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
            {
                ProcessMouseButton(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y), MBT_LEFT, false);
            }

            if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
            {
                ProcessMouseButton(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y), MBT_RIGHT, true);
            }

            if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
            {
                ProcessMouseButton(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y), MBT_RIGHT, false);
            }

            if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
            {
                ProcessMouseButton(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y), MBT_MIDDLE, true);
            }

            if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
            {
                ProcessMouseButton(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y), MBT_MIDDLE, false);
            }
        }
    }
    else if (uMsg == WM_CHAR)
    {
        uint16_t scancode = (lParam >> 16) & 0xFF;
        uint16_t virtualKey = static_cast<uint16_t>(wParam);
        if (!IsWindowUnicode(hwnd))
        {
            wchar_t wch;
            ::MultiByteToWideChar(s_currentACP, MB_PRECOMPOSED, reinterpret_cast<char*>(&virtualKey), 2, &wch, sizeof(wchar_t));
            virtualKey = wch;
        }
        ProcessKeyboard(virtualKey, scancode, KEYEVENT_CHAR, false, false);
    }
    // If the player tabs out/in with UI visible, this WndProc doesn't run during mouse or keyboard events.
    // When player tabs in, force the UI state
    else if (uMsg == WM_SETFOCUS && s_pOverlay->GetActive())
    {
        s_pOverlay->SetActive(true);
    }
    else if (uMsg == WM_INPUTLANGCHANGE)
    {
        s_currentACP = GetRealACP();
        spdlog::info("Input language changed, current ACP: {}", s_currentACP);
    }

    return 0;
}

InputService::InputService(OverlayService& aOverlay) noexcept
{
    s_pOverlay = &aOverlay;
    TiltedPhoques::DInputHook::Get().OnLeftMousePress = OpenMenuShortcut;
    s_currentACP = GetRealACP();
    TP_HOOK_IAT(ShowCursor, "user32.dll");

    // Windows swaps a game that stops pumping messages for 5s for a ghost window, which takes the foreground away.
    DisableProcessWindowsGhosting();
    std::thread(RunSystemKeyHook).detach();
}

InputService::~InputService() noexcept
{
    TiltedPhoques::DInputHook::Get().OnLeftMousePress = nullptr;
    ReleaseCursorClip();
    s_pOverlay = nullptr;
}
