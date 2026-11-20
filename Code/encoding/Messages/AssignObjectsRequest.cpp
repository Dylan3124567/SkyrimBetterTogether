#include <Messages/AssignObjectsRequest.h>
#include <TiltedCore/Serialization.hpp>

#include <algorithm>

using TiltedPhoques::Serialization;

void AssignObjectsRequest::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    // A cell can hold more than 255 doors and containers, which the 8 bit count this replaced wrapped around.
    const size_t cCount = std::min(Objects.size(), kMaxObjects);
    Serialization::WriteVarInt(aWriter, cCount);

    for (size_t i = 0; i < cCount; ++i)
    {
        Objects[i].Serialize(aWriter);
    }
}

void AssignObjectsRequest::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);

    const uint64_t cCount = Serialization::ReadVarInt(aReader);
    if (cCount > kMaxObjects)
        return;

    Objects.resize(cCount);

    for (auto& object : Objects)
    {
        object.Deserialize(aReader);
    }
}
