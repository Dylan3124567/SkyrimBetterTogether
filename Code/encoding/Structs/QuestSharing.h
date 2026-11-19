#pragma once

#include <cstdint>
#include <set>

/**
 * @brief How quests are shared in a party, the same rules on the server and on every client.
 *
 * 1. Every quest with a journal entry is shared: main, guild, Daedric, side, civil war, DLC and miscellaneous alike.
 *    Quests without one (type None) are the game's own background systems, which already run on every client.
 * 2. A quest a member starts starts for everyone in the party, each stage a member reaches is set for everyone who
 *    hasn't reached it yet, and a quest a member stops or completes stops for everyone.
 * 3. Only what happens while in the party is shared: joining a party doesn't copy quests from before.
 * 4. A quest that stops and starts again, a repeatable one, is shared again from its start.
 * 5. The party leader may hold a member's stage while one of the leader's own scenes still needs to get to it, for at
 *    most 45 seconds after that scene stops moving (see QuestStall.h). Nothing else delays or drops an update.
 */
namespace QuestSharing
{
// TESQuest::Type::None
inline constexpr uint8_t kNoJournalType = 0;

[[nodiscard]] constexpr bool IsShared(uint8_t aQuestType) noexcept
{
    return aQuestType != kNoJournalType;
}

// Same values as RequestQuestUpdate and NotifyQuestUpdate
enum Status : uint8_t
{
    kStageUpdate = 0,
    kStarted = 1,
    kStopped = 2,
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
