#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Allocator.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>
#include <optional>
#include <glm/glm.hpp>
#include <Messages/ClientMessageFactory.h>
#include <Messages/ServerMessageFactory.h>
#include <Systems/UiRules.h>
#include <catch2/catch.hpp>
#include <algorithm>
#include <string_view>

using namespace UiRules;

TEST_CASE("Ordinary game menus don't get the multiplayer strip", "[ui]")
{
    const auto hasShortcut = [](std::string_view aMenu) {
        return std::find(kMenuShortcutMenus.begin(), kMenuShortcutMenus.end(), aMenu) != kMenuShortcutMenus.end();
    };
    for (const char* menu : kFullscreenMenus)
        REQUIRE_FALSE(hasShortcut(menu));

    REQUIRE(hasShortcut("RaceSex Menu"));
    REQUIRE(hasShortcut("Journal Menu"));
}

TEST_CASE("The strip's rectangle follows the screen height and matches what the page paints", "[ui]")
{
    constexpr Rect cAt1080 = MenuShortcutRect(1080);
    STATIC_REQUIRE(cAt1080.X == 16);
    STATIC_REQUIRE(cAt1080.Y == 96);
    STATIC_REQUIRE(cAt1080.Width == 160);
    STATIC_REQUIRE(cAt1080.Height == 180);

    // Edges at 10.67, 64, 117.33 and 184, Chromium painted x 11 to 117 and y 64 to 184
    constexpr Rect cAt720 = MenuShortcutRect(720);
    REQUIRE(cAt720.X == 11);
    REQUIRE(cAt720.Y == 64);
    REQUIRE(cAt720.X + cAt720.Width == 117);
    REQUIRE(cAt720.Y + cAt720.Height == 184);

    // Painted x 11 to 125 and y 68 to 196
    constexpr Rect cAt768 = MenuShortcutRect(768);
    REQUIRE(cAt768.X == 11);
    REQUIRE(cAt768.X + cAt768.Width == 125);
    REQUIRE(cAt768.Y == 68);
    REQUIRE(cAt768.Y + cAt768.Height == 196);

    constexpr Rect cAt1440 = MenuShortcutRect(1440);
    REQUIRE(cAt1440.X == 21);
    REQUIRE(cAt1440.Width == 235 - 21);
    REQUIRE(cAt1440.Contains(21, 128));
    REQUIRE_FALSE(cAt1440.Contains(20, 128));
    REQUIRE_FALSE(cAt1440.Contains(235, 128));
}

TEST_CASE("The cursor is only held while the game owns the mouse", "[ui]")
{
    REQUIRE(ShouldClipCursor(true, false, false, false));
    REQUIRE_FALSE(ShouldClipCursor(false, false, false, false));
    REQUIRE_FALSE(ShouldClipCursor(true, true, false, false));
    REQUIRE_FALSE(ShouldClipCursor(true, false, true, false));
    REQUIRE_FALSE(ShouldClipCursor(true, false, false, true));
}

TEST_CASE("System shortcuts can't tab a player out mid-fight, a quick Alt+Tab still can", "[ui]")
{
    constexpr uint32_t kTab = 0x09, kEscape = 0x1B, kLeftWin = 0x5B, kRightWin = 0x5C, kSpace = 0x20;
    constexpr uint32_t kHeld = kAltHeldForSprintMs;

    for (bool keyDown : {true, false})
    {
        REQUIRE(ShouldBlockSystemKey(kLeftWin, keyDown, false, false, 0));
        REQUIRE(ShouldBlockSystemKey(kRightWin, keyDown, false, false, 0));
    }

    REQUIRE(ShouldBlockSystemKey(kEscape, true, true, false, 0));
    REQUIRE(ShouldBlockSystemKey(kEscape, true, false, true, 0));
    REQUIRE_FALSE(ShouldBlockSystemKey(kEscape, true, false, false, 0));
    REQUIRE_FALSE(ShouldBlockSystemKey(kEscape, false, true, false, 0));

    REQUIRE(ShouldBlockSystemKey(kTab, true, true, false, kHeld));
    REQUIRE_FALSE(ShouldBlockSystemKey(kTab, true, true, false, kHeld - 1));
    REQUIRE_FALSE(ShouldBlockSystemKey(kTab, true, false, false, kHeld));
    REQUIRE_FALSE(ShouldBlockSystemKey(kTab, false, true, false, kHeld));

    REQUIRE_FALSE(ShouldBlockSystemKey(kSpace, true, true, true, kHeld));
}

TEST_CASE("Compass markers match the game's heading and fade", "[ui]")
{
    REQUIRE(MakeCompassMarker(0.f, 100.f, 0.f).Heading == Approx(0.0));
    REQUIRE(MakeCompassMarker(100.f, 0.f, 0.f).Heading == Approx(90.0));
    REQUIRE(MakeCompassMarker(0.f, -100.f, 0.f).Heading == Approx(180.0));
    REQUIRE(MakeCompassMarker(-100.f, 0.f, 0.f).Heading == Approx(270.0));

    const auto close = MakeCompassMarker(0.f, 0.f, 0.f);
    REQUIRE(close.Alpha == Approx(100.0));
    REQUIRE(close.Scale == Approx(150.0));

    const auto far = MakeCompassMarker(100000.f, 0.f, 0.f);
    REQUIRE(far.Alpha == Approx(90.0));
    REQUIRE(far.Scale == Approx(100.0));
}

TEST_CASE("Compass markers only compare positions within one space", "[ui]")
{
    constexpr uint32_t kTamriel = 0x3C, kSolstheim = 0x1, kCell = 0x1000, kOtherCell = 0x2000;

    REQUIRE(SharesCompassSpace(kTamriel, kCell, kTamriel, kOtherCell));
    REQUIRE_FALSE(SharesCompassSpace(kTamriel, kCell, kSolstheim, kCell));
    REQUIRE(SharesCompassSpace(0, kCell, 0, kCell));
    REQUIRE_FALSE(SharesCompassSpace(0, kCell, 0, kOtherCell));
    REQUIRE_FALSE(SharesCompassSpace(0, kCell, kTamriel, kCell));
    REQUIRE_FALSE(SharesCompassSpace(0, 0, 0, 0));
}

TEST_CASE("Player positions round trip", "[encoding.ui]")
{
    NotifyPlayerPositions sent;
    sent.Entries.push_back({1, GameId(0, 0x3C), GameId(0, 0x1234), glm::vec3(1.5f, -2.5f, 3.f)});
    sent.Entries.push_back({70000, GameId(), GameId(1, 0x5678), glm::vec3(-100000.f, 200000.f, -300.f)});

    TiltedPhoques::Buffer buffer(1024);
    TiltedPhoques::Buffer::Writer writer(&buffer);
    sent.Serialize(writer);

    TiltedPhoques::Buffer::Reader reader(&buffer);
    ServerMessageFactory factory;
    auto received = factory.Extract(reader);
    REQUIRE(received);
    REQUIRE(received->GetOpcode() == kNotifyPlayerPositions);
    REQUIRE(*static_cast<NotifyPlayerPositions*>(received.get()) == sent);
}
