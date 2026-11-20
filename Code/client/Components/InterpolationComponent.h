#pragma once

#ifndef TP_INTERNAL_COMPONENTS_GUARD
#error Include Components.h instead
#endif

#include <Structs/AnimationVariables.h>
#include <Systems/PlaybackClock.h>

struct InterpolationComponent
{
    struct TimePoint
    {
        // The owner's capture tick
        uint64_t Tick{};
        glm::vec3 Position{};
        glm::vec3 Rotation{};
        AnimationVariables Variables{};
        float Direction{};

        TimePoint() = default;
    };

    List<TimePoint> TimePoints;
    // Last displayed position
    glm::vec3 Position;

    PlaybackClock Clock;

    // Left over from correcting an extrapolation, decays over a few frames instead of popping the actor into place
    glm::vec3 PositionError{};
    bool IsExtrapolating{false};
    bool HasNewTimePoint{false};

    // Latest word from the owner, and when this copy started disagreeing with it, see SyncRules::ShouldReleaseFurniture
    bool OwnerHoldsInPlace{false};
    double HeldMismatchStart{-1.0};
    // Whether a snapshot from the current owner has arrived, the fields about the owner mean nothing before
    bool HasOwnerState{false};
    // The owner has the actor in furniture that is missing or disabled here, see SyncRules::ShouldPlaySeatingAction
    bool OwnerFurnitureMissing{false};
    // That furniture as a local form id when it is here, 0 otherwise
    uint32_t OwnerFurniture{};
    // Where the owner has the actor as local form ids: the world space outside, the cell inside, both 0 when this client
    // doesn't have it
    uint32_t OwnerWorldSpace{};
    uint32_t OwnerCell{};
    double LastSpaceMove{-1e9};

    bool IsOwnerInFurnitureHere() const noexcept { return OwnerHoldsInPlace && !OwnerFurnitureMissing; }
    // See SyncRules::ShouldLeaveFurniture
    bool MayBeInFurniture{true};
    double OwnerOutOfFurnitureSince{-1.0};

    // See NpcSyncRules::ShouldClaimActor
    double LastSnapshot{-1.0};
    double LastClaim{-1e9};
};
