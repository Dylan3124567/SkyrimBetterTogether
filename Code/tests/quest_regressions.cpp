#include <catch2/catch.hpp>

#include <Systems/QuestStall.h>
#include <Structs/QuestSharing.h>

TEST_CASE("A scene that keeps moving never releases held stages")
{
    QuestStall stall;
    for (uint64_t phase = 0; phase < 100; ++phase)
    {
        for (int frame = 0; frame < 30 * 30; ++frame)
            REQUIRE_FALSE(stall.Update(phase, 1.0 / 30.0));
    }
}

TEST_CASE("A scene stuck in one phase releases stages one at a time")
{
    QuestStall stall;
    double elapsed = 0.0;
    while (!stall.Update(7, 0.5))
    {
        elapsed += 0.5;
        REQUIRE(elapsed < QuestStall::cStallTime + 1.0);
    }
    REQUIRE(elapsed >= QuestStall::cStallTime - 1.0);

    stall.OnReleased();
    REQUIRE_FALSE(stall.Update(7, QuestStall::cReleaseGap - 1.0));
    REQUIRE(stall.Update(7, 1.0));
}

TEST_CASE("Progress after a release starts the wait again")
{
    QuestStall stall;
    stall.Update(1, 0.0);
    REQUIRE(stall.Update(1, QuestStall::cStallTime));
    stall.OnReleased();

    REQUIRE_FALSE(stall.Update(2, QuestStall::cStallTime));
    REQUIRE_FALSE(stall.Update(2, QuestStall::cStallTime - 1.0));
    REQUIRE(stall.Update(2, 1.0));
}

TEST_CASE("Every quest with a journal entry is shared, background quests are not", "[quest.sharing]")
{
    // TESQuest::Type: None, Main, Mages Guild, Thieves Guild, Dark Brotherhood, Companions, Miscellaneous, Daedric, Side,
    // Civil War, Dawnguard, Dragonborn
    REQUIRE_FALSE(QuestSharing::IsShared(0));
    for (uint8_t type = 1; type <= 11; ++type)
        REQUIRE(QuestSharing::IsShared(type));
}

TEST_CASE("A quest's updates are relayed once, whoever reports them", "[quest.sharing]")
{
    QuestSharing::Run run;
    REQUIRE(run.Relay(QuestSharing::kStarted, 0));
    REQUIRE(run.Relay(QuestSharing::kStageUpdate, 10));

    // The other member's own scripts reach the same start and stage
    REQUIRE_FALSE(run.Relay(QuestSharing::kStarted, 0));
    REQUIRE_FALSE(run.Relay(QuestSharing::kStageUpdate, 10));
    REQUIRE(run.Relay(QuestSharing::kStageUpdate, 20));

    REQUIRE(run.Relay(QuestSharing::kStopped, 20));
    REQUIRE_FALSE(run.Relay(QuestSharing::kStopped, 20));
}

TEST_CASE("A repeatable quest is shared again from its start", "[quest.sharing]")
{
    QuestSharing::Run run;
    REQUIRE(run.Relay(QuestSharing::kStarted, 0));
    REQUIRE(run.Relay(QuestSharing::kStageUpdate, 10));
    REQUIRE(run.Relay(QuestSharing::kStopped, 10));

    // Reported at the stage it stopped at, its startup stage event still follows
    REQUIRE(run.Relay(QuestSharing::kStarted, 10));
    REQUIRE(run.Relay(QuestSharing::kStageUpdate, 10));
}

TEST_CASE("A stage of a quest started before the party still reaches the party", "[quest.sharing]")
{
    // No start was relayed: the quest began before the party, or its stop was missed and it runs again
    QuestSharing::Run run;
    REQUIRE(run.Relay(QuestSharing::kStageUpdate, 30));
    REQUIRE_FALSE(run.Relay(QuestSharing::kStageUpdate, 30));
    REQUIRE_FALSE(run.Relay(QuestSharing::kStarted, 30));

    QuestSharing::Run stopped;
    REQUIRE(stopped.Relay(QuestSharing::kStopped, 30));
    REQUIRE(stopped.Relay(QuestSharing::kStageUpdate, 10));
}