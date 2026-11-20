#include <catch2/catch.hpp>

#include <Structs/QuestRecovery.h>
#include <Structs/QuestSharing.h>

#include <algorithm>
#include <set>

// TESQuest runtime flags, as the game sets them
static constexpr uint16_t kEnabled = 1 << 0;
static constexpr uint16_t kActive = 1 << 11;
// SetFailed (24988) ORs 0x42 into the flags
static constexpr uint16_t kFailedQuest = QuestSharing::kCompletedFlag | QuestSharing::kFailedFlag;

TEST_CASE("A stopped quest is shared with how it ended", "[quest.feedback]")
{
    REQUIRE(QuestSharing::StopStatus(0) == QuestSharing::kStopped);
    REQUIRE(QuestSharing::StopStatus(kActive) == QuestSharing::kStopped);
    REQUIRE(QuestSharing::StopStatus(QuestSharing::kCompletedFlag) == QuestSharing::kCompleted);
    // The House of Horrors' stages 500 and 550 have Fail Quest log entries
    REQUIRE(QuestSharing::StopStatus(kFailedQuest) == QuestSharing::kFailed);
    REQUIRE(QuestSharing::StopStatus(kFailedQuest | kEnabled | kActive) == QuestSharing::kFailed);
}

TEST_CASE("Only the three stop statuses are stops, and nothing past them is valid", "[quest.feedback]")
{
    REQUIRE_FALSE(QuestSharing::IsStop(QuestSharing::kStageUpdate));
    REQUIRE_FALSE(QuestSharing::IsStop(QuestSharing::kStarted));
    REQUIRE(QuestSharing::IsStop(QuestSharing::kStopped));
    REQUIRE(QuestSharing::IsStop(QuestSharing::kCompleted));
    REQUIRE(QuestSharing::IsStop(QuestSharing::kFailed));

    REQUIRE(QuestSharing::IsValid(QuestSharing::kFailed));
    for (unsigned status = QuestSharing::kFailed + 1; status <= 0xFF; ++status)
    {
        REQUIRE_FALSE(QuestSharing::IsValid(static_cast<uint8_t>(status)));
        REQUIRE_FALSE(QuestSharing::IsStop(static_cast<uint8_t>(status)));
    }
}

TEST_CASE("A member's failure fails the quest here unless it already ended", "[quest.feedback]")
{
    REQUIRE(QuestSharing::NeedsOutcome(QuestSharing::kFailed, kEnabled));
    REQUIRE(QuestSharing::NeedsOutcome(QuestSharing::kCompleted, kEnabled | kActive));
    REQUIRE_FALSE(QuestSharing::NeedsOutcome(QuestSharing::kStopped, kEnabled));

    // The fail stage's own log entry already failed it here
    REQUIRE_FALSE(QuestSharing::NeedsOutcome(QuestSharing::kFailed, kFailedQuest));
    // The game keeps the first way a quest ended, so neither outcome overrides the other
    REQUIRE_FALSE(QuestSharing::NeedsOutcome(QuestSharing::kFailed, QuestSharing::kCompletedFlag));
    REQUIRE_FALSE(QuestSharing::NeedsOutcome(QuestSharing::kCompleted, kFailedQuest));
}

TEST_CASE("A failure is relayed once like any stop, and the next run is shared again", "[quest.feedback]")
{
    QuestSharing::Run run;
    REQUIRE(run.Relay(QuestSharing::kStarted, 0));
    REQUIRE(run.Relay(QuestSharing::kStageUpdate, 500));
    REQUIRE(run.Relay(QuestSharing::kFailed, 500));

    // The other members stop their own copy in turn, however theirs ended
    REQUIRE_FALSE(run.Relay(QuestSharing::kStopped, 500));
    REQUIRE_FALSE(run.Relay(QuestSharing::kFailed, 500));
    REQUIRE_FALSE(run.Relay(QuestSharing::kCompleted, 500));

    REQUIRE(run.Relay(QuestSharing::kStarted, 500));
    REQUIRE(run.Relay(QuestSharing::kStageUpdate, 10));
    REQUIRE(run.Relay(QuestSharing::kCompleted, 200));
}

