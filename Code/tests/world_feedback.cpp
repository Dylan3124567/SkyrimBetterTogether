#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Allocator.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>
#include <optional>
#include <glm/glm.hpp>
#include <Messages/ClientMessageFactory.h>
#include <Messages/ServerMessageFactory.h>
#include <Structs/TimeSyncRules.h>
#include <Structs/WorldObjectState.h>
#include <catch2/catch.hpp>
#include <array>
#include <vector>

namespace
{
// One frame of game time as common/DateTime::GetDeltaTime gives it
float FrameHours(uint64_t aMs, float aTimeScale = 20.f)
{
    return (static_cast<float>(aMs) / 1000.f) * (aTimeScale * 0.00027777778f);
}
} // namespace

TEST_CASE("GameDaysPassed keeps running in old saves and at high frame rates", "[time]")
{
    const float hours = FrameHours(7); // 144 fps
    constexpr int kFrames = 514286;    // one real hour, 20 game hours

    // How the calendar used to add a frame: below the float's precision at day 150, so the days stood still.
    float oldDays = 150.f;
    for (int i = 0; i < 1000; ++i)
        oldDays += hours * (1.f / 24.f);
    CHECK(oldDays == 150.f);

    TimeSyncRules::DaysPassed clock;
    float days = 150.f;
    for (int i = 0; i < kFrames; ++i)
        days = clock.Advance(days, hours);
    REQUIRE(days == Approx(150.0 + kFrames * double(hours) / 24.0).margin(1e-4));
    REQUIRE(days == Approx(150.0 + 20.0 / 24.0).margin(2e-3));
}

TEST_CASE("GameDaysPassed follows what the game sets and stands still while waiting in bed", "[time][sleep]")
{
    TimeSyncRules::DaysPassed clock;
    float days = clock.Advance(40.25f, 0.0);
    REQUIRE(days == 40.25f);

    // Native sleep steps set it themselves (0.25 hours each)
    days = clock.Advance(40.5f, 0.0);
    REQUIRE(days == 40.5f);
    days = clock.Advance(days, 6.0);
    REQUIRE(days == Approx(40.75).margin(1e-5));

    // The first one in bed waits for the others with the days held: Survival's OnSleepStart to OnSleepStop then spans
    // only the shared sleep, the same hours as for the last one in.
    const float lyingDown = days;
    for (int i = 0; i < 7200; ++i)
        days = clock.Advance(days, 0.0);
    REQUIRE(days == lyingDown);
}

TEST_CASE("The server resends its clock away from midnight", "[time]")
{
    REQUIRE(TimeSyncRules::kResyncIntervalMs == 60000);
    REQUIRE_FALSE(TimeSyncRules::CanResyncAt(0.f));
    REQUIRE_FALSE(TimeSyncRules::CanResyncAt(0.05f));
    REQUIRE_FALSE(TimeSyncRules::CanResyncAt(23.95f));
    REQUIRE(TimeSyncRules::CanResyncAt(0.1f));
    REQUIRE(TimeSyncRules::CanResyncAt(12.f));
    REQUIRE(TimeSyncRules::CanResyncAt(23.9f));
}

TEST_CASE("A door a player opened stays open for players who come later", "[objects]")
{
    using namespace WorldObjectRules;
    REQUIRE(OpenStateAfterActivation(kClosed) == kOpen);
    REQUIRE(OpenStateAfterActivation(kClosing) == kOpen);
    REQUIRE(OpenStateAfterActivation(kOpen) == kClosed);
    REQUIRE(OpenStateAfterActivation(kOpening) == kClosed);
    REQUIRE(OpenStateAfterActivation(kNone) == kNone);
    REQUIRE(OpenStateAfterActivation(9) == kNone);

    constexpr uint32_t kOpener = 1, kWithOpener = 2, kAway = 3;
    const std::array<uint32_t, 2> inCell{kOpener, kWithOpener};
    WorldObjectMemory memory;
    const GameId cistern(0, 0xC19C0); // TGSecretDoor01 in RiftenRaggedFlagon
    REQUIRE_FALSE(memory.TakeFor(cistern, kAway));

    memory.RecordActivation(cistern, kClosed, inCell);
    // The opener and whoever was there got it live
    REQUIRE_FALSE(memory.TakeFor(cistern, kOpener));
    REQUIRE_FALSE(memory.TakeFor(cistern, kWithOpener));
    auto state = memory.TakeFor(cistern, kAway);
    REQUIRE(state);
    REQUIRE(state->Id == cistern);
    REQUIRE(state->OpenState == kOpen);
    REQUIRE_FALSE(state->HasLock);
    // Once: a script closing it again on that client isn't undone on every visit
    REQUIRE_FALSE(memory.TakeFor(cistern, kAway));

    LockData unlocked;
    unlocked.IsLocked = false;
    unlocked.LockLevel = 255;
    const std::array<uint32_t, 1> opener{kOpener};
    memory.RecordLock(cistern, unlocked, opener);
    state = memory.TakeFor(cistern, kAway);
    REQUIRE(state);
    REQUIRE(state->HasLock);
    REQUIRE(state->Lock == unlocked);
    REQUIRE(state->OpenState == kNone); // Already given
    state = memory.TakeFor(cistern, kWithOpener);
    REQUIRE(state);
    REQUIRE(state->HasLock);
    REQUIRE(state->OpenState == kNone);

    memory.RecordActivation(cistern, kOpen, opener);
    state = memory.TakeFor(cistern, kAway);
    REQUIRE(state);
    REQUIRE(state->OpenState == kClosed);
    REQUIRE_FALSE(state->HasLock);

    // Not a door, or no id: nothing to remember
    memory.RecordActivation(GameId(0, 0x12345), kNone, opener);
    memory.RecordActivation(GameId{}, kClosed, opener);
    memory.RecordLock(GameId{}, unlocked, opener);
    REQUIRE(memory.Size() == 1);
}

