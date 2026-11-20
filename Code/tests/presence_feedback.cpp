#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>
#include <glm/glm.hpp>
#include <Structs/ActionEvent.h>
#include <Structs/ActionReplayChain.h>
#include <Structs/Movement.h>
#include <Messages/RequestOwnershipClaim.h>
#include <catch2/catch.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include <Systems/CaravanRules.h>
#include <Systems/NpcSyncRules.h>
#include <Systems/SyncRules.h>

namespace
{
// Skyrim's sit and sleep states, bits 14-17 of the actor state
constexpr uint32_t cWantToSit = 1;
constexpr uint32_t cIsSitting = 3;
constexpr uint32_t cWantToStand = 4;
constexpr uint32_t cIsSleeping = 7;
constexpr uint32_t cWantToWake = 8;

ActionEvent MakeAction(const char* apEventName, const uint32_t aFurnitureState)
{
    ActionEvent action;
    action.EventName = TiltedPhoques::String(apEventName);
    // Weapon drawn and running bits around the field must not count
    action.State1 = (aFurnitureState << 14) | 0x3FFFu | (1u << 20);
    return action;
}
} // namespace

TEST_CASE("A remote actor never has two copies", "[presence]")
{
    SyncRules::RemoteCopies copies;

    REQUIRE(copies.Add(7, 0xFF000801) == 0);

    SECTION("A new copy of the same actor hands over the old one to be deleted")
    {
        REQUIRE(copies.Add(7, 0xFF000802) == 0xFF000801);
        REQUIRE(copies.Find(7) == 0xFF000802);
    }

    SECTION("Recording the same copy again replaces nothing")
    {
        REQUIRE(copies.Add(7, 0xFF000801) == 0);
        REQUIRE(copies.Find(7) == 0xFF000801);
    }

    SECTION("A form id the game gave out again leaves the stale entry behind")
    {
        // The first copy was deleted by the game, its id now belongs to another actor's copy
        REQUIRE(copies.Add(9, 0xFF000801) == 0);
        REQUIRE(copies.Find(7) == 0);
        REQUIRE(copies.Remove(7) == 0);
        REQUIRE(copies.Find(9) == 0xFF000801);
    }

    SECTION("A copy this client takes over stops being one")
    {
        REQUIRE(copies.Remove(7) == 0xFF000801);
        REQUIRE(copies.Find(7) == 0);
        REQUIRE(copies.TakeAll().empty());
    }
}

TEST_CASE("Copies left without an entity are handed out for deletion", "[presence]")
{
    SyncRules::RemoteCopies copies;
    copies.Add(1, 0xFF000A01);
    copies.Add(2, 0xFF000A02);
    copies.Add(3, 0xFF000A03);

    // The remove for actor 2 came while its copy wasn't discovered yet, its entity is gone but the copy still stands
    const auto orphans = copies.TakeOrphans([](const uint32_t aServerId) { return aServerId != 2; });
    REQUIRE(orphans == std::vector<uint32_t>{0xFF000A02});
    REQUIRE(copies.Find(2) == 0);
    REQUIRE(copies.Find(1) == 0xFF000A01);

    // Disconnecting deletes every copy, discovered or not
    auto all = copies.TakeAll();
    std::sort(all.begin(), all.end());
    const std::vector<uint32_t> cExpected{0xFF000A01, 0xFF000A03};
    REQUIRE(all == cExpected);
    REQUIRE(copies.TakeAll().empty());
}

TEST_CASE("Undiscovered copies slide instead of being moved every frame", "[presence]")
{
    REQUIRE_FALSE(SyncRules::ShouldMoveCopy(true, 0.f));
    REQUIRE_FALSE(SyncRules::ShouldMoveCopy(true, SyncRules::cMaxSlideDistance - 1.f));
    REQUIRE(SyncRules::ShouldMoveCopy(true, SyncRules::cMaxSlideDistance));
    REQUIRE(SyncRules::ShouldMoveCopy(false, 0.f));
    REQUIRE(SyncRules::ShouldMoveCopy(true, std::numeric_limits<float>::quiet_NaN()));
}

TEST_CASE("Furniture actions are told apart by the owner's sit state", "[presence][furniture]")
{
    REQUIRE_FALSE(MakeAction("moveStart", 0).IsFurnitureAction());
    REQUIRE_FALSE(MakeAction("moveStart", 0).IsSeatingAction());

    for (const uint32_t cState : {1u, 2u, 3u, 5u, 6u, 7u})
    {
        const auto cAction = MakeAction("IdleChairEnterStart", cState);
        REQUIRE(cAction.IsFurnitureAction());
        REQUIRE(cAction.IsSeatingAction());
    }

    // Getting up has to play even when sitting down was skipped, it can only bring the copy back to its feet
    for (const uint32_t cState : {cWantToStand, cWantToWake})
    {
        const auto cAction = MakeAction("IdleChairExitStart", cState);
        REQUIRE(cAction.IsFurnitureAction());
        REQUIRE_FALSE(cAction.IsSeatingAction());
    }
}

