#pragma once

#include "Message.h"
#include <Structs/DeathMemory.h>

struct RequestDeathStateChange final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestDeathStateChange;

    RequestDeathStateChange()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestDeathStateChange& acRhs) const noexcept
    {
        return Id == acRhs.Id && OwnershipEpoch == acRhs.OwnershipEpoch && IsDead == acRhs.IsDead && IsDown == acRhs.IsDown && Health == acRhs.Health && KillerId == acRhs.KillerId && GetOpcode() == acRhs.GetOpcode();
    }

    uint32_t Id;
    uint32_t OwnershipEpoch{};
    bool IsDead;
    // Bleeding out, or knocked down as an essential actor. Copies go down and get up with their owner's actor.
    bool IsDown{};
    // The sender's health for the actor, which a copy gets up with
    float Health{};
    // Server id of the actor that killed it, DeathMemory::cNoActor if unknown. The server only passes on one the sender may name.
    uint32_t KillerId{DeathMemory::cNoActor};
};
