#pragma once

#include <Structs/GameId.h>

#include <Structs/Vector3_NetQuantize.h>
#include <Structs/Rotator2_NetQuantize.h>
#include <Structs/AnimationVariables.h>

using TiltedPhoques::Buffer;

struct Movement
{
    Movement() = default;
    ~Movement() = default;

    bool operator==(const Movement& acRhs) const noexcept;
    bool operator!=(const Movement& acRhs) const noexcept;

    void Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept;
    void Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept;

    GameId CellId{};
    uint32_t OwnershipEpoch{};
    GameId WorldSpaceId{};
    Vector3_NetQuantize Position{};
    Rotator2_NetQuantize Rotation{};
    AnimationVariables Variables{};
    float Direction{};
    // The owner has the actor in furniture or restrained, remote copies can't work this out from the animations alone
    bool IsHeldInPlace{};
    // The furniture or idle marker the owner has the actor in, only sent while held, empty for temporary references
    GameId FurnitureId{};
};
