#include <catch2/catch.hpp>

#include <Systems/NpcSyncRules.h>

using namespace NpcSyncRules;

TEST_CASE("Replayed actions don't bring a dead copy back", "[npc]")
{
    constexpr uint32_t cDead = 4u << 21;
    constexpr uint32_t cSitting = 3u << 14;

    SECTION("A dead copy keeps its life state and takes the rest")
    {
        const uint32_t cMerged = MergeReplayedState(cDead, cSitting, true);
        REQUIRE((cMerged & cLifeStateMask) == cDead);
        REQUIRE((cMerged & cSitting) == cSitting);
    }

    SECTION("A living copy takes everything")
    {
        REQUIRE(MergeReplayedState(0, cDead | cSitting, false) == (cDead | cSitting));
    }
}

TEST_CASE("Ambush state is read from what the script sets", "[npc]")
{
    REQUIRE(IsAmbushAwake(1.f, 2.f));
    REQUIRE_FALSE(IsAmbushAwake(0.f, 2.f));
    REQUIRE_FALSE(IsAmbushAwake(1.f, 1.f));
}

TEST_CASE("Remote copies claim actors nobody is running", "[npc]")
{
    double lastSnapshot = -1.0;
    double lastClaim = -cClaimRetryTime;

    SECTION("The first check only starts the wait")
    {
        REQUIRE_FALSE(ShouldClaimActor(true, false, 1000.0, lastSnapshot, lastClaim));
        REQUIRE(lastSnapshot == 1000.0);
    }

    SECTION("A silent owner loses the actor, and a claim isn't repeated right away")
    {
        lastSnapshot = 0.0;
        REQUIRE_FALSE(ShouldClaimActor(true, false, cOwnerSilentTime - 1.0, lastSnapshot, lastClaim));
        REQUIRE(ShouldClaimActor(true, false, cOwnerSilentTime, lastSnapshot, lastClaim));
        REQUIRE_FALSE(ShouldClaimActor(true, false, cOwnerSilentTime + 1.0, lastSnapshot, lastClaim));
        REQUIRE(ShouldClaimActor(true, false, cOwnerSilentTime + cClaimRetryTime, lastSnapshot, lastClaim));
    }

    SECTION("An actor that can't be taken over is left alone")
    {
        lastSnapshot = 0.0;
        REQUIRE_FALSE(ShouldClaimActor(false, false, cOwnerSilentTime * 2.0, lastSnapshot, lastClaim));
    }

    SECTION("An ambush that woke here is claimed even while the owner keeps sending")
    {
        lastSnapshot = 100.0;
        REQUIRE(ShouldClaimActor(true, true, 101.0, lastSnapshot, lastClaim));
    }
}

TEST_CASE("A held actor is released once per scene phase", "[npc]")
{
    double holdStart = -1.0;
    uint64_t releasedPhase = 0;

    REQUIRE_FALSE(ShouldReleaseFromScene(true, 7, 0.0, holdStart, releasedPhase));
    REQUIRE_FALSE(ShouldReleaseFromScene(true, 7, cSceneHoldTime - 1.0, holdStart, releasedPhase));
    REQUIRE(ShouldReleaseFromScene(true, 7, cSceneHoldTime, holdStart, releasedPhase));

    SECTION("It stays down if the package sits it back down")
    {
        REQUIRE_FALSE(ShouldReleaseFromScene(true, 7, cSceneHoldTime * 3.0, holdStart, releasedPhase));
    }

    SECTION("The next phase starts over")
    {
        REQUIRE_FALSE(ShouldReleaseFromScene(true, 8, cSceneHoldTime * 3.0, holdStart, releasedPhase));
        REQUIRE(ShouldReleaseFromScene(true, 8, cSceneHoldTime * 4.0, holdStart, releasedPhase));
    }

    SECTION("Getting up on its own cancels the wait")
    {
        REQUIRE_FALSE(ShouldReleaseFromScene(true, 8, 0.0, holdStart, releasedPhase));
        REQUIRE_FALSE(ShouldReleaseFromScene(false, 8, 1.0, holdStart, releasedPhase));
        REQUIRE(holdStart < 0.0);
    }
}

TEST_CASE("A voice line is only relayed once", "[npc]")
{
    REQUIRE(IsRepeatedLine("a.fuz", "a.fuz", 1000.0, 1017.0));
    REQUIRE_FALSE(IsRepeatedLine("a.fuz", "a.fuz", 1000.0, 1000.0 + cRepeatedLineTime));
    REQUIRE_FALSE(IsRepeatedLine("a.fuz", "b.fuz", 1000.0, 1017.0));
}
