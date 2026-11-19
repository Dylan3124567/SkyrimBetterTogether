#pragma once

// Shared with the in-process playtest cursor; coordinates use the game's menu viewport.
struct MenuCursor
{
    uint32_t pad;
    float x, y, safeX, safeY, width, height, sensitivity, unknown20, unknown24, speed;
    int32_t showCursorCount; // cached ShowCursor result; >= 0 means the OS cursor is shown

    static MenuCursor* Get()
    {
        static const VersionDbPtr<MenuCursor*> singleton(403551);
        return *singleton.Get();
    }
};
static_assert(sizeof(MenuCursor) == 0x30);
