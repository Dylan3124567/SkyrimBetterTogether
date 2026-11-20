#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>
#include <glm/glm.hpp>

#include <iterator>
#include <unordered_map>

#include <catch2/catch.hpp>

#include <Messages/ClientMessageFactory.h>
#include <Messages/ServerMessageFactory.h>
#include <Structs/DeathMemory.h>
#include <Systems/CombatRules.h>

using namespace TiltedPhoques;
using namespace CombatRules;

namespace
{
template <class T, class TFactory> UniquePtr<T> RoundTrip(const T& acMessage)
{
    Buffer buffer(1000);
    Buffer::Writer writer(&buffer);
    acMessage.Serialize(writer);

    Buffer::Reader reader(&buffer);
    const TFactory factory;
    auto pMessage = factory.Extract(reader);
    REQUIRE(pMessage);
    REQUIRE(pMessage->GetOpcode() == acMessage.GetOpcode());
    return CastUnique<T>(std::move(pMessage));
}
} // namespace

TEST_CASE("A copy's own health never decides its life", "[combat]")
{
    REQUIRE(ReactsToHealth(false, false));
    REQUIRE_FALSE(ReactsToHealth(true, false));
    // A downed player shows as down on the other clients through its respawn mode
    REQUIRE(ReactsToHealth(true, true));
}

TEST_CASE("The owner of a debtor only knocks it down when the brawling player's game would", "[combat]")
{
    constexpr float cPunch = -12.f;

    SECTION("TG01 brawl: essential only on the hitter's client, the relayed punch used to kill")
    {
        REQUIRE(ResolveRelayedBlow(cPunch, -2.f, true, false, false, true, false) == Blow::KnockDown);
    }

    SECTION("Essential on the owner's client only")
    {
        REQUIRE(ResolveRelayedBlow(cPunch, -2.f, true, false, false, false, true) == Blow::KnockDown);
    }

    SECTION("Neither game protects it")
    {
        REQUIRE(ResolveRelayedBlow(cPunch, -2.f, true, false, false, false, false) == Blow::Kill);
        REQUIRE(ResolveRelayedBlow(cPunch, 0.f, true, false, false, false, false) == Blow::Kill);
    }

    SECTION("Nothing to decide")
    {
        // Still standing
        REQUIRE(ResolveRelayedBlow(cPunch, 0.5f, true, false, false, false, false) == Blow::None);
        // A copy follows its owner instead
        REQUIRE(ResolveRelayedBlow(cPunch, -2.f, false, false, false, false, false) == Blow::None);
        // Players are never killed
        REQUIRE(ResolveRelayedBlow(cPunch, -2.f, true, true, false, false, false) == Blow::None);
        // Already down or dead
        REQUIRE(ResolveRelayedBlow(cPunch, -2.f, true, false, true, false, false) == Blow::None);
        // A heal
        REQUIRE(ResolveRelayedBlow(5.f, -2.f, true, false, false, false, false) == Blow::None);
    }
}

TEST_CASE("A copy goes down, gets up and dies with its owner's actor", "[combat]")
{
    // Upstream #645: the owner's essential NPC got up, the copy stayed on the floor
    REQUIRE(FollowOwner(false, true, false, false) == Follow::StandUp);
    REQUIRE(FollowOwner(false, false, false, true) == Follow::KnockDown);
    // Down only through replayed state, its scripts never heard of it, so down again properly
    REQUIRE(FollowOwner(false, true, false, true) == Follow::KnockDown);
    REQUIRE(FollowOwner(false, false, false, false) == Follow::None);

    REQUIRE(FollowOwner(false, false, true, false) == Follow::Kill);
    REQUIRE(FollowOwner(false, true, true, false) == Follow::Kill);
    REQUIRE(FollowOwner(true, false, true, false) == Follow::None);
    REQUIRE(FollowOwner(true, false, false, false) == Follow::Revive);
    REQUIRE(FollowOwner(true, false, false, true) == Follow::Revive);
}

TEST_CASE("Life state bits", "[combat]")
{
    REQUIRE(GetLifeState(kEssentialDown << cLifeStateShift | 0x3u) == kEssentialDown);
    REQUIRE(GetLifeState(kBleedout << cLifeStateShift | 1u << 25) == kBleedout);
    REQUIRE(cLifeStateMask == 0xFu << 21);
}

