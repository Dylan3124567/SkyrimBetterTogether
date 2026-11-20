#pragma once

#include "Message.h"
#include <Structs/ObjectData.h>
#include <Structs/WorldObjectState.h>

using TiltedPhoques::Vector;

struct AssignObjectsRequest final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kAssignObjectsRequest;
    // Far more doors and containers than one cell holds; a count above it is a broken or hostile packet
    static constexpr size_t kMaxObjects = WorldObjectRules::kMaxObjectsPerCell;

    AssignObjectsRequest()
        : ClientMessage(Opcode)
    {
    }

    virtual ~AssignObjectsRequest() = default;

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const AssignObjectsRequest& acRhs) const noexcept { return Objects == acRhs.Objects && GetOpcode() == acRhs.GetOpcode(); }

    Vector<ObjectData> Objects{};
};
