#pragma once

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>

/**
 * @brief How quests are shared in a party, the same rules on the server and on every client.
 *
 * 1. Every quest with a journal entry is shared: main, guild, Daedric, side, civil war, DLC and miscellaneous alike.
 *    Quests without one (type None) are the game's own background systems, which already run on every client.
 * 2. A quest a member starts starts for everyone in the party, each stage a member reaches is set for everyone who
 *    hasn't reached it yet, and a quest a member stops, completes or fails ends the same way for everyone it runs for.
 *    A start the game itself gave up on (its story event couldn't start it) is not a start.
 * 3. Only what happens while in the party is shared: joining a party doesn't copy quests from before.
 * 4. A quest that stops and starts again, a repeatable one, is shared again from its start.
 * 5. The party leader may hold a member's stage while one of the leader's own scenes still needs to get to it, for at
 *    most 45 seconds after that scene stops moving (see QuestStall.h).
 * 6. A member's stop waits until this client has run the fragments of the member stages it applied before it, at most
 *    kStopWait seconds, because the member's quest only stopped after them. The quest's own fragments usually stop it
 *    themselves (a fail stage's Stop()), otherwise it's completed or failed and stopped through the game's own path.
 *    What the game does while stopping it is the member's stop, not sent back (EndingQuests).
 *    Nothing else delays or drops an update.
 */
namespace QuestSharing
{
// TESQuest::Type::None
inline constexpr uint8_t kNoJournalType = 0;

[[nodiscard]] constexpr bool IsShared(uint8_t aQuestType) noexcept
{
    return aQuestType != kNoJournalType;
}

// Same values as RequestQuestUpdate and NotifyQuestUpdate. The last three are stops, telling how the quest ended.
enum Status : uint8_t
{
    kStageUpdate = 0,
    kStarted = 1,
    kStopped = 2,
    kCompleted = 3,
    kFailed = 4,
};

[[nodiscard]] constexpr bool IsValid(uint8_t aStatus) noexcept
{
    return aStatus <= kFailed;
}

[[nodiscard]] constexpr bool IsStop(uint8_t aStatus) noexcept
{
    return aStatus >= kStopped && aStatus <= kFailed;
}

// TESQuest flags (+0xDC). SetCompleted (24991) sets Completed, SetFailed (24988) sets Completed and Failed, and both
// return early once Completed is set: a quest keeps the first way it ended.
inline constexpr uint16_t kCompletedFlag = 1 << 1;
inline constexpr uint16_t kFailedFlag = 1 << 6;

// How a quest that just stopped ended, from its flags. The stop event itself doesn't say (25047 sends every stop as
// {formId, false, false}).
[[nodiscard]] constexpr Status StopStatus(uint16_t aQuestFlags) noexcept
{
    if (aQuestFlags & kFailedFlag)
        return kFailed;

    return (aQuestFlags & kCompletedFlag) ? kCompleted : kStopped;
}

// Whether a member's stop still has to complete or fail this client's quest
[[nodiscard]] constexpr bool NeedsOutcome(uint8_t aStatus, uint16_t aQuestFlags) noexcept
{
    return (aStatus == kCompleted || aStatus == kFailed) && !(aQuestFlags & kCompletedFlag);
}

// Longest a member's stop waits for this client's fragments, see rule 6
inline constexpr double kStopWait = 10.0;

/**
 * @brief Member stages a client applied whose fragments haven't run yet. A stage counts as run once one of its log
 * entries reports done; entries whose conditions fail never report, so all count as run kStopWait seconds after the
 * last stage was applied.
 */
struct AppliedStages
{
    std::vector<uint16_t> Stages{};
    double Age{0.0};

    void Add(uint16_t aStage)
    {
        Stages.push_back(aStage);
        Age = 0.0;
    }

    void Done(uint16_t aStage)
    {
        if (const auto it = std::find(Stages.begin(), Stages.end(), aStage); it != Stages.end())
            Stages.erase(it);
    }

