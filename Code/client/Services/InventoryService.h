#pragma once

#include <Structs/DroppedItem.h>

struct World;
struct TransportService;
struct TESObjectREFR;
struct TESObjectCELL;

struct UpdateEvent;
struct NotifyObjectInventoryChanges;
struct NotifyInventoryChanges;
struct InventoryChangeEvent;
struct EquipmentChangeEvent;
struct NotifyEquipmentChanges;
struct DroppedItemEvent;
struct DroppedItemPickUpEvent;
struct DisconnectedEvent;
struct CellChangeEvent;
struct NotifyDroppedItems;
struct NotifyDroppedItemRemoved;

/**
 * @brief Manages inventories of actors and containers.
 *
 * The initial contents of actor inventories are synced through spawn messages through CharacterService
 * @see CharacterService
 * The initial contents of container inventories are synced through the ObjectService
 * @see ObjectService
 * The InventoryService manages any changes to both the current equipment of actors,
 * and the contents of both actor and object inventories.
 */
struct InventoryService
{
    InventoryService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~InventoryService() noexcept = default;

    TP_NOCOPYMOVE(InventoryService);

    /**
     * @brief Check weapon state.
     * @see RunWeaponStateUpdates
     */
    void OnUpdate(const UpdateEvent& acUpdateEvent) noexcept;
    /**
     * @brief Sends out inventory changes made by the local client.
     */
    void OnInventoryChangeEvent(const InventoryChangeEvent& acEvent) noexcept;
    /**
     * @brief Sends out equipment changes made by the local client.
     */
    void OnEquipmentChangeEvent(const EquipmentChangeEvent& acEvent) noexcept;

    /**
     * @brief Applies inventory changes sent by the server.
     */
    void OnNotifyInventoryChanges(const NotifyInventoryChanges& acMessage) noexcept;
    /**
     * @brief Applies equipment changes sent by the server.
     */
    void OnNotifyEquipmentChanges(const NotifyEquipmentChanges& acMessage) noexcept;

    /**
     * @brief Whether a reference stands for an item other clients also show. Safe to call from engine threads.
     */
    bool IsSharedDrop(const TESObjectREFR* apObject) const noexcept;
    /**
     * @brief Asks the server for a shared item instead of picking it up. Safe to call from engine threads.
     * @return true if the item must be left alone for now, false if it isn't shared and the engine can go ahead.
     */
    bool DeferDropPickUp(const TESObjectREFR* apObject, int32_t aCount, bool aUnk1, bool aUnk2) noexcept;

private:
    struct DropCheck
    {
        uint32_t Handle;
        uint32_t CellId;
        glm::vec3 ExpectedPosition;
        std::chrono::steady_clock::time_point Deadline;
        uint32_t Corrections;
        // Sent to the server on the first check, when the item is known to be in its place. Empty from then on.
        Inventory::Entry Item;
    };

    /**
     * @brief An item the server knows about, and this client's reference for it if there is one.
     */
    struct SharedDrop
    {
        DroppedItem Data;
        uint32_t Handle;
        // The dropper's reference is the real item, every other reference is a copy with the base item only.
        bool IsOwnReference;
        bool PickUpRequested;
        int32_t PickUpCount;
        bool PickUpUnk1;
        bool PickUpUnk2;
    };

    void OnDroppedItem(const DroppedItemEvent& acEvent) noexcept;
    void OnDroppedItemPickUp(const DroppedItemPickUpEvent& acEvent) noexcept;
    void OnDisconnected(const DisconnectedEvent& acEvent) noexcept;
    void OnCellChange(const CellChangeEvent& acEvent) noexcept;
    void OnNotifyDroppedItems(const NotifyDroppedItems& acMessage) noexcept;
    void OnNotifyDroppedItemRemoved(const NotifyDroppedItemRemoved& acMessage) noexcept;

    /**
     * Tells the server about an item the local player dropped.
     */
    void ShareDrop(const TESObjectREFR& acItem, const TESObjectCELL& acCell, const Inventory::Entry& acEntry) noexcept;
    /**
     * Creates the references that are missing for items around the player, and forgets the ones that are gone.
     */
    void RunSharedDropUpdates() noexcept;
    void SetSharedReference(uint32_t aHandle, uint32_t aDropId) noexcept;
    void ForgetSharedReference(uint32_t aHandle) noexcept;
    /**
     * Puts dropped items back in front of whoever dropped them if they end up somewhere else.
     * Keeps watching for a few seconds, as physics can still throw an item out of the world after it was placed.
     */
    void RunDropChecks() noexcept;

    /**
     * Checks whether local actors their weapon draw states have changed,
     * and if so, send the new states to the server.
     */
    void RunWeaponStateUpdates() noexcept;

    World& m_world;
    entt::dispatcher& m_dispatcher;
    TransportService& m_transport;

    Vector<DropCheck> m_dropChecks;
    Vector<SharedDrop> m_sharedDrops;
    // References sent to the server, by cookie, that have not been given an ID yet.
    Map<uint32_t, uint32_t> m_pendingDrops;
    uint32_t m_nextDropCookie = 1;
    std::chrono::steady_clock::time_point m_nextSharedDropUpdate{};

    // Handle to ID of everything that may not be picked up without the server, 0 while the ID isn't known yet.
    // Separate from the rest, as the engine asks about this from its own threads.
    mutable std::mutex m_sharedReferencesLock;
    Map<uint32_t, uint32_t> m_sharedReferences;

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_droppedItemConnection;
    entt::scoped_connection m_droppedItemPickUpConnection;
    entt::scoped_connection m_disconnectedConnection;
    entt::scoped_connection m_cellChangeConnection;
    entt::scoped_connection m_notifyDroppedItemsConnection;
    entt::scoped_connection m_notifyDroppedItemRemovedConnection;
    entt::scoped_connection m_inventoryConnection;
    entt::scoped_connection m_equipmentConnection;
    entt::scoped_connection m_inventoryChangeConnection;
    entt::scoped_connection m_equipmentChangeConnection;
};
