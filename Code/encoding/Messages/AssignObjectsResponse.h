#pragma once

#include "Message.h"

#include <Structs/ObjectData.h>
#include <Structs/WorldObjectState.h>

using TiltedPhoques::Vector;

struct AssignObjectsResponse final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kAssignObjectsResponse;

    AssignObjectsResponse()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const AssignObjectsResponse& achRhs) const noexcept { return GetOpcode() == achRhs.GetOpcode() && Objects == achRhs.Objects && States == achRhs.States; }

    Vector<ObjectData> Objects{};
    // What the server remembers of the requested objects, applied after Objects
    Vector<WorldObjectState> States{};
};
