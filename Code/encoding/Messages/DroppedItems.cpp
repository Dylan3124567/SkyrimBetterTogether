#include <Messages/DroppedItems.h>
#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void RequestItemDrop::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Item.Serialize(aWriter);
}

void RequestItemDrop::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);

    Item.Deserialize(aReader);
}

void RequestDroppedItems::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    CellId.Serialize(aWriter);
    WorldSpaceId.Serialize(aWriter);
    Coords.Serialize(aWriter);
}

void RequestDroppedItems::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);

    CellId.Deserialize(aReader);
    WorldSpaceId.Deserialize(aReader);
    Coords.Deserialize(aReader);
}

void RequestDroppedItemPickUp::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, Id);
}

void RequestDroppedItemPickUp::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);

    Id = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
}

void NotifyDroppedItems::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, Items.size());

    for (const auto& item : Items)
        item.Serialize(aWriter);
}

void NotifyDroppedItems::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);

    Items.resize(Serialization::ReadVarInt(aReader));

    for (auto& item : Items)
        item.Deserialize(aReader);
}

void NotifyDroppedItemRemoved::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, Id);
    Serialization::WriteBool(aWriter, Granted);
}

void NotifyDroppedItemRemoved::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);

    Id = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    Granted = Serialization::ReadBool(aReader);
}
