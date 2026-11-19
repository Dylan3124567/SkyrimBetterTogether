#pragma once

#include "Message.h"
#include <Structs/Vector3_NetQuantize.h>
#include <Structs/GameId.h>

struct NotifySetWaypoint final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifySetWaypoint;

    NotifySetWaypoint() : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifySetWaypoint& acRhs) const noexcept
    {
        return GetOpcode() == acRhs.GetOpcode() && PlayerId == acRhs.PlayerId && Position == acRhs.Position && WorldSpaceFormID == acRhs.WorldSpaceFormID;
    }

    // Whose waypoint it is, everyone in the party has one of their own
    uint32_t PlayerId{};
    Vector3_NetQuantize Position;
    GameId WorldSpaceFormID;
};
