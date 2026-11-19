#include <catch2/catch.hpp>

#include <limits>
#include <string>
#include <vector>

#include <Systems/LoadOrderRules.h>
#include <Systems/RespawnFlow.h>
#include <Systems/SyncRules.h>

namespace
{
using Phase = RespawnFlow::Phase;

// Plays frames at a fixed rate and remembers everything the flow asked for
struct RespawnRun
{
    RespawnFlow Flow;
    RespawnFlow::Frame Frame;
    uint32_t Seen = 0;
    int BeginDeaths = 0;
    int Respawns = 0;
    int GetUps = 0;
    int Knocks = 0;
    int FadeIns = 0;

    void Step(const double aSeconds = 0.1)
    {
        const double cEnd = Frame.Now + aSeconds;
        do
        {
            Frame.Now += 0.05;
            const uint32_t cCommands = Flow.Update(Frame);
            Seen |= cCommands;
            BeginDeaths += (cCommands & RespawnFlow::kBeginDeath) != 0;
            Respawns += (cCommands & RespawnFlow::kRespawn) != 0;
            GetUps += (cCommands & RespawnFlow::kGetUp) != 0;
            Knocks += (cCommands & RespawnFlow::kKnock) != 0;
            FadeIns += (cCommands & (RespawnFlow::kFadeIn | RespawnFlow::kFadeInNow)) != 0;
            Frame.HasBroken = false;
        } while (Frame.Now < cEnd);
    }

    void Die()
    {
        Frame.IsDown = Frame.IsStuck = true;
        Step();
        REQUIRE(Flow.GetPhase() == Phase::Dying);
    }

    void ReachRecovering()
    {
        Die();
        Step(RespawnFlow::cRespawnDelay + 0.1);
        REQUIRE(Flow.GetPhase() == Phase::Recovering);
    }

    void ReachProtected()
    {
        ReachRecovering();
        Frame.IsDown = Frame.IsStuck = false;

        // Stops right as protection starts, so tests can time what comes next
        const double cLimit = Frame.Now + RespawnFlow::cSettleDelay + 1.0;
        while (Flow.GetPhase() != Phase::Protected && Frame.Now < cLimit)
            Step(0.05);

        REQUIRE(Flow.GetPhase() == Phase::Protected);
    }
};
} // namespace

TEST_CASE("A mod pack gets its plugins switched back on in order", "[recovery.loadorder]")
{
    using Lines = std::vector<std::string>;

    const Lines cRequired{"# Pack", "*Unofficial Skyrim Special Edition Patch.esp", "SkyrimTogether.esp", "", "*Cloaks.esp"};

    // What the game leaves behind, with the marks stripped
    const Lines cStripped{"# Header", "SkyrimTogether.esp", "Cloaks.esp\r"};
    REQUIRE(LoadOrderRules::Merge(cRequired, cStripped) == Lines{"# Header", "*Unofficial Skyrim Special Edition Patch.esp", "*SkyrimTogether.esp", "*Cloaks.esp"});

    // The player's own plugins keep their state, and names don't depend on case
    const Lines cOwnMods{"*skyrimtogether.ESP", "*PlayerMod.esp", "DisabledMod.esp"};
    REQUIRE(LoadOrderRules::Merge(cRequired, cOwnMods) == Lines{"*Unofficial Skyrim Special Edition Patch.esp", "*SkyrimTogether.esp", "*Cloaks.esp", "*PlayerMod.esp", "DisabledMod.esp"});

    // A second start has nothing left to change
    const auto cOnce = LoadOrderRules::Merge(cRequired, cStripped);
    REQUIRE(LoadOrderRules::Merge(cRequired, cOnce) == cOnce);

    REQUIRE(LoadOrderRules::Merge(cRequired, {}) == Lines{"*Unofficial Skyrim Special Edition Patch.esp", "*SkyrimTogether.esp", "*Cloaks.esp"});
}

TEST_CASE("A death runs through to spawn protection and fades the screen back in", "[recovery.respawn]")
{
    RespawnRun run;
    run.ReachProtected();

    REQUIRE(run.BeginDeaths == 1);
    REQUIRE(run.Respawns == 1);
    REQUIRE(run.GetUps == 1);
    REQUIRE(run.Knocks == 0);
    REQUIRE(run.FadeIns == 2);
    REQUIRE(run.Flow.IsInvulnerable());
    REQUIRE_FALSE(run.Flow.IsScreenFaded());
}

