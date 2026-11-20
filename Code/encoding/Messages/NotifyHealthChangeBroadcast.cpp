#include <Messages/NotifyHealthChangeBroadcast.h>

void NotifyHealthChangeBroadcast::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, Id);
    Serialization::WriteFloat(aWriter, DeltaHealth);
    Serialization::WriteBool(aWriter, NonLethal);
    Serialization::WriteVarInt(aWriter, HitterId);
}

void NotifyHealthChangeBroadcast::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);

    Id = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    DeltaHealth = Serialization::ReadFloat(aReader);
    NonLethal = Serialization::ReadBool(aReader);
    HitterId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
}