TEST_CASE("A copy only sits down while its owner is in furniture this client has", "[presence][furniture]")
{
    // The spawn's replay plays before any snapshot, the server already left out furniture an owner had left
    REQUIRE(SyncRules::ShouldPlaySeatingAction(false, false));
    REQUIRE(SyncRules::ShouldPlaySeatingAction(true, true));
    // Owner already walking, or sitting in a bedroll that is disabled here
    REQUIRE_FALSE(SyncRules::ShouldPlaySeatingAction(true, false));
}

TEST_CASE("An owner sitting in furniture missing here sends its copy out once", "[presence][furniture]")
{
    bool pending = true;
    double outSince = -1.0;

    // The caller passes the owner as out of furniture when its furniture is missing here
    REQUIRE_FALSE(SyncRules::ShouldLeaveFurniture(false, 0.0, pending, outSince));
    REQUIRE(SyncRules::ShouldLeaveFurniture(false, SyncRules::cFurnitureMismatchTime, pending, outSince));
    REQUIRE_FALSE(SyncRules::ShouldLeaveFurniture(false, SyncRules::cFurnitureMismatchTime * 3, pending, outSince));

    // A seating action played after that makes the copy pending again
    pending = true;
    REQUIRE(SyncRules::ShouldLeaveFurniture(false, SyncRules::cFurnitureMismatchTime * 4, pending, outSince));
}

TEST_CASE("A spawn replay leaves out furniture its owner is no longer in", "[presence][furniture]")
{
    ActionReplayChain chain;
    chain.Actions.push_back(MakeAction("IdleChairEnterInstant", cWantToSit));
    chain.Actions.push_back(MakeAction("idleChairVar3", cIsSitting));
    // Not recognized as an exit, so the cache kept everything before it
    chain.Actions.push_back(MakeAction("IdleChairGetUpQuick", cWantToStand));
    chain.Actions.push_back(MakeAction("moveStart", 0));
    chain.Actions.push_back(MakeAction("IdleBedEnterInstant", cIsSleeping));

    chain.DropFurnitureActions();

    REQUIRE(chain.Actions.size() == 1);
    REQUIRE(chain.Actions[0].EventName == "moveStart");
    REQUIRE(chain.ResetAnimationGraph);

    ActionReplayChain walking;
    walking.Actions.push_back(MakeAction("moveStart", 0));
    walking.DropFurnitureActions();
    REQUIRE(walking.Actions.size() == 1);
    REQUIRE_FALSE(walking.ResetAnimationGraph);
}

TEST_CASE("Snapshots carry the furniture only while held", "[presence][furniture][encoding.sync]")
{
    for (const bool cHeld : {false, true})
    {
        Movement source;
        source.OwnershipEpoch = 3;
        source.IsHeldInPlace = cHeld;
        source.FurnitureId = cHeld ? GameId(2, 0x10F5C3) : GameId{};

        TiltedPhoques::Buffer buffer(1024);
        TiltedPhoques::Buffer::Writer writer(&buffer);
        source.Serialize(writer);
        TiltedPhoques::Serialization::WriteVarInt(writer, 0x123456);

        Movement destination;
        destination.FurnitureId = GameId(9, 9);
        TiltedPhoques::Buffer::Reader reader(&buffer);
        destination.Deserialize(reader);

        REQUIRE(destination == source);
        REQUIRE(destination.FurnitureId == source.FurnitureId);
        REQUIRE(TiltedPhoques::Serialization::ReadVarInt(reader) == 0x123456);
    }
}

TEST_CASE("An ownership claim says whether its player is talking to the actor", "[presence][dialogue][encoding]")
{
    for (const bool cForDialogue : {false, true})
    {
        RequestOwnershipClaim sendMessage, recvMessage;
        sendMessage.ServerId = 0x1234;
        sendMessage.ExpectedOwnershipEpoch = 7;
        sendMessage.ForDialogue = cForDialogue;

        TiltedPhoques::Buffer buffer(1000);
        TiltedPhoques::Buffer::Writer writer(&buffer);
        sendMessage.Serialize(writer);

        TiltedPhoques::Buffer::Reader reader(&buffer);
        uint64_t opcode = 0;
        reader.ReadBits(opcode, sizeof(ClientOpcode) * 8);
        recvMessage.DeserializeRaw(reader);

        REQUIRE(recvMessage == sendMessage);
        REQUIRE(recvMessage.ForDialogue == cForDialogue);
    }
}

TEST_CASE("Another player's follower never becomes ours", "[presence][followers]")
{
    // CurrentFollowerFaction, PlayerFollowerFaction, CurrentHireling, PlayerMarriedFaction
    for (const uint32_t cFaction : {0x5C84Eu, 0x84D1Bu, 0xBD738u, 0xC6472u})
        REQUIRE(NpcSyncRules::IsPlayerRelationshipFaction(cFaction));

    // CrimeFactionWhiterun and BanditFaction still come from the owner
    REQUIRE_FALSE(NpcSyncRules::IsPlayerRelationshipFaction(0x267EA));
    REQUIRE_FALSE(NpcSyncRules::IsPlayerRelationshipFaction(0x1BCC0));
}

