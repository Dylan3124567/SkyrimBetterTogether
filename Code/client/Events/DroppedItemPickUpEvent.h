#pragma once

/**
 * @brief The local player tried to pick up an item other clients also show. The engine's arguments are kept for when the
 * server has said the item is theirs.
 */
struct DroppedItemPickUpEvent
{
    DroppedItemPickUpEvent(uint32_t aHandle, int32_t aCount, bool aUnk1, bool aUnk2)
        : Handle(aHandle)
        , Count(aCount)
        , Unk1(aUnk1)
        , Unk2(aUnk2)
    {
    }

    uint32_t Handle;
    int32_t Count;
    bool Unk1;
    bool Unk2;
};