    [[nodiscard]] bool Running() const noexcept { return !Stages.empty() && Age < kStopWait; }
};

/**
 * @brief A member's stop waiting for this client's fragments, see rule 6.
 */
struct PendingStop
{
    uint8_t Status{kStopped};
    double Waited{0.0};

    // Whether to end the quest now
    [[nodiscard]] bool Update(const bool aFragmentsRunning, const double aDelta) noexcept
    {
        Waited += aDelta;
        return !aFragmentsRunning || Waited >= kStopWait;
    }
};

// Longest a quest this client ends for a member stays in EndingQuests
inline constexpr double kEndingWait = 30.0;

/**
 * @brief Quests this client is ending because a member's ended (rule 6). The game stops them a frame or more later and
 * runs their shutdown stages on the way. Those stage and stop events are the member's stop, already relayed: sent back,
 * the server takes them for this client's own and relays them to a run that may have started again meanwhile, stopping
 * it for everyone. Updates of an ending quest wait too, so a restart doesn't mix into the run being stopped.
 * An entry goes when the quest's stop event comes, or kEndingWait after it was added.
 */
struct EndingQuests
{
    struct Entry
    {
        uint32_t Quest;
        double Age;
    };

    std::vector<Entry> Entries{};

    void Add(const uint32_t aQuest)
    {
        for (Entry& entry : Entries)
        {
            if (entry.Quest == aQuest)
            {
                entry.Age = 0.0;
                return;
            }
        }
        Entries.push_back({aQuest, 0.0});
    }

    [[nodiscard]] bool Contains(const uint32_t aQuest) const noexcept
    {
        return std::any_of(Entries.begin(), Entries.end(), [aQuest](const Entry& acEntry) { return acEntry.Quest == aQuest; });
    }

    // Whether this client's update of the quest is kept from being sent. The quest's stop event ends the entry.
    [[nodiscard]] bool Suppresses(const uint32_t aQuest, const bool aIsStop)
    {
        const auto it = std::find_if(Entries.begin(), Entries.end(), [aQuest](const Entry& acEntry) { return acEntry.Quest == aQuest; });
        if (it == Entries.end())
            return false;

        if (aIsStop)
            Entries.erase(it);
        return true;
    }

    void Update(const double aDelta)
    {
        for (Entry& entry : Entries)
            entry.Age += aDelta;
        Entries.erase(std::remove_if(Entries.begin(), Entries.end(), [](const Entry& acEntry) { return acEntry.Age >= kEndingWait; }), Entries.end());
    }
};

/**
 * @brief What the server remembers of one quest in a party. Members' scripts often reach the same start, stage or stop
 * on their own, which is relayed once.
 */
struct Run
{
    enum class State : uint8_t
    {
        Unknown,
        Running,
        Stopped,
    };

    State Current{State::Unknown};
    std::set<uint16_t> Stages{};

    // Whether the update goes to the rest of the party
    [[nodiscard]] bool Relay(const uint8_t aStatus, const uint16_t aStage) noexcept
    {
        switch (aStatus)
        {
        case kStarted:
            if (Current == State::Running)
                return false;
            // The stage a start reports is where the quest happened to be, its stage event still follows
            Begin();
            return true;
        case kStageUpdate:
            // A stage of a quest started before the party, or of a run whose start was missed, starts the run
            if (Current != State::Running)
                Begin();
            return Stages.insert(aStage).second;
        case kStopped:
        case kCompleted:
        case kFailed:
            // The first member to report the stop decides how the run ended, the others' stops follow from it
            if (Current == State::Stopped)
                return false;
            Current = State::Stopped;
            Stages.clear();
            return true;
        default: return false;
        }
    }

private:
    void Begin()
    {
        Current = State::Running;
        Stages.clear();
    }
};
} // namespace QuestSharing