TEST_CASE("TG01 debtors killed by co-op damage come back", "[combat]")
{
    // Skyrim.esm TG01: KeeravaAlias 2, BersiAlias 3, HaelgaAlias 4, their TG01DeathCoverageScript pStage 35, 42, 47
    REQUIRE(cTG01 == 0x1F326);
    REQUIRE(std::size(cTG01Debtors) == 3);
    REQUIRE(cTG01Debtors[0].AliasId == 2);
    REQUIRE(cTG01Debtors[0].KilledByPlayerStage == 35);
    REQUIRE(cTG01Debtors[1].AliasId == 3);
    REQUIRE(cTG01Debtors[1].KilledByPlayerStage == 42);
    REQUIRE(cTG01Debtors[2].AliasId == 4);
    REQUIRE(cTG01Debtors[2].KilledByPlayerStage == 47);

    constexpr double cLongDead = cDebtorReviveGrace;

    // Dead in the save already, the user's case: no killer of this session, the grace period alone
    REQUIRE(RevivesDebtor(true, true, true, false, cLongDead, false));
    // Alive, quest over or not started, job not given (still essential then), a player kill the quest counted
    REQUIRE_FALSE(RevivesDebtor(false, true, true, false, cLongDead, false));
    REQUIRE_FALSE(RevivesDebtor(true, false, true, false, cLongDead, false));
    REQUIRE_FALSE(RevivesDebtor(true, true, false, false, cLongDead, false));
    REQUIRE_FALSE(RevivesDebtor(true, true, true, true, cLongDead, false));

    SECTION("A party member's kill, its stage still on its way from the killer's client")
    {
        REQUIRE_FALSE(RevivesDebtor(true, true, true, false, cDebtorReviveGrace - 1.0, false));
        REQUIRE_FALSE(RevivesDebtor(true, true, true, false, cDebtorReviveGrace * 10, true));
    }
}

TEST_CASE("A brawl knock out never becomes a death on the owner", "[combat]")
{
    // Punches thrown before the hitter's copy went down
    REQUIRE(IgnoresRelayedBlow(-8.f, true, true));
    // Up again: blows count, and a lethal one is resolved again (NonLethal from the brawler)
    REQUIRE_FALSE(IgnoresRelayedBlow(-8.f, true, false));
    // Down for another reason, the game decides
    REQUIRE_FALSE(IgnoresRelayedBlow(-8.f, false, true));
    // Healing always counts
    REQUIRE_FALSE(IgnoresRelayedBlow(5.f, true, true));
}

TEST_CASE("A brawl with a dead opponent ends", "[combat]")
{
    REQUIRE(EndsBrawl(true, false, true));
    REQUIRE_FALSE(EndsBrawl(true, false, false));
    REQUIRE_FALSE(EndsBrawl(true, true, true));
    REQUIRE_FALSE(EndsBrawl(false, false, true));
}

TEST_CASE("A remembered death doesn't kill an actor brought back by someone who saw it die", "[combat]")
{
    REQUIRE(DeathMemory::KeepsDeath(0, false));
    REQUIRE(DeathMemory::KeepsDeath(DeathMemory::cRecordHours - 1, false));
    REQUIRE_FALSE(DeathMemory::KeepsDeath(DeathMemory::cRecordHours, false));
    // The console's resurrect on Keerava, by a player whose game had her dead
    REQUIRE_FALSE(DeathMemory::KeepsDeath(1, true));
}

TEST_CASE("Only the owner kills, a party member near the actor may bring it back", "[combat]")
{
    using DeathMemory::AcceptsChange;

    // Owner: anything, at its epoch
    REQUIRE(AcceptsChange(true, true, true, false, false, true, true));
    REQUIRE(AcceptsChange(true, true, false, true, false, false, false));
    REQUIRE_FALSE(AcceptsChange(true, false, true, false, false, true, true));

    // Party member reviving a dead NPC near it
    REQUIRE(AcceptsChange(false, true, false, true, false, true, true));
    REQUIRE_FALSE(AcceptsChange(false, true, true, false, false, true, true));
    REQUIRE_FALSE(AcceptsChange(false, true, false, false, false, true, true));
    REQUIRE_FALSE(AcceptsChange(false, true, false, true, true, true, true));
    REQUIRE_FALSE(AcceptsChange(false, true, false, true, false, false, true));
    REQUIRE_FALSE(AcceptsChange(false, true, false, true, false, true, false));
    REQUIRE_FALSE(AcceptsChange(false, false, false, true, false, true, true));
}

TEST_CASE("The killer passed on with a death is one the sender may name (#840)", "[combat]")
{
    using DeathMemory::AcceptsKiller;

    REQUIRE(AcceptsKiller(true, false, false, true));
    REQUIRE(AcceptsKiller(false, true, false, true));
    // Erandur killed by the player who doesn't own him: the owner names that player, whose blow the server relayed last
    REQUIRE(AcceptsKiller(false, false, true, true));
    REQUIRE_FALSE(AcceptsKiller(false, false, false, true));
    REQUIRE_FALSE(AcceptsKiller(true, true, true, false));
}

