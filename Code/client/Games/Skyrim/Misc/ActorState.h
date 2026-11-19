#pragma once

#include <Misc/IMovementState.h>

struct ActorState : IMovementState
{
    // Bit fields of flags1, the engine's own getters read them the same way
    static constexpr uint32_t kSitSleepStateMask = 0xFu << 14;
    static constexpr uint32_t kLifeStateMask = 0xFu << 21;
    static constexpr uint32_t kLifeStateRestrained = 6u << 21;
    static constexpr uint32_t kKnockStateMask = 0x7u << 25;

    virtual ~ActorState();

    uint32_t flags1;
    uint32_t flags2;

    bool IsWeaponDrawn() const noexcept { return (flags2 >> 5 & 7) >= 3; }

    bool IsWeaponFullyDrawn() const noexcept { return (flags2 >> 5 & 7) == 3; }

    bool IsBleedingOut() const noexcept { return (flags1 & 0x1E00000) == 0x1000000 || (flags1 & 0x1E00000) == 0xE00000; }

    bool IsRestrained() const noexcept { return (flags1 & kLifeStateMask) == kLifeStateRestrained; }

    bool IsKnockedDown() const noexcept { return (flags1 & kKnockStateMask) != 0; }

    // Sitting, sleeping or on the way in or out of either, this covers all furniture and idle marker use
    bool IsUsingFurniture() const noexcept { return (flags1 & kSitSleepStateMask) != 0; }

    bool IsHeldInPlace() const noexcept { return IsUsingFurniture() || IsRestrained(); }

    void ClearFurnitureState() noexcept
    {
        flags1 &= ~kSitSleepStateMask;
        if (IsRestrained())
            flags1 &= ~kLifeStateMask;
    }

    void ClearDownedState() noexcept { flags1 &= ~(kLifeStateMask | kKnockStateMask); }

    bool SetWeaponDrawn(bool aDraw) noexcept;
};
