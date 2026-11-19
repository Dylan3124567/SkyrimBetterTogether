#include <Messages/PlayerAppearance.h>

void PlayerAppearance::Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, ActorId);
    Serialization::WriteVarInt(aWriter, OwnershipEpoch);
    Serialization::WriteVarInt(aWriter, ChangeFlags);
    Serialization::WriteString(aWriter, AppearanceBuffer);
    FaceTints.Serialize(aWriter);
}

void PlayerAppearance::Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ActorId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    OwnershipEpoch = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    ChangeFlags = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    AppearanceBuffer = Serialization::ReadString(aReader);
    FaceTints.Entries.clear();
    FaceTints.Deserialize(aReader);
}
