#pragma once

#include "Message.h"
#include <Structs/DeathMemory.h>

struct NotifyHealthChangeBroadcast final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyHealthChangeBroadcast;

    NotifyHealthChangeBroadcast()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyHealthChangeBroadcast& acRhs) const noexcept
    {
        return Id == acRhs.Id && DeltaHealth == acRhs.DeltaHealth && NonLethal == acRhs.NonLethal && HitterId == acRhs.HitterId && GetOpcode() == acRhs.GetOpcode();
    }

    uint32_t Id;
    float DeltaHealth;
    // The sender's game would only knock the actor down (essential there), so its owner must not kill it
    bool NonLethal{};
    // Server id of the sender's player character, set by the server, DeathMemory::cNoActor if it has none
    uint32_t HitterId{DeathMemory::cNoActor};
};