TEST_CASE("Unknown statuses are never relayed", "[quest.feedback]")
{
    QuestSharing::Run run;
    REQUIRE_FALSE(run.Relay(QuestSharing::kFailed + 1, 0));
    REQUIRE_FALSE(run.Relay(0xFF, 0));
    REQUIRE(run.Relay(QuestSharing::kStarted, 0));
}

TEST_CASE("A member's stop waits for this client's fragments of the stages before it", "[quest.feedback]")
{
    QuestSharing::PendingStop stop{QuestSharing::kFailed};
    REQUIRE_FALSE(stop.Update(true, 1.0 / 30.0));
    REQUIRE_FALSE(stop.Update(true, 1.0));
    // The fail stage's fragment ran, its own Stop() usually ended the quest already
    REQUIRE(stop.Update(false, 1.0 / 30.0));

    QuestSharing::PendingStop nothingToWaitFor{QuestSharing::kStopped};
    REQUIRE(nothingToWaitFor.Update(false, 0.0));
}

TEST_CASE("A member's stop never waits longer than kStopWait", "[quest.feedback]")
{
    QuestSharing::PendingStop stop{QuestSharing::kStopped};
    double waited = 0.0;
    while (!stop.Update(true, 0.5))
    {
        waited += 0.5;
        REQUIRE(waited < QuestSharing::kStopWait);
    }
    REQUIRE(waited + 0.5 >= QuestSharing::kStopWait);
}

TEST_CASE("Applied member stages count as running until one of their fragments reports", "[quest.feedback]")
{
    QuestSharing::AppliedStages applied;
    REQUIRE_FALSE(applied.Running());

    applied.Add(500);
    applied.Add(500);
    REQUIRE(applied.Running());

    // A stage reached by this client's own scripts isn't waited for
    applied.Done(80);
    REQUIRE(applied.Stages.size() == 2);

    applied.Done(500);
    REQUIRE(applied.Running());
    applied.Done(500);
    REQUIRE_FALSE(applied.Running());
}

TEST_CASE("Log entries that never report stop counting kStopWait after the last stage", "[quest.feedback]")
{
    // Stage 200 of Waking Nightmare has three conditional log entries, only one of which runs
    QuestSharing::AppliedStages applied;
    applied.Add(200);
    applied.Age += QuestSharing::kStopWait - 1.0;
    REQUIRE(applied.Running());

    // A newer stage starts the wait again
    applied.Add(199);
    REQUIRE(applied.Running());
    applied.Age += QuestSharing::kStopWait;
    REQUIRE_FALSE(applied.Running());
}

// QuestRecovery: The House of Horrors failed while the player was inside Logrolf's house

static const QuestRecovery::Rule& HouseOfHorrors()
{
    const auto it = std::find_if(QuestRecovery::kRules.begin(), QuestRecovery::kRules.end(), [](const QuestRecovery::Rule& aRule) { return aRule.Quest == 0x22F08; });
    REQUIRE(it != QuestRecovery::kRules.end());
    return *it;
}

TEST_CASE("The House of Horrors unlocks Logrolf's house and ends the haunting as its stage 80 does", "[quest.feedback]")
{
    const QuestRecovery::Rule& rule = HouseOfHorrors();
    // Stages 15/20 lock MainDoor (alias 24 -> MarkarthAbanonedHouseDoorREF), 80 unlocks it and ends the haunting
    REQUIRE(rule.SinceStage == 15);
    REQUIRE(rule.UntilStage == 80);
    REQUIRE(rule.Door == 0x18BF9);
    REQUIRE(rule.Controller == 0x2BB38);
    REQUIRE(rule.ControllerStartStage == 0);
    REQUIRE(rule.ControllerStages == std::array<uint16_t, 2>{80, 90});
}

