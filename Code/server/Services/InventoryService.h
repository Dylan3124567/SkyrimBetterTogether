#pragma once

#include <Events/PacketEvent.h>
#include <Structs/DropLedger.h>

struct World;
struct UpdateEvent;
struct RequestObjectInventoryChanges;
struct RequestInventoryChanges;
struct RequestEquipmentChanges;
struct DrawWeaponRequest;
struct PlayerLeaveCellEvent;
struct PlayerLeaveEvent;
struct RequestItemDrop;
struct RequestDroppedItems;
struct RequestDroppedItemPickUp;

/**
 * @brief Relays inventory/equipment changes and updates the server side state.
 */
class InventoryService
{
public:
    InventoryService(World& aWorld, entt::dispatcher& aDispatcher);

    /**
     * @brief Relays inventory changes to other clients and updates server side inventories.
     */
    void OnInventoryChanges(const PacketEvent<RequestInventoryChanges>& acMessage) noexcept;
    /**
     * @brief Relays equipment changes to other clients and updates server side equipment.
     */
    void OnEquipmentChanges(const PacketEvent<RequestEquipmentChanges>& acMessage) noexcept;
    /**
     * @brief Relays weapon draw changes to other clients and updates server side weapon draw state.
     */
    void OnWeaponDrawnRequest(const PacketEvent<DrawWeaponRequest>& acMessage) noexcept;
    /**
     * @brief Records a dropped item and tells the players around it.
     */
    void OnItemDrop(const PacketEvent<RequestItemDrop>& acMessage) noexcept;
    /**
     * @brief Sends a player the dropped items around the cell it just entered.
     */
    void OnDroppedItemsRequest(const PacketEvent<RequestDroppedItems>& acMessage) const noexcept;
    /**
     * @brief Gives a dropped item to the first player asking for it.
     */
    void OnDroppedItemPickUp(const PacketEvent<RequestDroppedItemPickUp>& acMessage) noexcept;
    /**
     * @brief Takes the items of a leaving player out of the world of everyone else.
     */
    void OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept;

private:
    World& m_world;
    DropLedger m_droppedItems;

    entt::scoped_connection m_inventoryChangeConnection;
    entt::scoped_connection m_equipmentChangeConnection;
    entt::scoped_connection m_drawWeaponConnection;
    entt::scoped_connection m_itemDropConnection;
    entt::scoped_connection m_droppedItemsConnection;
    entt::scoped_connection m_droppedItemPickUpConnection;
    entt::scoped_connection m_playerLeaveConnection;
};
