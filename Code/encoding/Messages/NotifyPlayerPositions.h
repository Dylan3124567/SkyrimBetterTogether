#pragma once

#include "Message.h"

#include <Structs/GameId.h>

using TiltedPhoques::Vector;

// Character movement is only relayed in range, so this is the one source for where a distant player is.
struct NotifyPlayerPositions final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyPlayerPositions;

    struct Entry
    {
        bool operator==(const Entry& acRhs) const noexcept { return PlayerId == acRhs.PlayerId && WorldSpaceId == acRhs.WorldSpaceId && CellId == acRhs.CellId && Position == acRhs.Position; }

        uint32_t PlayerId{};
        GameId WorldSpaceId{};
        GameId CellId{};
        glm::vec3 Position{};
    };

    NotifyPlayerPositions()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyPlayerPositions& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Entries == acRhs.Entries; }

    Vector<Entry> Entries{};
};