TEST_CASE("A respawned player stands up from bleedout without being ragdolled", "[recovery.respawn]")
{
    RespawnRun run;
    run.ReachRecovering();
    run.Step(RespawnFlow::cSettleDelay + 0.1);
    REQUIRE(run.GetUps == 1);

    // Getting up from bleedout plays for a few seconds, none of the forced recoveries may cut into it
    run.Step(RespawnFlow::cForceGraphDelay - 1.0);
    run.Frame.IsDown = run.Frame.IsStuck = false;
    run.Step(1.0);

    REQUIRE(run.Flow.GetPhase() == Phase::Protected);
    REQUIRE(run.Knocks == 0);
    REQUIRE((run.Seen & (RespawnFlow::kForceGraph | RespawnFlow::kForceState)) == 0);
}

TEST_CASE("Being spawn killed can't start a second death on top of the respawn", "[recovery.respawn]")
{
    RespawnRun run;
    run.ReachRecovering();

    // Still down while the world settles, which used to count as a new death and cost the saved controls
    REQUIRE(run.Flow.IsInvulnerable());
    run.Step(RespawnFlow::cSettleDelay + 1.0);

    REQUIRE(run.BeginDeaths == 1);
    REQUIRE(run.Respawns == 1);
    REQUIRE(run.GetUps == 1);
    REQUIRE(run.Flow.GetPhase() == Phase::Recovering);
}

TEST_CASE("Getting up during the death fade still fades back in", "[recovery.respawn]")
{
    RespawnRun run;
    run.Die();
    run.Frame.IsDown = run.Frame.IsStuck = false;

    run.Step(RespawnFlow::cRespawnDelay + RespawnFlow::cSettleDelay + 1.0);

    REQUIRE(run.Respawns == 1);
    REQUIRE(run.FadeIns >= 1);
    REQUIRE(run.Flow.GetPhase() == Phase::Protected);
}

TEST_CASE("A loading screen holds back the getting up and fade in until the world is there", "[recovery.respawn]")
{
    RespawnRun run;
    run.ReachRecovering();
    run.Frame.IsDown = run.Frame.IsStuck = false;
    run.Frame.IsLoading = true;

    run.Step(30.0);
    REQUIRE(run.GetUps == 0);
    REQUIRE(run.FadeIns == 0);

    run.Frame.IsLoading = false;
    run.Step(RespawnFlow::cSettleDelay / 2);
    REQUIRE(run.GetUps == 0);

    run.Step(RespawnFlow::cSettleDelay);
    REQUIRE(run.GetUps == 1);
    REQUIRE(run.FadeIns >= 1);
}

TEST_CASE("A loading state that never ends can't leave the screen black", "[recovery.respawn]")
{
    RespawnRun run;
    run.Die();
    run.Frame.IsDown = run.Frame.IsStuck = false;
    run.Frame.IsLoading = true;

    run.Step(RespawnFlow::cRespawnDelay + RespawnFlow::cLoadingLimit + 0.5);
    REQUIRE(run.Respawns == 1);
    REQUIRE(run.FadeIns == 0);

    run.Step(RespawnFlow::cLoadingLimit + RespawnFlow::cSettleDelay + 1.0);
    REQUIRE(run.FadeIns >= 1);
    REQUIRE(run.Flow.GetPhase() != RespawnFlow::Phase::Dying);
}

TEST_CASE("A respawn waits for a loading screen that was already up", "[recovery.respawn]")
{
    RespawnRun run;
    run.Die();
    run.Frame.IsLoading = true;
    run.Step(RespawnFlow::cRespawnDelay + 5.0);
    REQUIRE(run.Respawns == 0);

    run.Frame.IsLoading = false;
    run.Step();
    REQUIRE(run.Respawns == 1);
}

TEST_CASE("A player who won't get up is forced up, and is never left behind", "[recovery.respawn]")
{
    RespawnRun run;
    run.ReachRecovering();

    run.Step(RespawnFlow::cSettleDelay + RespawnFlow::cRecoveryLimit + 1.0);

    REQUIRE((run.Seen & RespawnFlow::kForceGraph) != 0);
    REQUIRE((run.Seen & RespawnFlow::kForceState) != 0);
    // Ragdolling is the last thing tried, once
    REQUIRE(run.Knocks == 1);
    REQUIRE(run.Flow.GetPhase() == Phase::Protected);
    REQUIRE(run.FadeIns == 2);

    // A player stuck down for good must not loop through the black screen forever
    run.Step(5.0);
    REQUIRE(run.BeginDeaths == 1);
}

