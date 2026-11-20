#pragma once

#include <cstdint>

/**
 * @brief Who decides that an NPC dies or goes down in co-op. No game dependencies so they can be unit tested.
 *
 * Health travels as deltas, while each game has its own idea of who is essential: a quest alias only exists on the client
 * running the quest. A brawl makes its opponent essential through an alias on the brawling player's client, the owner of
 * the opponent knew nothing of it and let the relayed punches kill.
 * 1. A copy never dies or goes down from its own health, which misses whatever its owner's game restores. Its owner's
 *    death and bleedout state drive it.
 * 2. The owner kills with a blow from another client's player only if neither game would just knock the actor down. The
 *    hitter says so with the blow, the owner checks its own actor as if its player had hit it.
 * 3. Copies go down and get up through the game's own code, so their scripts hear of it (OnEnterBleedout).
 */
namespace CombatRules
{
// ACTOR_LIFE_STATE, bits 21-24 of ActorState::flags1
enum LifeState : uint32_t
{
    kAlive = 0,
    kDying = 1,
    kDead = 2,
    kUnconscious = 3,
    kReanimate = 4,
    kRecycle = 5,
    kRestrained = 6,
    kEssentialDown = 7,
    kBleedout = 8,
};

inline constexpr uint32_t cLifeStateShift = 21;
inline constexpr uint32_t cLifeStateMask = 0xFu << cLifeStateShift;

[[nodiscard]] constexpr uint32_t GetLifeState(const uint32_t aFlags1) noexcept
{
    return (aFlags1 & cLifeStateMask) >> cLifeStateShift;
}

/**
 * @brief Whether health changes may decide the actor's life on this client (Actor::HandleHealthDamage).
 * Players keep their own: a downed player goes down here through its respawn mode.
 */
[[nodiscard]] constexpr bool ReactsToHealth(const bool aIsRemote, const bool aIsPlayer) noexcept
{
    return !aIsRemote || aIsPlayer;
}

enum class Blow : uint8_t
{
    None,
    KnockDown,
    Kill,
};

/**
 * @brief What the owner does when a blow relayed from another client's player takes its actor to zero health.
 * @param aHitterKnockOnly The hitter's game would only knock the actor down (NonLethal in the message).
 * @param aOwnerKnockOnly This game would only knock the actor down if its player had hit it.
 */
[[nodiscard]] constexpr Blow ResolveRelayedBlow(
    const float aDelta, const float aNewHealth, const bool aIsOwner, const bool aIsPlayer, const bool aIsDeadOrDown, const bool aHitterKnockOnly,
    const bool aOwnerKnockOnly) noexcept
{
    if (!aIsOwner || aIsPlayer || aIsDeadOrDown || aDelta >= 0.f || aNewHealth > 0.f)
        return Blow::None;

    return aHitterKnockOnly || aOwnerKnockOnly ? Blow::KnockDown : Blow::Kill;
}

/**
 * @brief Whether the owner leaves a relayed blow out of its actor's health while the actor is down from a knock down.
 *
 * Punches the hitter threw before its copy went down are still on their way. The game only skips the death of a downed
 * actor in essential down (Actor::IsDead(false), vfunc 0x99), not in bleedout (life state 8), and a brawl opponent knocked
 * down here isn't essential in this game. The knock out never becomes a death.
 */
[[nodiscard]] constexpr bool IgnoresRelayedBlow(const float aDelta, const bool aKnockedOut, const bool aIsDown) noexcept
{
    return aDelta < 0.f && aKnockedOut && aIsDown;
}

enum class Follow : uint8_t
{
    None,
    Kill,
    Revive,
    KnockDown,
    StandUp,
};

/**
 * @brief What a copy does to match the death and bleedout state its owner reported.
 *
 * Going down always goes through the game: replayed actions write the owner's life state into the copy without its
 * scripts ever hearing of it, so a copy already showing as down is put down again properly.
 */
[[nodiscard]] constexpr Follow FollowOwner(const bool aCopyDead, const bool aCopyDown, const bool aOwnerDead, const bool aOwnerDown) noexcept
{
    if (aOwnerDead)
        return aCopyDead ? Follow::None : Follow::Kill;

    if (aCopyDead)
        return Follow::Revive;

    if (aOwnerDown)
        return Follow::KnockDown;

    return aCopyDown ? Follow::StandUp : Follow::None;
}

/**
 * @brief Thieves Guild "Taking Care of Business" (TG01, Skyrim.esm 0x1F326).
 *
 * Brynjolf's job (stage 20) clears Keerava's, Bersi's and Haelga's essential flag. A brawl (FavorDialogueScript.Brawl)
 * makes its opponent essential only on the brawling player's client, so co-op damage killed debtors on their owner's
 * client. Each debtor alias' TG01DeathCoverageScript sets a stage when the player kills the debtor, which a co-op death
 * never did (no killer), leaving the job stuck.
 */
struct QuestVictim
{
    uint32_t AliasId;
    uint16_t KilledByPlayerStage;
};

inline constexpr uint32_t cTG01 = 0x1F326;
inline constexpr uint16_t cTG01JobStage = 20;
// KeeravaAlias, BersiAlias, HaelgaAlias
inline constexpr QuestVictim cTG01Debtors[] = {{2, 35}, {3, 42}, {4, 47}};

// A party member's kill sets the debtor's killed-by-player stage on that member's client only, it reaches the owner through
// the quest sync. Seconds a debtor stays dead before its owner takes it for a co-op death, well past that sync.
inline constexpr double cDebtorReviveGrace = 15.0;

/**
 * @brief Whether the owner brings back a dead debtor of TG01.
 * @param aQuestRunning TG01 runs, so the debtor's alias is filled and the debt was collected or is still pending.
 * @param aKilledByPlayerStage The debtor's TG01DeathCoverageScript stage is done, the quest counted a player's kill.
 * @param aDeadSeconds How long this client has seen the debtor dead without a break.
 * @param aPlayerKilledThisSession A player, this client's or another's copy, killed it while this client watched. A debtor
 * already dead when the save loaded has no such killer, the co-op deaths this repair is for.
 */
[[nodiscard]] constexpr bool RevivesDebtor(
    const bool aIsDead, const bool aQuestRunning, const bool aJobGiven, const bool aKilledByPlayerStage, const double aDeadSeconds, const bool aPlayerKilledThisSession) noexcept
{
    return aIsDead && aQuestRunning && aJobGiven && !aKilledByPlayerStage && aDeadSeconds >= cDebtorReviveGrace && !aPlayerKilledThisSession;
}

/**
 * @brief DGIntimidateQuest (Skyrim.esm 0x47AE6), the brawl.
 *
 * Its opponent alias is essential, so in the game a brawl never ends with a dead opponent, and one that did sat at
 * stage 15 for good with the opponent's force greet and fight packages and Confidence 4. Stage 200 ends it the game's way.
 */
inline constexpr uint32_t cBrawlQuest = 0x47AE6;
inline constexpr uint32_t cBrawlOpponentAlias = 0;
inline constexpr uint16_t cBrawlEndStage = 200;

[[nodiscard]] constexpr bool EndsBrawl(const bool aQuestRunning, const bool aEnded, const bool aOpponentDead) noexcept
{
    return aQuestRunning && !aEnded && aOpponentDead;
}
} // namespace CombatRules
