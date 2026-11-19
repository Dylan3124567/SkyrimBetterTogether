#include <TiltedOnlinePCH.h>
#include <Playtest.h>
#include <DInputHook.hpp>
#include <dinput.h>
#include <World.h>
#include <Services/OverlayService.h>
#include <Interface/MenuCursor.h>
#include <Interface/UI.h>

namespace Playtest
{
static bool s_cursorDirty{};
bool Cursor(float& x, float& y, bool set)
{
    auto* cursor = MenuCursor::Get();
    if (!cursor || cursor->width <= 0 || cursor->height <= 0) return false;
    if (set)
    {
        cursor->x = std::clamp(x, 0.f, cursor->width);
        cursor->y = std::clamp(y, 0.f, cursor->height);
        s_cursorDirty = true;
    }
    x = cursor->x;
    y = cursor->y;
    return true;
}
// powerof3/CommonLibSSE AE BSInputEventQueue/InputEvent layouts and singleton ID.
// Reuse the engine's initialized cached events, including their vtables.
struct Event
{
    void* vtable;
    uint32_t device, type;
    Event* next;
    BSFixedString userEvent;
    uint32_t id, pad;
    union { float value; int32_t x; };
    union { float duration; int32_t y; };
};
struct Queue
{
    uint32_t pad0, buttons, chars, mice, sticks, connects, kinects, sixaxis, gestures, amiibo;
    Event buttonEvents[10];
    char charEvents[0xA0];
    Event mouse;
    char remainder[0x280];
    Event* head;
    Event* tail;
    void Push(Event* event)
    {
        if (!head) head = event;
        if (tail) tail->next = event;
        tail = event;
        event->next = nullptr;
    }
};
static_assert(sizeof(Event) == 0x30);
static_assert(offsetof(Event, userEvent) == 0x18);
static_assert(offsetof(Queue, mouse) == 0x2A8);
static_assert(offsetof(Queue, head) == 0x558);
static_assert(sizeof(Queue) == 0x568);

void PollMouse(float delta)
{
    static float held[8]{};
    if (!Active()) { std::fill(std::begin(held), std::end(held), 0.f); return; }
    auto callback = TiltedPhoques::DInputHook::Get().VirtualState;
    DIMOUSESTATE2 mouse{};
    if (!callback || !callback(false, sizeof(mouse), &mouse)) return;
    if (!Baseline() && World::Get().GetOverlayService().GetActive()) return;
    static const VersionDbPtr<Queue*> singleton(407374);
    auto* queue = *singleton.Get();
    if (!queue || queue->buttons > 10 || queue->mice > 1) return;
    if ((mouse.lX || mouse.lY || s_cursorDirty) && !queue->mice && queue->mouse.vtable)
    {
        auto& event = queue->mouse;
        event.device = 1;
        event.type = 1;
        event.userEvent = BSFixedString(UI::Get() && UI::Get()->GetMenuOpen("Lockpicking Menu") ? "RotatePick" : "Look");
        event.x = mouse.lX;
        event.y = mouse.lY;
        queue->Push(&event);
        ++queue->mice;
        s_cursorDirty = false;
    }
    for (unsigned i = 0; i < 10 && queue->buttons < 10; ++i)
    {
        const bool down = i < 8 ? mouse.rgbButtons[i] != 0 : (i == 8 ? mouse.lZ > 0 : mouse.lZ < 0);
        const float previous = i < 8 ? held[i] : 0.f;
        if (!down && !previous) continue;
        auto& event = queue->buttonEvents[queue->buttons];
        if (!event.vtable) continue;
        event.device = 1;
        event.type = 0;
        event.userEvent = BSFixedString();
        event.id = i;
        event.value = down ? 1.f : 0.f;
        event.duration = previous;
        queue->Push(&event);
        ++queue->buttons;
        if (i < 8) held[i] = down ? previous + std::max(delta, .001f) : 0.f;
    }
}
}