TEST_CASE("Remembered door states only go to persistent doors that aren't load doors", "[objects]")
{
    using namespace WorldObjectRules;
    REQUIRE(CanApplyTo(true, kPersistentFormFlag, false));
    REQUIRE(CanApplyTo(true, kPersistentFormFlag | 0x800, false));
    REQUIRE_FALSE(CanApplyTo(true, 0, false));
    REQUIRE_FALSE(CanApplyTo(true, kPersistentFormFlag, true));
    REQUIRE_FALSE(CanApplyTo(false, kPersistentFormFlag, false));

    REQUIRE(NeedsOpenChange(kOpen, kClosed));
    REQUIRE(NeedsOpenChange(kOpen, kClosing));
    REQUIRE(NeedsOpenChange(kClosed, kOpening));
    REQUIRE_FALSE(NeedsOpenChange(kOpen, kOpening));
    REQUIRE_FALSE(NeedsOpenChange(kClosed, kClosed));
    REQUIRE_FALSE(NeedsOpenChange(kNone, kOpen));
}

TEST_CASE("Junk ids can't switch the door memory off: the least recently changed door goes", "[objects]")
{
    using namespace WorldObjectRules;
    const std::array<uint32_t, 1> opener{1};
    const auto kCap = static_cast<uint32_t>(kMaxRemembered);
    WorldObjectMemory memory;
    for (uint32_t i = 1; i <= kCap; ++i)
        memory.RecordActivation(GameId(1, i), kClosed, opener);
    REQUIRE(memory.Size() == kMaxRemembered);

    // Door 1 changes again, so door 2 is now the least recently changed
    memory.RecordActivation(GameId(1, 1), kOpen, opener);

    // A full memory still takes new doors (it used to refuse every one from here on)
    const GameId cistern(0, 0xC19C0);
    memory.RecordActivation(cistern, kClosed, opener);
    REQUIRE(memory.Size() == kMaxRemembered);
    REQUIRE(memory.Contains(cistern));
    REQUIRE_FALSE(memory.Contains(GameId(1, 2)));
    REQUIRE(memory.Contains(GameId(1, 1)));
    REQUIRE(memory.Contains(GameId(1, 3)));

    const auto state = memory.TakeFor(cistern, 2);
    REQUIRE(state);
    REQUIRE(state->OpenState == kOpen);
    const auto touched = memory.TakeFor(GameId(1, 1), 2);
    REQUIRE(touched);
    REQUIRE(touched->OpenState == kClosed);
}

TEST_CASE("A change counts as given to a bounded number of players", "[objects]")
{
    using namespace WorldObjectRules;
    std::vector<uint32_t> many(kMaxHolders + 20);
    for (uint32_t i = 0; i < many.size(); ++i)
        many[i] = 100 + i;
    WorldObjectMemory memory;
    const GameId door(0, 0x1234);
    memory.RecordActivation(door, kClosed, many);
    // Only the first kMaxHolders count as having it
    REQUIRE_FALSE(memory.TakeFor(door, many.front()));
    REQUIRE(memory.TakeFor(door, many.back()));
    REQUIRE_FALSE(memory.TakeFor(door, many.back()));
}

