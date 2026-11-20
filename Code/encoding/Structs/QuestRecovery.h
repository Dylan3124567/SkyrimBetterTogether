#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

/**
 * @brief Finishing what a quest that ended early never got to finish.
 *
 * A party member's failure can end a quest in the middle of a sequence that, alone, only ends at a later stage of it.
 * The House of Horrors (DA10) locks the player inside Logrolf's house at stages 15 and 20 and starts the haunting, whose
 * loop (DA10HauntingControllerScript, state "running") shakes the camera every 0-2 seconds until the controller quest
 * reaches stage 90. DA10 stage 70 sets controller stage 80, and only stage 80 (Fragment_55) unlocks the door and sets
 * controller stage 90. Its fail stages 500 and 550 do neither, and single player only fails it outside the house (500
 * needs Tyranus outside): a failure relayed from a member outside leaves the player locked in, the camera shaking for
 * good.
 *
 * So once such a quest stops having reached SinceStage but not UntilStage, the door is unlocked as UntilStage does and
 * the controller's stages are set as UntilStage and the scene after it set them.
 */
namespace QuestRecovery
{
struct Rule
{
    // The quest that stopped
    uint32_t Quest;
    // Reached: the sequence began
    uint16_t SinceStage;
    // Not reached: the quest's own scripts never ended it
    uint16_t UntilStage;
    // Reference the sequence locked, unlocked as UntilStage does (Papyrus Lock(false, false))
    uint32_t Door;
    // Quest whose script runs the sequence
    uint32_t Controller;
    // Done once the controller's script is running
    uint16_t ControllerStartStage;
    // Set in order, kStageGap seconds apart
    std::array<uint16_t, 2> ControllerStages;
};

// Skyrim.esm: DA10's MainDoor alias 24 is forced to MarkarthAbanonedHouseDoorREF 0x18BF9, the house's side of the load
// door (cell MarkarthAbandonedHouse 0x18BE9, key MarkarthAbandonedHouseKey). DA10HauntingController 0x2BB38's stage 0
// starts the loop, stage 80 sets hauntingStage 8 (lights back, ghosts dropped), 90 sets 9 (loop ends, imagespace
// removed, stage 100 stops the controller).
inline constexpr std::array kRules{
    Rule{0x22F08, 15, 80, 0x18BF9, 0x2BB38, 0, {80, 90}},
};

// Stage 80's pass through the loop waits 3 seconds (lightsBackOnTime) after the loop's own 0.3 second wait, and has to
// start before stage 90 ends the loop
inline constexpr double kStageGap = 4.0;
// After the last stage the loop stops the controller (stage 100) within a few seconds. Until then and a while after,
// the controller's own updates aren't shared: its stop would stop a member's loop that still runs, for good.
inline constexpr double kLinger = 30.0;

[[nodiscard]] constexpr bool Applies(const bool aQuestStopped, const bool aSinceStageDone, const bool aUntilStageReached) noexcept
{
    return aQuestStopped && aSinceStageDone && !aUntilStageReached;
}

/**
 * @brief Sets a rule's controller stages from a per-frame update.
 */
struct Steps
{
    size_t Next{0};
    // Since the last stage was set
    double Waited{kStageGap};
    // Since there was nothing left to set
    double Over{0.0};

    // The controller stage to set now, if any. One the controller reached on its own needs no wait after it.
    template <class TIsReached>
    [[nodiscard]] std::optional<uint16_t> Update(const Rule& acRule, const double aDelta, const bool aControllerStopped, TIsReached&& aIsReached)
    {
        Waited += aDelta;

        // A stopped controller's loop has ended (its last stage stops it), and setting a stage would start it again
        if (aControllerStopped)
            Next = acRule.ControllerStages.size();
        while (Next < acRule.ControllerStages.size() && aIsReached(acRule.ControllerStages[Next]))
            ++Next;

        if (Next >= acRule.ControllerStages.size())
        {
            Over += aDelta;
            return std::nullopt;
        }

        if (Waited < kStageGap)
            return std::nullopt;

        Waited = 0.0;
        return acRule.ControllerStages[Next++];
    }

    [[nodiscard]] bool Finished() const noexcept { return Over >= kLinger; }
};

/**
 * @brief A stage of a shared quest that a local quest's sequence sets when it ends.
 *
 * Speaking with Silence (TG05, Thieves Guild, shared) starts TG05SP at its stage 50. TG05SP has no journal entry (type
 * None), so it isn't shared and every client runs its own: the poison arrow knock-out, then the kill scene, whose last
 * fragment applies pTG05BlackFrame (0xE7C28) as the crossfade. Only after it does TG05SP's stage 40 set TG05 stage 60,
 * whose fragment applies pimodWakeUp (0x3AE43) over the black frame. The first member through sets 60 for everyone: on
 * a client whose kill scene still runs, the wake-up comes first and the black frame stays (upstream issue 835).
 * So a member's update of Quest at or past Stage waits while SetBy runs here, at most kOrderWait seconds in all.
 */
struct StageOrder
{
    uint32_t Quest;
    uint16_t Stage;
    uint32_t SetBy;
};

inline constexpr std::array kStageOrders{
    StageOrder{0x21551, 60, 0x2C367},
};

// TG05SP's knock-out, Karliah and Mercer's lines and the kill scene take a couple of minutes, a scene stuck on an NPC
// another client owns must not hold the quest for good
inline constexpr double kOrderWait = 180.0;

[[nodiscard]] constexpr bool MustWait(const StageOrder& acOrder, const uint32_t aQuest, const uint16_t aStage, const bool aSetByRunning, const double aWaited) noexcept
{
    return aQuest == acOrder.Quest && aStage >= acOrder.Stage && aSetByRunning && aWaited < kOrderWait;
}

/**
 * @brief A crossfade a quest's sequence applied that the stage meant to replace it no longer will, see StageOrder.
 *
 * Once Stage is done and Sequence stopped here, Modifier still being the crossfade means the replacing stage ran
 * before it: a save from before this fix, or a StageOrder that timed out.
 */
struct CrossFade
{
    uint32_t Quest;
    uint16_t Stage;
    uint32_t Sequence;
    uint32_t Modifier;
};

inline constexpr std::array kCrossFades{
    CrossFade{0x21551, 60, 0x2C367, 0xE7C28},
};

// After the sequence stops, the stage's own fragment (TG05 Fragment_15 moves the player first) gets this long to apply
// its crossfade
inline constexpr double kCrossFadeDelay = 10.0;

[[nodiscard]] constexpr bool IsLeftOver(const bool aStageDone, const bool aSequenceRunning, const bool aModifierApplied) noexcept
{
    return aStageDone && !aSequenceRunning && aModifierApplied;
}
} // namespace QuestRecovery