TEST_CASE("The caravan table matches Skyrim.esm", "[presence][caravan]")
{
    using namespace CaravanRules;

    // Quest Caravans and its script's properties, aliases LeaderA/B/C, CaravanXCampNSit package targets
    REQUIRE(kCaravansQuest == 0x3ACDB);
    REQUIRE(kTamriel == 0x3C);

    const uint32_t cAliases[] = {15, 16, 18};
    const uint32_t cLocations[] = {0x72FFB, 0x73C98, 0x73CAE};
    const uint32_t cCamped[] = {0x73006, 0x73C96, 0x73C97};
    const uint32_t cMarkers[][2] = {{0x10372F, 0x1030E2}, {0x104AD6, 0x103774}, {0x10311B, 0x10332A}};
    const uint32_t cSeats[][2] = {{0x103717, 0x1030A0}, {0x104AE0, 0x10376D}, {0x1030F2, 0x10334C}};
    // The marker's cell in Tamriel, every tent of the camp is in it or one next to it
    const int32_t cMarkerCells[][2][2] = {{{3, -3}, {-40, 1}}, {{-17, 25}, {33, 6}}, {{42, -22}, {6, 24}}};

    for (size_t i = 0; i < kCaravans.size(); ++i)
    {
        REQUIRE(kCaravans[i].LeaderAlias == cAliases[i]);
        REQUIRE(kCaravans[i].LocationGlobal == cLocations[i]);
        REQUIRE(kCaravans[i].CampedGlobal == cCamped[i]);
        for (size_t camp = 0; camp < 2; ++camp)
        {
            REQUIRE(kCaravans[i].Camps[camp].EnableMarker == cMarkers[i][camp]);
            REQUIRE(kCaravans[i].Camps[camp].Seat == cSeats[i][camp]);
            REQUIRE(CellOf(kCaravans[i].Camps[camp].MarkerX) == cMarkerCells[i][camp][0]);
            REQUIRE(CellOf(kCaravans[i].Camps[camp].MarkerY) == cMarkerCells[i][camp][1]);
        }
    }

    // The globals of a caravan camped at its first and second camp, as CaravanXCamp1Sit and CaravanXCamp2Sit require them
    REQUIRE(LocationValue(0) == 0.f);
    REQUIRE(LocationValue(1) == 1.f);
    REQUIRE(kCampedValue == 1.f);
}

TEST_CASE("A caravan leader only counts as camped once it stays at its camp seat", "[presence][caravan]")
{
    using namespace CaravanRules;

    const Caravan& cRisaad = kCaravans[0];
    const Camp& cWhiterun = cRisaad.Camps[0];

    // Sitting at the Whiterun camp's seat, and standing next to it
    REQUIRE(FindCamp(cRisaad, cWhiterun.SeatX, cWhiterun.SeatY) == 0);
    REQUIRE(FindCamp(cRisaad, cWhiterun.SeatX + 300.f, cWhiterun.SeatY - 300.f) == 0);
    REQUIRE(FindCamp(cRisaad, cRisaad.Camps[1].SeatX, cRisaad.Camps[1].SeatY) == 1);
    // On the road between the camps, and the other caravans' camps never count
    REQUIRE(FindCamp(cRisaad, -73000.f, -2000.f) == -1);
    REQUIRE(FindCamp(cRisaad, kCaravans[1].Camps[0].SeatX, kCaravans[1].Camps[0].SeatY) == -1);

    CampTracker tracker;
    for (uint32_t check = 1; check < kSettleChecks; ++check)
        REQUIRE(tracker.Update(0) == -1);
    REQUIRE(tracker.Update(0) == 0);
    REQUIRE(tracker.Update(0) == 0);

    // Leaving starts over, a caravan travelling past its other camp doesn't settle there
    REQUIRE(tracker.Update(-1) == -1);
    REQUIRE(tracker.Update(1) == -1);
    tracker.Reset();
    REQUIRE(tracker.Camp == -1);
    REQUIRE(tracker.Update(1) == -1);
}

TEST_CASE("A caravan camp only goes away where the player can't see it", "[presence][caravan]")
{
    using namespace CaravanRules;

    const Camp& cWhiterun = kCaravans[0].Camps[0];

    // Standing in the camp, two cells away with the camp at the edge of the loaded grid, and one cell further out
    REQUIRE_FALSE(CanHideCamp(true, 3, -3, cWhiterun));
    REQUIRE_FALSE(CanHideCamp(true, 6, -3, cWhiterun));
    REQUIRE(CanHideCamp(true, 7, -3, cWhiterun));
    REQUIRE(CanHideCamp(true, 3, 1, cWhiterun));
    // Inside, or in a city's own world space, none of Tamriel is attached
    REQUIRE(CanHideCamp(false, 3, -3, cWhiterun));
}
