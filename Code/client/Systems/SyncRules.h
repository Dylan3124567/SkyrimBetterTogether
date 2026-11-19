#pragma once

#include <cmath>
#include <cstdint>

#include <glm/vec3.hpp>

/**
 * @brief Decisions behind the client's sync guards. These have no game dependencies so they can be unit tested.
 */
namespace SyncRules
{
// A dropped item further than this from where it should be has been misplaced rather than rolled or bounced.
inline constexpr float cMaxDropDistance = 384.f;
inline constexpr float cDropForwardDistance = 70.f;
inline constexpr float cDropHeight = 50.f;
// Snapshots are unreliable and low priority actors only send them every ~90 ms, so wait for several before acting.
inline constexpr double cFurnitureMismatchTime = 1000.0;

// The stage that removes Thorald from the Thalmor faction, and the one set once he reaches safety.
inline constexpr uint16_t cThoraldFreedStage = 60;
inline constexpr uint16_t cThoraldSafeStage = 100;

/**
 * @brief Where a dropped item should end up: in front of the actor it fell from.
 * @param aHeading The actor's rotation around Z, in radians.
 */
inline glm::vec3 GetDropPosition(const glm::vec3& acActorPosition, const float aHeading) noexcept
{
    return {acActorPosition.x + std::sin(aHeading) * cDropForwardDistance, acActorPosition.y + std::cos(aHeading) * cDropForwardDistance, acActorPosition.z + cDropHeight};
}

inline bool IsDropMisplaced(const glm::vec3& acItem, const glm::vec3& acExpected) noexcept
{
    if (!std::isfinite(acItem.x) || !std::isfinite(acItem.y) || !std::isfinite(acItem.z))
        return true;

    const glm::vec3 cOffset = acItem - acExpected;
    return cOffset.x * cOffset.x + cOffset.y * cOffset.y + cOffset.z * cOffset.z > cMaxDropDistance * cMaxDropDistance;
}

/**
 * @brief Tracks whether a remote actor is held in a furniture pose its owner has already left.
 *
 * Nothing tells a remote copy to stand up when the owner's copy never sat down, which happens after an ownership change,
 * so the copy has to notice by itself. A short mismatch is normal, as actions and snapshots travel separately.
 *
 * @param aMismatchStart Time the mismatch started in milliseconds, negative if there is none. Owned by the caller.
 * @return true once the copy should be released.
 */
inline bool ShouldReleaseFurniture(const bool aIsHeld, const bool aOwnerHolds, const double aNow, double& aMismatchStart) noexcept
{
    if (!aIsHeld || aOwnerHolds)
    {
        aMismatchStart = -1.0;
        return false;
    }

    if (aMismatchStart < 0.0)
        aMismatchStart = aNow;

    if (aNow - aMismatchStart < cFurnitureMismatchTime)
        return false;

    aMismatchStart = -1.0;
    return true;
}

/**
 * @brief Whether a remote copy has to be sent out of furniture now that its owner is out of it.
 *
 * A copy plays its owner's furniture enter and exit as replayed actions, without the furniture itself. The exit can leave
 * the body in the furniture pose while the copy's state already says it stands: frozen half way out of a bed, or sitting
 * in mid air where the owner got up. Its state then shows nothing wrong, so this goes by the owner alone. Once the owner
 * has been out of furniture for a moment the copy is sent out once, and once after the copy appears, as it replays
 * whatever its owner did before.
 *
 * @param aPending Whether the copy may still be in furniture, true for a new copy. Owned by the caller.
 * @param aOwnerOutSince When the owner was first seen out of furniture in milliseconds, negative while it is in. Owned by the caller.
 */
inline bool ShouldLeaveFurniture(const bool aOwnerHolds, const double aNow, bool& aPending, double& aOwnerOutSince) noexcept
{
    if (aOwnerHolds)
    {
        aPending = true;
        aOwnerOutSince = -1.0;
        return false;
    }

    if (!aPending)
        return false;

    if (aOwnerOutSince < 0.0)
        aOwnerOutSince = aNow;

    if (aNow - aOwnerOutSince < cFurnitureMismatchTime)
        return false;

    aPending = false;
    return true;
}

inline bool ShouldFreeThorald(const bool aFreedStageReached, const bool aSafeStageReached) noexcept
{
    return aFreedStageReached && !aSafeStageReached;
}

/**
 * @brief Whether an action can only come from the player trying to do something, form IDs are from Skyrim.esm.
 * Turning and idle actions are left out on purpose, looking around must not end spawn protection.
 */
inline bool IsDeliberateAction(const uint32_t aActionId) noexcept
{
    switch (aActionId)
    {
    case 0x13004: // ActionLeftAttack
    case 0x13005: // ActionRightAttack
    case 0x50C96: // ActionDualAttack
    case 0x2E2F6: // ActionLeftPowerAttack
    case 0x13383: // ActionRightPowerAttack
    case 0x2E2F7: // ActionDualPowerAttack
    case 0x13008: // ActionVoice
    case 0x13458: // ActionVoiceReady
    case 0x13457: // ActionVoiceRelease
    case 0x132AF: // ActionDraw
    case 0x46BAF: // ActionSheath
    case 0x13006: // ActionJump
    case 0x13007: // ActionSneak
    case 0x3B4A7: // ActionSprintStart
    case 0x13009: // ActionActivate
        return true;
    default:
        return false;
    }
}
} // namespace SyncRules
