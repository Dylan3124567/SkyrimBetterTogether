#pragma once

#include <cstdint>
#include "AnimationVariables.h"
#include "CachedString.h"

using TiltedPhoques::String;

struct ActionEvent
{
    // Skyrim's sit and sleep state (SIT_SLEEP_STATE) in bits 14-17 of the actor state flags, see ActorState
    static constexpr uint32_t kFurnitureStateShift = 14;
    static constexpr uint32_t kFurnitureStateMask = 0xFu << kFurnitureStateShift;
    static constexpr uint32_t kWantToStand = 4;
    static constexpr uint32_t kWantToWake = 8;

    // The owner was in furniture or an idle marker when the action started, getting in, out, or using it
    bool IsFurnitureAction() const noexcept { return (State1 & kFurnitureStateMask) != 0; }

    // A furniture action other than getting out: played where the furniture isn't, it sits the actor down there
    bool IsSeatingAction() const noexcept
    {
        const uint32_t cState = (State1 & kFurnitureStateMask) >> kFurnitureStateShift;
        return cState != 0 && cState != kWantToStand && cState != kWantToWake;
    }

    uint64_t Tick{0};
    uint32_t ActorId{0};
    uint32_t ActionId{0};
    uint32_t TargetId{0};
    uint32_t IdleId{0};
    uint32_t State1{0};
    uint32_t State2{0};
    uint32_t Type{0};
    CachedString EventName{};
    CachedString TargetEventName{};
    AnimationVariables Variables{};

    ActionEvent() = default;
    ActionEvent(const ActionEvent& acRhs) = default;
    ActionEvent(ActionEvent&& acRhs) = default;
    ~ActionEvent() = default;
    ActionEvent& operator=(const ActionEvent& acRhs) = default;
    ActionEvent& operator=(ActionEvent&& acRhs) = default;

    bool operator==(const ActionEvent& acRhs) const noexcept;
    bool operator!=(const ActionEvent& acRhs) const noexcept;

    void Load(std::istream&);
    void Save(std::ostream&) const;

    void GenerateDifferential(const ActionEvent& aPrevious, TiltedPhoques::Buffer::Writer& aWriter) const noexcept;
    void ApplyDifferential(TiltedPhoques::Buffer::Reader& aReader) noexcept;
};