TEST_CASE("Spawn protection lasts until the player does something", "[recovery.protection]")
{
    RespawnRun run;
    run.ReachProtected();

    // Keys held from before the death don't count while the screen is still clearing
    run.Frame.HasMovementInput = true;
    run.Step(RespawnFlow::cInputGrace / 2);
    REQUIRE(run.Flow.GetPhase() == Phase::Protected);

    run.Step(RespawnFlow::cInputGrace);
    REQUIRE(run.Flow.GetPhase() == Phase::Alive);
    REQUIRE((run.Seen & RespawnFlow::kEndProtection) != 0);
    REQUIRE_FALSE(run.Flow.IsInvulnerable());
}

TEST_CASE("Attacking ends spawn protection at once, even during the grace period", "[recovery.protection]")
{
    RespawnRun run;
    run.ReachProtected();

    run.Frame.HasBroken = true;
    run.Step(0.05);

    REQUIRE(run.Flow.GetPhase() == Phase::Alive);
    REQUIRE_FALSE(run.Flow.IsInvulnerable());
}

TEST_CASE("Idling can't keep spawn protection up forever", "[recovery.protection]")
{
    RespawnRun run;
    run.ReachProtected();

    run.Step(RespawnFlow::cProtectionLimit - 2.0);
    REQUIRE(run.Flow.GetPhase() == Phase::Protected);

    run.Step(3.0);
    REQUIRE(run.Flow.GetPhase() == Phase::Alive);
}

TEST_CASE("A player can go down again once protection is over", "[recovery.protection]")
{
    RespawnRun run;
    run.ReachProtected();
    run.Frame.HasBroken = true;
    run.Step(0.05);
    REQUIRE(run.Flow.GetPhase() == Phase::Alive);

    run.ReachProtected();
    REQUIRE(run.BeginDeaths == 2);
    REQUIRE(run.Respawns == 2);
}

TEST_CASE("A script putting a protected player down starts over cleanly", "[recovery.protection]")
{
    RespawnRun run;
    run.ReachProtected();

    run.Frame.IsDown = run.Frame.IsStuck = true;
    run.Step(0.05);

    REQUIRE(run.Flow.GetPhase() == Phase::Dying);
    REQUIRE(run.BeginDeaths == 2);
    REQUIRE((run.Seen & RespawnFlow::kEndProtection) != 0);
}

TEST_CASE("Protection breaks are only remembered while protection is up", "[recovery.protection]")
{
    RespawnProtection::SetActive(false);

    RespawnProtection::Break();
    REQUIRE_FALSE(RespawnProtection::ConsumeBreak());

    RespawnProtection::SetActive(true);
    RespawnProtection::Break();
    REQUIRE(RespawnProtection::ConsumeBreak());
    REQUIRE_FALSE(RespawnProtection::ConsumeBreak());

    RespawnProtection::Break();
    RespawnProtection::SetActive(false);
    REQUIRE_FALSE(RespawnProtection::ConsumeBreak());
}

TEST_CASE("Only deliberate actions end spawn protection", "[recovery.protection]")
{
    for (uint32_t action : {0x13005u, 0x13004u, 0x50C96u, 0x13383u, 0x13008u, 0x13457u, 0x132AFu, 0x13006u, 0x13007u, 0x13009u})
        REQUIRE(SyncRules::IsDeliberateAction(action));

    // Turning, stopping and the actions the engine runs by itself while getting the player up
    for (uint32_t action : {0x959FDu, 0x959FCu, 0x959FEu, 0x959F9u, 0x13002u, 0x18BA8u, 0x13ECAu, 0x2ADF1u, 0x937F4u, 0x937F5u})
        REQUIRE_FALSE(SyncRules::IsDeliberateAction(action));
}

TEST_CASE("Dropped items are placed in front of the actor", "[recovery.drop]")
{
    const glm::vec3 cActor{1000.f, 2000.f, 300.f};

    const glm::vec3 cNorth = SyncRules::GetDropPosition(cActor, 0.f);
    REQUIRE(cNorth.x == Approx(1000.f));
    REQUIRE(cNorth.y == Approx(2000.f + SyncRules::cDropForwardDistance));
    REQUIRE(cNorth.z == Approx(300.f + SyncRules::cDropHeight));

    const glm::vec3 cEast = SyncRules::GetDropPosition(cActor, 1.57079632f);
    REQUIRE(cEast.x == Approx(1000.f + SyncRules::cDropForwardDistance));
    REQUIRE(cEast.y == Approx(2000.f));
}

