#pragma once

#include <Structs/Inventory.h>

/**
 * @brief An actor dropped an item into the world. Item is empty for a drop other clients should not hear about.
 */
struct DroppedItemEvent
{
    DroppedItemEvent(uint32_t aHandle, uint32_t aCellId, const glm::vec3& acExpectedPosition, Inventory::Entry aItem)
        : Handle(aHandle)
        , CellId(aCellId)
        , ExpectedPosition(acExpectedPosition)
        , Item(std::move(aItem))
    {
    }

    uint32_t Handle;
    uint32_t CellId;
    glm::vec3 ExpectedPosition;
    Inventory::Entry Item;
};
