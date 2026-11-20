#pragma once

#include <cstdint>
#include <string_view>

/**
 * @brief Decisions behind keeping NPC copies in step with their owner. No game dependencies so they can be unit tested.
 */
namespace NpcSyncRules
{
// Owners send a snapshot at least every ~90 ms, this long without one means nobody is running the actor anymore.
inline constexpr double cOwnerSilentTime = 5000.0;
inline constexpr double cClaimRetryTime = 5000.0;
// Closer than this a MoveTo adds nothing, it only throws the actor out of the furniture it is using.
inline constexpr float cAlreadyPlacedDistance = 128.f;
// The game calls the voice function twice per line, one frame apart.
inline constexpr double cRepeatedLineTime = 250.0;
inline constexpr double cSceneHoldTime = 5000.0;

// ActorState::kLifeStateMask, see the static_assert next to the only user in AnimationSystem.cpp
inline constexpr uint32_t cLifeStateMask = 0xFu << 21;

/**
 * @brief The state flags a remote copy takes from a replayed action.
 *
 * Actions are played behind the owner and replays are older still, so a copy can be dead by the time one comes up. Taking
 * its life state as well would put a corpse back on its feet with no AI.
 */
inline uint32_t MergeReplayedState(const uint32_t aCurrent, const uint32_t aReplayed, const bool aIsDead) noexcept
{
    return aIsDead ? (aReplayed & ~cLifeStateMask) | (aCurrent & cLifeStateMask) : aReplayed;
}

/**
 * @brief What masterAmbushScript leaves behind once it wakes its actor: Variable01 at 1 and an aggressive Aggression.
 * Its triggers only fire on the client that saw them, which often isn't the owner.
 */
inline bool IsAmbushAwake(const float aVariable01, const float aAggression) noexcept
{
    return aVariable01 >= 1.f && aAggression >= 2.f;
}

/**
 * @brief Whether a client should ask for an NPC it only has a remote copy of, so that its AI runs somewhere.
 * @param aWokeUnderOwner The AI has to run on this client now: an ambush woke here, or this client's player talks to it.
 * @param aLastSnapshot When the owner was last heard from in milliseconds, negative until the first check. Owned by the caller.
 * @param aLastClaim When the last claim went out, owned by the caller.
 */
inline bool ShouldClaimActor(const bool aCanClaim, const bool aWokeUnderOwner, const double aNow, double& aLastSnapshot, double& aLastClaim) noexcept
{
    if (aLastSnapshot < 0.0)
        aLastSnapshot = aNow;

    if (!aCanClaim || aNow - aLastClaim < cClaimRetryTime)
        return false;

    if (!aWokeUnderOwner && aNow - aLastSnapshot < cOwnerSilentTime)
        return false;

    aLastClaim = aNow;
    return true;
}

/**
 * @brief Tracks an owned actor held in furniture while its scene waits for it to finish a package.
 *
 * The owner's copy has to walk there for the scene to go on, so one that stays put holds up every client. Fires once per
 * phase, a package that really does sit the actor down goes straight back to it.
 *
 * @param aHoldStart When the wait started in milliseconds, negative if there is none. Owned by the caller.
 * @param aReleasedPhase The phase key the actor was last released in, owned by the caller.
 */
inline bool ShouldReleaseFromScene(const bool aIsBlockingPhase, const uint64_t aPhaseKey, const double aNow, double& aHoldStart, uint64_t& aReleasedPhase) noexcept
{
    if (!aIsBlockingPhase || aReleasedPhase == aPhaseKey)
    {
        aHoldStart = -1.0;
        return false;
    }

    if (aHoldStart < 0.0)
        aHoldStart = aNow;

    if (aNow - aHoldStart < cSceneHoldTime)
        return false;

    aHoldStart = -1.0;
    aReleasedPhase = aPhaseKey;
    return true;
}

/**
 * @brief Factions that say how an actor stands with the player: follower, hireling, spouse. Form ids are from Skyrim.esm.
 *
 * Each client has its own player, so an owner's factions never change these here. Taken from another client they made
 * its follower ours as well, while our follower quest never had the actor: following, trading and dismissing did nothing,
 * and stayed broken in our save after disconnecting.
 */
inline bool IsPlayerRelationshipFaction(const uint32_t aFormId) noexcept
{
    switch (aFormId)
    {
    case 0x5C84E: // CurrentFollowerFaction
    case 0x5C84D: // PotentialFollowerFaction
    case 0x5C84C: // DismissedFollowerFaction
    case 0x84D1B: // PlayerFollowerFaction
    case 0xBD738: // CurrentHireling
    case 0xBCC9A: // PotentialHireling
    case 0x19809: // PotentialMarriageFaction
    case 0x7431A: // MarriageCourtingFaction
    case 0xFF7F3: // MarriageAskedFaction
    case 0xC6472: // PlayerMarriedFaction
        return true;
    default:
        return false;
    }
}

/**
 * @brief Whether a voice line is the second half of the same call.
 * Restarting it on the other clients plays it twice over and takes the mouth with it.
 */
inline bool IsRepeatedLine(const std::string_view aLast, const std::string_view aLine, const double aLastTime, const double aNow) noexcept
{
    return aLast == aLine && aNow - aLastTime < cRepeatedLineTime;
}
} // namespace NpcSyncRules
