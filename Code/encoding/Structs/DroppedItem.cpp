#include <Structs/DroppedItem.h>
#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

bool DroppedItem::operator==(const DroppedItem& acRhs) const noexcept
{
    return Id == acRhs.Id && Cookie == acRhs.Cookie && CellId == acRhs.CellId && WorldSpaceId == acRhs.WorldSpaceId && Coords == acRhs.Coords && Position == acRhs.Position && Item == acRhs.Item;
}

bool DroppedItem::operator!=(const DroppedItem& acRhs) const noexcept
{
    return !this->operator==(acRhs);
}

void DroppedItem::Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, Id);
    Serialization::WriteVarInt(aWriter, Cookie);
    CellId.Serialize(aWriter);
    WorldSpaceId.Serialize(aWriter);
    Coords.Serialize(aWriter);
    Position.Serialize(aWriter);
    Item.Serialize(aWriter);
}

void DroppedItem::Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    Id = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    Cookie = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    CellId.Deserialize(aReader);
    WorldSpaceId.Deserialize(aReader);
    Coords.Deserialize(aReader);
    Position.Deserialize(aReader);
    Item.Deserialize(aReader);
}