TEST_CASE("Death witnesses are told apart by the server, not by name", "[combat]")
{
    using DeathMemory::WitnessId;

    // Two players with the same username, or one renamed by the character creation, are still who the server says
    REQUIRE(WitnessId(0, 5) != WitnessId(0, 6));
    // The Discord id survives reconnecting, the connection's id doesn't
    REQUIRE(WitnessId(123456789012345678ull, 5) == WitnessId(123456789012345678ull, 9));
    // The two kinds never meet
    REQUIRE(WitnessId(5, 7) != WitnessId(0, 5));
    REQUIRE(WitnessId(0, 5) >> 63 == 1);
}

TEST_CASE("Death witness records expire and stay bounded", "[combat]")
{
    struct Record
    {
        int64_t Hour{};
    };

    const int64_t cDay = 24;
    const int64_t cNow = DeathMemory::GameHour(201, 8, 17, 12);
    REQUIRE(DeathMemory::GameHour(201, 8, 18, 12) - cNow == cDay);

    SECTION("A record that can no longer keep a death goes")
    {
        std::unordered_map<uint32_t, Record> records{{1, {cNow - DeathMemory::cRecordHours}}, {2, {cNow - DeathMemory::cRecordHours + 1}}, {3, {cNow}}};
        DeathMemory::PruneWitnesses(records, cNow);
        REQUIRE(records.size() == 2);
        REQUIRE_FALSE(records.contains(1));
    }

    SECTION("Made-up deaths can't grow it past the bound, the oldest goes first")
    {
        std::unordered_map<uint32_t, Record> records;
        for (uint32_t i = 0; i < DeathMemory::cMaxWitnessRecords + 10; ++i)
            records[i] = {cNow - 100 + static_cast<int64_t>(i % 50)};
        records[0xFFFF0] = {cNow - 200};

        DeathMemory::PruneWitnesses(records, cNow);
        REQUIRE(records.size() == DeathMemory::cMaxWitnessRecords - 1);
        REQUIRE_FALSE(records.contains(0xFFFF0));
    }
}

TEST_CASE("Entity 0 is an actor, the first player's character often", "[combat]")
{
    // The ids are entity integers. Taken as "none", that player never counted as a hitter or a killer.
    REQUIRE(DeathMemory::cNoActor == 0xFFFFFFFFu);
    REQUIRE(NotifyHealthChangeBroadcast{}.HitterId == DeathMemory::cNoActor);
    REQUIRE(RequestDeathStateChange{}.KillerId == DeathMemory::cNoActor);
    REQUIRE(NotifyDeathStateChange{}.KillerId == DeathMemory::cNoActor);
    REQUIRE(DeathMemory::AcceptsKiller(false, false, true, true));

    NotifyDeathStateChange notify;
    notify.Id = 0x99;
    notify.OwnershipEpoch = 3;
    notify.IsDead = true;
    notify.KillerId = 0;
    REQUIRE(RoundTrip<NotifyDeathStateChange, ServerMessageFactory>(notify)->KillerId == 0);

    NotifyHealthChangeBroadcast health;
    health.Id = 0x99;
    health.DeltaHealth = -1.f;
    REQUIRE(RoundTrip<NotifyHealthChangeBroadcast, ServerMessageFactory>(health)->HitterId == DeathMemory::cNoActor);
}

TEST_CASE("Health and death messages carry the co-op combat state", "[combat]")
{
    {
        RequestHealthChangeBroadcast request;
        request.Id = 0x1234;
        request.DeltaHealth = -7.5f;
        request.NonLethal = true;
        REQUIRE(*RoundTrip<RequestHealthChangeBroadcast, ClientMessageFactory>(request) == request);
    }

    {
        NotifyHealthChangeBroadcast notify;
        notify.Id = 0x1234;
        notify.DeltaHealth = -7.5f;
        notify.NonLethal = true;
        notify.HitterId = 0x42;
        REQUIRE(*RoundTrip<NotifyHealthChangeBroadcast, ServerMessageFactory>(notify) == notify);
    }

    {
        RequestDeathStateChange request;
        request.Id = 0x99;
        request.OwnershipEpoch = 3;
        request.IsDead = false;
        request.IsDown = true;
        request.Health = -20.f;
        request.KillerId = 0x77;
        REQUIRE(*RoundTrip<RequestDeathStateChange, ClientMessageFactory>(request) == request);
    }

    {
        NotifyDeathStateChange notify;
        notify.Id = 0x99;
        notify.OwnershipEpoch = 3;
        notify.IsDead = true;
        notify.IsDown = false;
        notify.Health = 0.f;
        notify.KillerId = 0x77;
        REQUIRE(*RoundTrip<NotifyDeathStateChange, ServerMessageFactory>(notify) == notify);
    }
}
