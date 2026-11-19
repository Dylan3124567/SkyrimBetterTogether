#pragma once

#ifndef TP_INTERNAL_COMPONENTS_GUARD
#error Include Components.h instead
#endif

#include <Structs/AnimationVariables.h>

struct MovementComponent
{
    uint64_t Tick;
    glm::vec3 Position;
    glm::vec3 Rotation;
    AnimationVariables Variables;
    float Direction;
    // In the server's own ticks, Tick is the owner's
    uint64_t LastReceived{};
    bool IsHeldInPlace{};
};
