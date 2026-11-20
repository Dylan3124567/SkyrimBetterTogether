#include <Messages/AssignObjectsResponse.h>
#include <Messages/AssignObjectsRequest.h>
#include <TiltedCore/Serialization.hpp>

#include <algorithm>

using TiltedPhoques::Serialization;

void AssignObjectsResponse::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    // A cell can hold more than 255 doors and containers, which the 8 bit count this replaced wrapped around.
    const size_t cCount = std::min(Objects.size(), AssignObjectsRequest::kMaxObjects);
    Serialization::WriteVarInt(aWriter, cCount);

    for (size_t i = 0; i < cCount; ++i)
    {
        Objects[i].Serialize(aWriter);
    }

    const size_t cStateCount = std::min(States.size(), AssignObjectsRequest::kMaxObjects);
    Serialization::WriteVarInt(aWriter, cStateCount);

    for (size_t i = 0; i < cStateCount; ++i)
    {
        const WorldObjectState& state = States[i];
        state.Id.Serialize(aWriter);
        aWriter.WriteBits(state.OpenState, 8);
        Serialization::WriteBool(aWriter, state.HasLock);
        state.Lock.Serialize(aWriter);
    }
}

void AssignObjectsResponse::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);

    const uint64_t cCount = Serialization::ReadVarInt(aReader);
    if (cCount > AssignObjectsRequest::kMaxObjects)
        return;

    Objects.resize(cCount);

    for (auto& object : Objects)
    {
        object.Deserialize(aReader);
    }

    const uint64_t cStateCount = Serialization::ReadVarInt(aReader);
    if (cStateCount > AssignObjectsRequest::kMaxObjects)
        return;

    States.resize(cStateCount);

    for (auto& state : States)
    {
        state.Id.Deserialize(aReader);
        uint64_t openState = 0;
        aReader.ReadBits(openState, 8);
        state.OpenState = openState & 0xFF;
        state.HasLock = Serialization::ReadBool(aReader);
        state.Lock.Deserialize(aReader);
    }
}
