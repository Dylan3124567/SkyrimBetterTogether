#include <Messages/NotifyPlayerPositions.h>
#include <TiltedCore/Serialization.hpp>

void NotifyPlayerPositions::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, Entries.size());

    for (const auto& entry : Entries)
    {
        Serialization::WriteVarInt(aWriter, entry.PlayerId);
        entry.WorldSpaceId.Serialize(aWriter);
        entry.CellId.Serialize(aWriter);
        Serialization::WriteFloat(aWriter, entry.Position.x);
        Serialization::WriteFloat(aWriter, entry.Position.y);
        Serialization::WriteFloat(aWriter, entry.Position.z);
    }
}

void NotifyPlayerPositions::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);

    const auto count = Serialization::ReadVarInt(aReader) & 0xFFFF;

    Entries.resize(count);
    for (auto& entry : Entries)
    {
        entry.PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
        entry.WorldSpaceId.Deserialize(aReader);
        entry.CellId.Deserialize(aReader);
        entry.Position.x = Serialization::ReadFloat(aReader);
        entry.Position.y = Serialization::ReadFloat(aReader);
        entry.Position.z = Serialization::ReadFloat(aReader);
    }
}
