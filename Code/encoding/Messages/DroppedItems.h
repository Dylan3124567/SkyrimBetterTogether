#pragma once
#include "Message.h"
#include <Structs/DroppedItem.h>

struct RequestItemDrop final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestItemDrop;
    RequestItemDrop() : ClientMessage(Opcode) {}
    void SerializeRaw(TiltedPhoques::Buffer::Writer&) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader&) noexcept override;
    bool operator==(const RequestItemDrop& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Item == acRhs.Item; }

    DroppedItem Item{};
};

/**
 * @brief Asks for the items around a cell the player just entered, so that drops made while away show up.
 */
struct RequestDroppedItems final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestDroppedItems;
    RequestDroppedItems() : ClientMessage(Opcode) {}
    void SerializeRaw(TiltedPhoques::Buffer::Writer&) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader&) noexcept override;
    bool operator==(const RequestDroppedItems& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && CellId == acRhs.CellId && WorldSpaceId == acRhs.WorldSpaceId && Coords == acRhs.Coords; }

    GameId CellId{};
    GameId WorldSpaceId{};
    GridCellCoords Coords{};
};

struct RequestDroppedItemPickUp final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestDroppedItemPickUp;
    RequestDroppedItemPickUp() : ClientMessage(Opcode) {}
    void SerializeRaw(TiltedPhoques::Buffer::Writer&) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader&) noexcept override;
    bool operator==(const RequestDroppedItemPickUp& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Id == acRhs.Id; }

    uint32_t Id{};
};

struct NotifyDroppedItems final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyDroppedItems;
    NotifyDroppedItems() : ServerMessage(Opcode) {}
    void SerializeRaw(TiltedPhoques::Buffer::Writer&) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader&) noexcept override;
    bool operator==(const NotifyDroppedItems& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Items == acRhs.Items; }

    TiltedPhoques::Vector<DroppedItem> Items{};
};

/**
 * @brief An item is gone. Granted tells the player who asked for it that it is theirs.
 */
struct NotifyDroppedItemRemoved final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyDroppedItemRemoved;
    NotifyDroppedItemRemoved() : ServerMessage(Opcode) {}
    void SerializeRaw(TiltedPhoques::Buffer::Writer&) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader&) noexcept override;
    bool operator==(const NotifyDroppedItemRemoved& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Id == acRhs.Id && Granted == acRhs.Granted; }

    uint32_t Id{};
    bool Granted = false;
};