TEST_CASE("Only a quest stopped inside the sequence is recovered", "[quest.feedback]")
{
    // Failed inside the house: stage 15 done, 80 never reached
    REQUIRE(QuestRecovery::Applies(true, true, false));
    // Still running, a member's stop may be on its way
    REQUIRE_FALSE(QuestRecovery::Applies(false, true, false));
    // Failed before entering: the fail stage locks the player out, as alone
    REQUIRE_FALSE(QuestRecovery::Applies(true, false, false));
    // Stage 80 ended the haunting and unlocked the door itself
    REQUIRE_FALSE(QuestRecovery::Applies(true, true, true));
}

TEST_CASE("The haunting's stages are set one at a time, the loop's stage 80 pass first", "[quest.feedback]")
{
    const QuestRecovery::Rule& rule = HouseOfHorrors();
    std::set<uint16_t> reached{0, 10, 30, 40, 60, 70};
    const auto isReached = [&reached](uint16_t aStage) { return reached.count(aStage) != 0; };

    QuestRecovery::Steps steps;
    auto stage = steps.Update(rule, 1.0 / 60.0, false, isReached);
    REQUIRE(stage == uint16_t{80});
    reached.insert(*stage);

    double waited = 0.0;
    while (!(stage = steps.Update(rule, 0.25, false, isReached)))
    {
        waited += 0.25;
        REQUIRE(waited < QuestRecovery::kStageGap);
    }
    REQUIRE(stage == uint16_t{90});
    REQUIRE(waited + 0.25 >= QuestRecovery::kStageGap);
    reached.insert(*stage);

    REQUIRE_FALSE(steps.Update(rule, 1.0, false, isReached));
    REQUIRE_FALSE(steps.Finished());
}

TEST_CASE("A stage the controller already reached is not waited for", "[quest.feedback]")
{
    // DA10 stage 70 (Tyranus died in the house) set controller stage 80 before the quest failed
    const QuestRecovery::Rule& rule = HouseOfHorrors();
    const auto isReached = [](uint16_t aStage) { return aStage <= 80; };

    QuestRecovery::Steps steps;
    REQUIRE(steps.Update(rule, 1.0 / 60.0, false, isReached) == uint16_t{90});
}

TEST_CASE("A stopped controller gets no stage, and the recovery lingers before it ends", "[quest.feedback]")
{
    const QuestRecovery::Rule& rule = HouseOfHorrors();
    const auto isReached = [](uint16_t) { return false; };

    QuestRecovery::Steps steps;
    REQUIRE(steps.Update(rule, 0.0, false, isReached) == uint16_t{80});
    // Setting a stage on a stopped quest starts it again (Papyrus SetCurrentStageID, 56684)
    REQUIRE_FALSE(steps.Update(rule, QuestRecovery::kStageGap, true, isReached));

    // Its own updates stay local a while longer, the stop event comes after
    REQUIRE_FALSE(steps.Finished());
    REQUIRE_FALSE(steps.Update(rule, QuestRecovery::kLinger - QuestRecovery::kStageGap - 1.0, true, isReached));
    REQUIRE_FALSE(steps.Finished());
    REQUIRE_FALSE(steps.Update(rule, 1.0, true, isReached));
    REQUIRE(steps.Finished());
}

// Speaking with Silence (upstream issue 835): TG05SP, unshared, sets TG05 stage 60 after its kill scene's black frame

TEST_CASE("A member's wake-up stage waits for this client's own kill scene", "[quest.feedback]")
{
    const QuestRecovery::StageOrder& order = QuestRecovery::kStageOrders.front();
    REQUIRE(order.Quest == 0x21551);
    REQUIRE(order.Stage == 60);
    REQUIRE(order.SetBy == 0x2C367);

    // TG05SP still runs here: the stage and anything after it waits
    REQUIRE(QuestRecovery::MustWait(order, 0x21551, 60, true, 0.0));
    REQUIRE(QuestRecovery::MustWait(order, 0x21551, 200, true, 0.0));
    // Earlier stages, other quests and a finished sequence don't wait
    REQUIRE_FALSE(QuestRecovery::MustWait(order, 0x21551, 50, true, 0.0));
    REQUIRE_FALSE(QuestRecovery::MustWait(order, 0x3372B, 60, true, 0.0));
    REQUIRE_FALSE(QuestRecovery::MustWait(order, 0x21551, 60, false, 0.0));
}