TEST_CASE("Only a sender in the object's own cell speaks for it", "[objects]")
{
    const GameId flagon(0, 0x16BCF), riften(0, 0x16778);
    WorldObjectIndex<uint32_t> objects;
    REQUIRE(objects.Add(GameId(0, 0xC19C0), flagon, 7));

    // The packet's claimed cell doesn't count, the cell the server assigned the object in does
    REQUIRE(WorldObjectRules::InSendersCell(objects.CellOf(GameId(0, 0xC19C0)), flagon));
    REQUIRE_FALSE(WorldObjectRules::InSendersCell(objects.CellOf(GameId(0, 0xC19C0)), riften));
    // An id the server never assigned: nothing to record
    REQUIRE_FALSE(WorldObjectRules::InSendersCell(objects.CellOf(GameId(0, 0xBEEF)), flagon));
    REQUIRE_FALSE(WorldObjectRules::InSendersCell(std::optional<GameId>(GameId{}), GameId{}));
}

TEST_CASE("Object entities are found by id and capped per cell", "[objects]")
{
    using namespace WorldObjectRules;
    const GameId cell(0, 0x16BCF), other(0, 0x16778);
    WorldObjectIndex<uint32_t> objects;

    REQUIRE_FALSE(objects.Add(GameId{}, cell, 1));
    REQUIRE_FALSE(objects.Add(GameId(0, 1), GameId{}, 1));
    for (uint32_t i = 1; i <= kMaxObjectsPerCell; ++i)
        REQUIRE(objects.Add(GameId(1, i), cell, i));
    REQUIRE(objects.CellFull(cell));
    // One request's worth of junk ids can't go past the cap, and another cell still takes objects
    REQUIRE_FALSE(objects.Add(GameId(2, 1), cell, 100000));
    REQUIRE(objects.Add(GameId(2, 1), other, 100000));
    REQUIRE_FALSE(objects.Add(GameId(2, 1), other, 100001)); // id already assigned
    REQUIRE(objects.Size() == kMaxObjectsPerCell + 1);

    REQUIRE(objects.Find(GameId(1, 5)) == std::optional<uint32_t>(5));
    REQUIRE(objects.CellOf(GameId(2, 1)) == std::optional<GameId>(other));

    // Destroyed entities leave the index and free their cell's room
    objects.Remove(5);
    REQUIRE_FALSE(objects.Find(GameId(1, 5)));
    REQUIRE(objects.CountIn(cell) == kMaxObjectsPerCell - 1);
    REQUIRE(objects.Add(GameId(1, 5), cell, 5));
    objects.Remove(100000);
    REQUIRE(objects.CountIn(other) == 0);
    objects.Remove(424242); // unknown handle
    REQUIRE(objects.Size() == kMaxObjectsPerCell);
}

TEST_CASE("Object assignment carries more than 255 objects and the remembered states", "[objects][encoding]")
{
    AssignObjectsRequest request;
    for (uint32_t i = 0; i < 300; ++i)
    {
        ObjectData object;
        object.Id = GameId(0, 0x1000 + i);
        object.CurrentLockData.IsLocked = (i % 2) != 0;
        request.Objects.push_back(object);
    }

    AssignObjectsResponse response;
    response.Objects = request.Objects;
    WorldObjectState state;
    state.Id = GameId(0, 0xC19C0);
    state.OpenState = WorldObjectRules::kOpen;
    state.HasLock = true;
    state.Lock.LockLevel = 255;
    response.States.push_back(state);

    TiltedPhoques::Buffer buffer(1 << 16);
    TiltedPhoques::Buffer::Writer writer(&buffer);
    request.Serialize(writer);
    response.Serialize(writer);

    TiltedPhoques::Buffer::Reader reader(&buffer);
    auto decodedRequest = ClientMessageFactory{}.Extract(reader);
    REQUIRE(decodedRequest);
    REQUIRE(decodedRequest->GetOpcode() == kAssignObjectsRequest);
    REQUIRE(static_cast<AssignObjectsRequest&>(*decodedRequest) == request);
    auto decodedResponse = ServerMessageFactory{}.Extract(reader);
    REQUIRE(decodedResponse);
    REQUIRE(decodedResponse->GetOpcode() == kAssignObjectsResponse);
    REQUIRE(static_cast<AssignObjectsResponse&>(*decodedResponse) == response);
}

TEST_CASE("A hostile object count is refused instead of allocated", "[objects][encoding]")
{
    TiltedPhoques::Buffer buffer(64);
    TiltedPhoques::Buffer::Writer writer(&buffer);
    TiltedPhoques::Serialization::WriteVarInt(writer, uint64_t(1) << 40);

    TiltedPhoques::Buffer::Reader reader(&buffer);
    AssignObjectsRequest request;
    request.DeserializeRaw(reader);
    REQUIRE(request.Objects.empty());
}
