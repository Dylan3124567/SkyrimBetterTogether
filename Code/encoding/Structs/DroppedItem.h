#pragma once

#include <Structs/GameId.h>
#include <Structs/GridCellCoords.h>
#include <Structs/Inventory.h>
#include <Structs/Vector3_NetQuantize.h>

/**
 * @brief One item lying in the world. Every client shows its own reference for it, which is a temporary form that only
 * means something on that client, so clients have to refer to the item through Id.
 */
struct DroppedItem
{
    bool operator==(const DroppedItem& acRhs) const noexcept;
    bool operator!=(const DroppedItem& acRhs) const noexcept;

    void Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept;
    void Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept;

    uint32_t Id{};
    // Only set on the copy sent back to the dropper, who already has the real reference and must not create another.
    uint32_t Cookie{};
    GameId CellId{};
    GameId WorldSpaceId{};
    GridCellCoords Coords{};
    Vector3_NetQuantize Position{};
    Inventory::Entry Item{};
};
