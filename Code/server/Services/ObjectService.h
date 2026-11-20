#pragma once

#include <Events/PacketEvent.h>
#include <Structs/ObjectData.h>
#include <Structs/WorldObjectState.h>

struct World;
struct Player;
struct UpdateEvent;
struct PlayerLeaveCellEvent;
struct ActivateRequest;
struct LockChangeRequest;
struct AssignObjectsRequest;
struct ScriptAnimationRequest;

/**
 * @brief Manages (interactive) objects and relays interactions with said objects.
 */
class ObjectService
{
public:
    ObjectService(World& aWorld, entt::dispatcher& aDispatcher);

private:
    void OnUpdate(const UpdateEvent&) noexcept;
    void OnPlayerLeaveCellEvent(const PlayerLeaveCellEvent& acEvent) noexcept;
    void OnObjectDestroyed(entt::registry& aRegistry, entt::entity aEntity) noexcept;
    void OnAssignObjectsRequest(const PacketEvent<AssignObjectsRequest>&) noexcept;
    void AssignObjects(Player* apPlayer, const TiltedPhoques::Vector<ObjectData>& acObjects) noexcept;
    void FreeUnoccupiedObjects() noexcept;
    void OnActivate(const PacketEvent<ActivateRequest>&) noexcept;
    void OnLockChange(const PacketEvent<LockChangeRequest>&) noexcept;
    void OnScriptAnimationRequest(const PacketEvent<ScriptAnimationRequest>&) noexcept;
    [[nodiscard]] bool InSendersCell(const GameId& acObjectId, const Player* apSender) const noexcept;
    TiltedPhoques::Vector<uint32_t> PlayersIn(const GameId& acCellId, const Player* apSender) const noexcept;

    // A client reports the objects of a cell before the cell itself, so they wait here until it is in that cell
    struct PendingAssignment
    {
        TiltedPhoques::Vector<ObjectData> Objects;
        uint64_t Expiry{};
    };

    World& m_world;
    WorldObjectIndex<entt::entity> m_objects;
    // Outlives the object entities, which go with the last player in their cell
    WorldObjectMemory m_objectMemory;
    // By player id, the latest request only
    std::unordered_map<uint32_t, PendingAssignment> m_pendingAssignments;

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_leaveCellConnection;
    entt::scoped_connection m_objectDestroyedConnection;
    entt::scoped_connection m_assignObjectConnection;
    entt::scoped_connection m_activateConnection;
    entt::scoped_connection m_lockChangeConnection;
    entt::scoped_connection m_scriptAnimationConnection;
};
