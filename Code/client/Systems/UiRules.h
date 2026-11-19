#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Input and HUD rules that need no game or Windows headers, so TPTests can cover them.
namespace UiRules
{
// F2 opens the overlay anywhere; the clickable strip stays only on the two screens with room for it.
inline constexpr std::array<const char*, 2> kMenuShortcutMenus{"RaceSex Menu", "Journal Menu"};
// Everything here fills the screen with lists the strip and party frames would cover.
inline constexpr std::array<const char*, 8> kFullscreenMenus{"InventoryMenu", "MagicMenu", "ContainerMenu", "BarterMenu",
                                                             "GiftMenu",      "Crafting Menu", "MapMenu",   "TweenMenu"};

struct Rect
{
    int X, Y, Width, Height;

    [[nodiscard]] constexpr bool Contains(int aX, int aY) const noexcept { return aX >= X && aX < X + Width && aY >= Y && aY < Y + Height; }
};

// .multiplayer-shortcut at 1080 lines (left, top, width, height), scaled with the screen height like the page's --sbt-u.
inline constexpr Rect kMenuShortcutAt1080{16, 96, 160, 180};

// Each edge where the browser paints it: layout keeps 1/64 of a pixel, and edges snap to the nearest pixel
[[nodiscard]] constexpr Rect MenuShortcutRect(int aViewHeight) noexcept
{
    const auto edge = [aViewHeight](int aValue) {
        const long long sixtyFourths = static_cast<long long>(aValue) * aViewHeight * 64 / 1080;
        return static_cast<int>((sixtyFourths + 32) / 64);
    };
    const Rect& r = kMenuShortcutAt1080;
    const int left = edge(r.X), top = edge(r.Y);
    return {left, top, edge(r.X + r.Width) - left, edge(r.Y + r.Height) - top};
}

[[nodiscard]] constexpr bool ShouldClipCursor(bool aForeground, bool aMinimized, bool aMovingWindow, bool aPlaytest) noexcept
{
    return aForeground && !aMinimized && !aMovingWindow && !aPlaytest;
}

// Sprint is Alt and the Tween menu is Tab by default. Alt held this long is sprinting, not the start of an Alt+Tab.
inline constexpr uint32_t kAltHeldForSprintMs = 400;

// The keyboard is non-exclusive so media keys work, which leaves these shortcuts live while playing.
// DirectInput's exclusive mode eats all but Alt+Tab, so vanilla never tabs out on them.
[[nodiscard]] constexpr bool ShouldBlockSystemKey(uint32_t aVirtualKey, bool aKeyDown, bool aAltDown, bool aCtrlDown, uint32_t aAltHeldMs) noexcept
{
    constexpr uint32_t kTab = 0x09, kEscape = 0x1B, kLeftWin = 0x5B, kRightWin = 0x5C;

    // Windows opens Start on the key up, so both halves have to go.
    if (aVirtualKey == kLeftWin || aVirtualKey == kRightWin)
        return true;
    if (!aKeyDown)
        return false;
    if (aVirtualKey == kEscape)
        return aAltDown || aCtrlDown;
    return aVirtualKey == kTab && aAltDown && aAltHeldMs >= kAltHeldForSprintMs;
}

struct CompassMarker
{
    double Heading;
    double Alpha;
    double Scale;
};

// Same curve as HUDMarkerManager::AddMarker with the quest marker constants, so players far away stay readable.
[[nodiscard]] inline CompassMarker MakeCompassMarker(float aDeltaX, float aDeltaY, float aDeltaZ) noexcept
{
    constexpr float kFadeStart = 7500.f, kMinAlpha = 90.f, kMinScale = 100.f, kMaxScale = 150.f;
    constexpr float kFadeStartSquared = kFadeStart * kFadeStart;
    constexpr float kPi = 3.14159265358979f;

    const float distanceSquared = aDeltaX * aDeltaX + aDeltaY * aDeltaY + aDeltaZ * aDeltaZ;
    const float nearness = (kFadeStartSquared - std::min(distanceSquared, kFadeStartSquared)) / kFadeStartSquared;

    float heading = std::atan2(aDeltaX, aDeltaY);
    if (heading < 0.f)
        heading += 2.f * kPi;

    return {heading * 180.f / kPi, kMinAlpha + (100.f - kMinAlpha) * nearness, kMinScale + (kMaxScale - kMinScale) * nearness};
}

// What a compass target is, as the HUD helper (Tools/Hud/compass.py) draws it
enum class CompassKind : uint8_t
{
    Player = 0,   // A coloured disc of its own
    Waypoint = 1, // The game's custom marker, tinted
};

// One colour per party member, in the order of their player ids, so every member sees each player in the same colour.
// Bright and apart from each other on the compass, which draws everything else white.
inline constexpr std::array<uint32_t, 8> kPlayerColours{0xFFC94A, 0x4AD8FF, 0xFF5CD6, 0x8CFF4A, 0xFF8A3D, 0xB28CFF, 0xFF5050, 0x3DFFC0};
inline constexpr uint32_t kNotInPartyColour = 0xC8C8C8;

template <class T> [[nodiscard]] uint32_t PlayerColour(uint32_t aPlayerId, const T& acPartyMembers) noexcept
{
    if (std::find(std::begin(acPartyMembers), std::end(acPartyMembers), aPlayerId) == std::end(acPartyMembers))
        return kNotInPartyColour;
    const auto cRank = std::count_if(std::begin(acPartyMembers), std::end(acPartyMembers), [aPlayerId](uint32_t aMember) { return aMember < aPlayerId; });
    return kPlayerColours[static_cast<size_t>(cRank) % kPlayerColours.size()];
}

// Coordinates only compare within one worldspace or one interior. A world of 0 means an interior.
[[nodiscard]] constexpr bool SharesCompassSpace(uint32_t aWorldSpace, uint32_t aCell, uint32_t aOtherWorldSpace, uint32_t aOtherCell) noexcept
{
    if (aWorldSpace != aOtherWorldSpace)
        return false;
    return aWorldSpace != 0 || (aCell != 0 && aCell == aOtherCell);
}
} // namespace UiRules
