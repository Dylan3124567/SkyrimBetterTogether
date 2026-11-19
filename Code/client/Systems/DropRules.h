#pragma once

#include <cstdint>

#include <glm/vec3.hpp>

#include <Systems/SyncRules.h>

/**
 * @brief Decisions behind shared dropped items. These have no game dependencies so they can be unit tested.
 */
namespace DropRules
{
// Exterior items are created in a loaded cell, 5x5 cells of 4096 units around the player, so stay inside of that.
inline constexpr float cSpawnDistance = 8192.f;

/**
 * @brief Whether the camera can't be used to place a dropped item. The engine drops items along the view of the camera
 * object, so one that is not where the player is leaves the item near the world origin instead.
 */
inline bool IsDropOriginStale(const glm::vec3& acCamera, const glm::vec3& acPlayer) noexcept
{
    return SyncRules::IsDropMisplaced(acCamera, acPlayer);
}

/**
 * @brief Whether a shared item is close enough to this client's player to be shown.
 * @param aSameCell For interiors, whether the player is in the item's cell.
 * @param aSameWorldSpace For exteriors, whether the player is in the item's world space.
 */
inline bool ShouldShowDrop(const bool aIsInterior, const bool aSameCell, const bool aSameWorldSpace, const glm::vec3& acItem, const glm::vec3& acPlayer) noexcept
{
    if (aIsInterior)
        return aSameCell;

    if (!aSameWorldSpace)
        return false;

    const float cX = acItem.x - acPlayer.x;
    const float cY = acItem.y - acPlayer.y;
    return cX * cX + cY * cY <= cSpawnDistance * cSpawnDistance;
}

/**
 * @brief Whether a reference made on this client can be turned into the real item by the engine's own pick up.
 * A copy only has the base item, so anything enchanted, tempered, poisoned or filled has to come from the server's entry.
 */
inline bool CanPickUpReference(const bool aIsOwnReference, const bool aHasExtraData) noexcept
{
    return aIsOwnReference || !aHasExtraData;
}
} // namespace DropRules
