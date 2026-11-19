#pragma once

#include <Systems/UiRules.h>

namespace CompassMarkers
{
// Other players and their waypoints
inline constexpr size_t kMaxTargets = 32;

struct Target
{
    NiPoint3 Position;
    uint32_t Colour;
    UiRules::CompassKind Kind;
};

// The HUD does not update on the game thread, so targets are handed over as a copy. The local player's own waypoint is
// the game's, drawn in aLocalColour, or as the game draws it when that is negative.
void SetTargets(const Target* apTargets, size_t aCount, int32_t aLocalColour) noexcept;

// What the last compass update did, for tests
struct Status
{
    size_t Targets;
    size_t Decorated; // Targets the HUD helper drew, 0 while it isn't loaded
    uint32_t HelperLoads;
    int32_t LocalColour;
};
Status GetStatus() noexcept;
} // namespace CompassMarkers