TEST_CASE("A stuck sequence holds the party's stage for kOrderWait at most", "[quest.feedback]")
{
    const QuestRecovery::StageOrder& order = QuestRecovery::kStageOrders.front();
    REQUIRE(QuestRecovery::MustWait(order, 0x21551, 60, true, QuestRecovery::kOrderWait - 1.0));
    REQUIRE_FALSE(QuestRecovery::MustWait(order, 0x21551, 60, true, QuestRecovery::kOrderWait));
}

TEST_CASE("The kill scene's black frame is removed only once nothing will replace it", "[quest.feedback]")
{
    const QuestRecovery::CrossFade& crossFade = QuestRecovery::kCrossFades.front();
    // pTG05BlackFrame on SF_TG05SPKillScene, replaced by TG05 stage 60's pimodWakeUp
    REQUIRE(crossFade.Modifier == 0xE7C28);
    REQUIRE(crossFade.Quest == 0x21551);
    REQUIRE(crossFade.Stage == 60);
    REQUIRE(crossFade.Sequence == 0x2C367);

    // Wake-up ran first, the kill scene ended later
    REQUIRE(QuestRecovery::IsLeftOver(true, false, true));
    // The wake-up replaced it, the kill scene is still to apply it, or the party isn't there yet
    REQUIRE_FALSE(QuestRecovery::IsLeftOver(true, false, false));
    REQUIRE_FALSE(QuestRecovery::IsLeftOver(true, true, true));
    REQUIRE_FALSE(QuestRecovery::IsLeftOver(false, false, true));
}

// A stop this client performs for a member's stop isn't sent back

TEST_CASE("The game's stop of a quest ended for a member is not sent back", "[quest.feedback]")
{
    QuestSharing::EndingQuests ending;
    ending.Add(0x22F08);
    REQUIRE(ending.Contains(0x22F08));

    // Shutdown stages run on the way, then the stop event comes and ends the entry
    REQUIRE(ending.Suppresses(0x22F08, false));
    REQUIRE(ending.Contains(0x22F08));
    REQUIRE(ending.Suppresses(0x22F08, true));
    REQUIRE_FALSE(ending.Contains(0x22F08));

    // The restart that waited for it is this client's own again
    REQUIRE_FALSE(ending.Suppresses(0x22F08, false));
    REQUIRE_FALSE(ending.Suppresses(0x22F08, true));
}

TEST_CASE("Echoes of a restarted quest's stop would stop the new run for everyone", "[quest.feedback]")
{
    // The server's side: a member restarts the quest, then this client's late stop of the old run arrives
    QuestSharing::Run run;
    REQUIRE(run.Relay(QuestSharing::kStarted, 0));
    REQUIRE(run.Relay(QuestSharing::kFailed, 500));
    REQUIRE(run.Relay(QuestSharing::kStarted, 0));
    REQUIRE(run.Relay(QuestSharing::kStopped, 500));

    // So the client keeps it: the stop it performs for the member's failure is consumed, not sent
    QuestSharing::EndingQuests ending;
    ending.Add(0x22F08);
    REQUIRE(ending.Suppresses(0x22F08, true));
}

TEST_CASE("A quest whose stop never comes stops being held after kEndingWait", "[quest.feedback]")
{
    QuestSharing::EndingQuests ending;
    ending.Add(1);
    ending.Add(2);
    ending.Update(QuestSharing::kEndingWait - 1.0);
    // Ended again: its wait starts over
    ending.Add(2);
    ending.Update(1.0);
    REQUIRE_FALSE(ending.Contains(1));
    REQUIRE(ending.Contains(2));
    REQUIRE_FALSE(ending.Suppresses(1, false));
    ending.Update(QuestSharing::kEndingWait);
    REQUIRE(ending.Entries.empty());
}