TEST_CASE("Items the engine threw away are told apart from items that rolled", "[recovery.drop]")
{
    const glm::vec3 cExpected{15000.f, -42000.f, 900.f};

    REQUIRE_FALSE(SyncRules::IsDropMisplaced(cExpected, cExpected));
    REQUIRE_FALSE(SyncRules::IsDropMisplaced({cExpected.x + 200.f, cExpected.y, cExpected.z - 150.f}, cExpected));

    // The engine's stale world bound case, twice the distance from the origin
    REQUIRE(SyncRules::IsDropMisplaced(cExpected * 2.f, cExpected));
    // Near the coordinate origin of the cell
    REQUIRE(SyncRules::IsDropMisplaced({0.f, 0.f, 0.f}, cExpected));
    // Falling out of the world
    REQUIRE(SyncRules::IsDropMisplaced({cExpected.x, cExpected.y, cExpected.z - 2000.f}, cExpected));

    constexpr float cNan = std::numeric_limits<float>::quiet_NaN();
    REQUIRE(SyncRules::IsDropMisplaced({cNan, 0.f, 0.f}, cExpected));
}

TEST_CASE("A remote NPC only stands up after its owner has been up for a while", "[recovery.furniture]")
{
    double mismatchStart = -1.0;

    // In step with the owner
    REQUIRE_FALSE(SyncRules::ShouldReleaseFurniture(true, true, 0.0, mismatchStart));
    REQUIRE_FALSE(SyncRules::ShouldReleaseFurniture(false, false, 100.0, mismatchStart));
    REQUIRE_FALSE(SyncRules::ShouldReleaseFurniture(false, true, 200.0, mismatchStart));
    REQUIRE(mismatchStart < 0.0);

    // The copy is sitting and the owner isn't, but this could just be the action and the snapshot arriving apart
    REQUIRE_FALSE(SyncRules::ShouldReleaseFurniture(true, false, 1000.0, mismatchStart));
    REQUIRE_FALSE(SyncRules::ShouldReleaseFurniture(true, false, 1999.0, mismatchStart));

    // The owner confirms it sat down, the wait starts over
    REQUIRE_FALSE(SyncRules::ShouldReleaseFurniture(true, true, 1999.5, mismatchStart));
    REQUIRE(mismatchStart < 0.0);
    REQUIRE_FALSE(SyncRules::ShouldReleaseFurniture(true, false, 2000.0, mismatchStart));
    REQUIRE_FALSE(SyncRules::ShouldReleaseFurniture(true, false, 2999.0, mismatchStart));

    REQUIRE(SyncRules::ShouldReleaseFurniture(true, false, 3000.0, mismatchStart));
    REQUIRE(mismatchStart < 0.0);
}

TEST_CASE("Thorald is only freed between his rescue and reaching safety", "[recovery.furniture]")
{
    REQUIRE_FALSE(SyncRules::ShouldFreeThorald(false, false));
    REQUIRE(SyncRules::ShouldFreeThorald(true, false));
    REQUIRE_FALSE(SyncRules::ShouldFreeThorald(true, true));
    REQUIRE_FALSE(SyncRules::ShouldFreeThorald(false, true));
}

TEST_CASE("A copy left in a furniture pose is sent out once its owner is out of furniture", "[recovery.furniture]")
{
    bool pending = true;
    double outSince = -1.0;

    // A new copy whose owner stands: sent out of whatever it replayed, once
    REQUIRE_FALSE(SyncRules::ShouldLeaveFurniture(false, 0.0, pending, outSince));
    REQUIRE(SyncRules::ShouldLeaveFurniture(false, SyncRules::cFurnitureMismatchTime, pending, outSince));
    REQUIRE_FALSE(SyncRules::ShouldLeaveFurniture(false, SyncRules::cFurnitureMismatchTime * 5, pending, outSince));

    // The owner sits, then gets up: the copy follows a moment later, even with no state of its own to show it
    REQUIRE_FALSE(SyncRules::ShouldLeaveFurniture(true, 10000.0, pending, outSince));
    REQUIRE_FALSE(SyncRules::ShouldLeaveFurniture(false, 20000.0, pending, outSince));
    REQUIRE(SyncRules::ShouldLeaveFurniture(false, 20000.0 + SyncRules::cFurnitureMismatchTime, pending, outSince));
    REQUIRE_FALSE(SyncRules::ShouldLeaveFurniture(false, 30000.0, pending, outSince));
}