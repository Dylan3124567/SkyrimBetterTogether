#include <Services/ObjectService.h>

#include <GameServer.h>
#include <World.h>
#include <Components.h>

#include <Events/PlayerLeaveCellEvent.h>
#include <Events/UpdateEvent.h>

#include <Messages/ActivateRequest.h>
#include <Messages/NotifyActivate.h>
#include <Messages/LockChangeRequest.h>
#include <Messages/NotifyLockChange.h>
#include <Messages/AssignObjectsRequest.h>
#include <Messages/AssignObjectsResponse.h>
#include <Messages/ScriptAnimationRequest.h>
#include <Messages/NotifyScriptAnimation.h>

#include <unordered_set>

ObjectService::ObjectService(World& aWorld, entt::dispatcher& aDispatcher)
    : m_world(aWorld)
{
    m_updateConnection = aDispatcher.sink<UpdateEvent>().connect<&ObjectService::OnUpdate>(this);
    m_leaveCellConnection = aDispatcher.sink<PlayerLeaveCellEvent>().connect<&ObjectService::OnPlayerLeaveCellEvent>(this);
    m_objectDestroyedConnection = m_world.on_destroy<ObjectComponent>().connect<&ObjectService::OnObjectDestroyed>(this);
    m_assignObjectConnection = aDispatcher.sink<PacketEvent<AssignObjectsRequest>>().connect<&ObjectService::OnAssignObjectsRequest>(this);
    m_activateConnection = aDispatcher.sink<PacketEvent<ActivateRequest>>().connect<&ObjectService::OnActivate>(this);
    m_lockChangeConnection = aDispatcher.sink<PacketEvent<LockChangeRequest>>().connect<&ObjectService::OnLockChange>(this);
    m_scriptAnimationConnection = aDispatcher.sink<PacketEvent<ScriptAnimationRequest>>().connect<&ObjectService::OnScriptAnimationRequest>(this);
}

// The client sends its EnterCell request right after the objects, in the same cell change
static constexpr uint64_t kPendingAssignmentMs = 10000;

void ObjectService::OnUpdate(const UpdateEvent&) noexcept
{
    if (m_pendingAssignments.empty())
        return;

    const uint64_t cNow = GameServer::Get()->GetTick();
    for (auto it = m_pendingAssignments.begin(); it != m_pendingAssignments.end();)
    {
        Player* pPlayer = m_world.GetPlayerManager().GetById(it->first);
        if (!pPlayer || cNow >= it->second.Expiry)
        {
            it = m_pendingAssignments.erase(it);
            continue;
        }
        if (pPlayer->GetCellComponent().Cell != it->second.Objects.front().CellId)
        {
            ++it;
            continue;
        }
        const auto objects = std::move(it->second.Objects);
        it = m_pendingAssignments.erase(it);
        AssignObjects(pPlayer, objects);
    }
}

void ObjectService::OnObjectDestroyed(entt::registry&, entt::entity aEntity) noexcept
{
    m_objects.Remove(aEntity);
}

// TODO(cosideci): the cell handling of objects need to be revamped.
// We already store the location and worldspace of the mod through CellIdComponent.
// Clients need a message saying the entity was destroyed.
void ObjectService::OnPlayerLeaveCellEvent(const PlayerLeaveCellEvent& acEvent) noexcept
{
    for (Player* pPlayer : m_world.GetPlayerManager())
    {
        if (pPlayer->GetCellComponent().Cell == acEvent.OldCell)
            return;
    }

    auto objectView = m_world.view<ObjectComponent, CellIdComponent>();
    Vector<entt::entity> toDestroy;

    for (auto entity : objectView)
    {
        const auto& cellIdComponent = objectView.get<CellIdComponent>(entity);

        if (cellIdComponent.Cell != acEvent.OldCell)
            continue;

        toDestroy.push_back(entity);
    }

    for (auto& entity : toDestroy)
    {
        m_world.destroy(entity);
    }
}

void ObjectService::OnAssignObjectsRequest(const PacketEvent<AssignObjectsRequest>& acMessage) noexcept
{
    const auto& objects = acMessage.Packet.Objects;
    if (objects.empty() || !objects.front().CellId)
        return;

    Player* pPlayer = acMessage.pPlayer;
    // A newer cell supersedes what still waited for an older one
    m_pendingAssignments.erase(pPlayer->GetId());

    if (pPlayer->GetCellComponent().Cell == objects.front().CellId)
    {
        AssignObjects(pPlayer, objects);
        return;
    }

    // Client ObjectService and PlayerService both answer the same CellChangeEvent and ObjectService is created first
    // (client World.cpp), so the objects arrive before the server knows the sender entered their cell.
    m_pendingAssignments[pPlayer->GetId()] = PendingAssignment{objects, GameServer::Get()->GetTick() + kPendingAssignmentMs};
}

// NOTE: this whole system kinda relies on all objects in a cell being static.
// This is fine for containers and doors, but if this system is expanded, think of temporaries.
// Only objects of the cell the sender is in: anything else is a claim the server can't check, and an entity for it would
// stay until someone happened to leave that cell.
void ObjectService::AssignObjects(Player* apPlayer, const TiltedPhoques::Vector<ObjectData>& acObjects) noexcept
{
    const GameId cCell = apPlayer->GetCellComponent().Cell;
    AssignObjectsResponse response;

    for (const ObjectData& object : acObjects)
    {
        if (object.CellId != cCell || !object.Id)
            continue;

        if (const auto existing = m_objects.Find(object.Id))
        {
            if (!WorldObjectRules::InSendersCell(m_objects.CellOf(object.Id), cCell))
                continue;

            const auto* pObjectComponent = m_world.try_get<ObjectComponent>(*existing);
            const auto* pInventoryComponent = m_world.try_get<InventoryComponent>(*existing);
            if (!pObjectComponent || !pInventoryComponent)
                continue;

            ObjectData objectData;
            objectData.ServerId = World::ToInteger(*existing);
            objectData.Id = object.Id;
            objectData.CurrentLockData = pObjectComponent->CurrentLockData;
            objectData.CurrentInventory = pInventoryComponent->Content;
            objectData.IsSenderFirst = false;

            response.Objects.push_back(objectData);
        }
        else
        {
            if (m_objects.Full())
                FreeUnoccupiedObjects();
            if (m_objects.Full() || m_objects.CellFull(cCell))
                continue;

            const auto cEntity = m_world.create();

            m_world.emplace<FormIdComponent>(cEntity, object.Id);

            auto& objectComponent = m_world.emplace<ObjectComponent>(cEntity, apPlayer);
            objectComponent.CurrentLockData = object.CurrentLockData;

            m_world.emplace<CellIdComponent>(cEntity, object.CellId, object.WorldSpaceId, object.CurrentCoords);
            auto& inventoryComp = m_world.emplace<InventoryComponent>(cEntity);
            inventoryComp.Content = object.CurrentInventory;

            m_objects.Add(object.Id, cCell, cEntity);

            ObjectData objectData;
            objectData.Id = object.Id;
            objectData.ServerId = World::ToInteger(cEntity);
            objectData.IsSenderFirst = true;

            response.Objects.push_back(objectData);
        }

        if (auto state = m_objectMemory.TakeFor(object.Id, apPlayer->GetId()))
            response.States.push_back(*state);
    }

    if (!response.Objects.empty())
        apPlayer->Send(response);
}

// Exterior cells never send a leave event, so their objects otherwise stay; freed only once the ceiling is reached.
void ObjectService::FreeUnoccupiedObjects() noexcept
{
    std::unordered_set<GameId> occupied;
    for (const Player* pPlayer : m_world.GetPlayerManager())
        occupied.insert(pPlayer->GetCellComponent().Cell);

    Vector<entt::entity> toDestroy;
    auto objectView = m_world.view<ObjectComponent, CellIdComponent>();
    for (auto entity : objectView)
        if (!occupied.contains(objectView.get<CellIdComponent>(entity).Cell))
            toDestroy.push_back(entity);

    spdlog::info("Object entity ceiling reached, freeing {} objects in cells nobody is in", toDestroy.size());
    for (auto entity : toDestroy)
        m_world.destroy(entity);
}

bool ObjectService::InSendersCell(const GameId& acObjectId, const Player* apSender) const noexcept
{
    return WorldObjectRules::InSendersCell(m_objects.CellOf(acObjectId), apSender->GetCellComponent().Cell);
}

// The players a change in this cell reaches live: the sender and everyone the relay goes to
TiltedPhoques::Vector<uint32_t> ObjectService::PlayersIn(const GameId& acCellId, const Player* apSender) const noexcept
{
    TiltedPhoques::Vector<uint32_t> players{apSender->GetId()};
    for (Player* pPlayer : m_world.GetPlayerManager())
        if (pPlayer != apSender && pPlayer->GetCellComponent().Cell == acCellId)
            players.push_back(pPlayer->GetId());
    return players;
}

void ObjectService::OnActivate(const PacketEvent<ActivateRequest>& acMessage) noexcept
{
    const auto cActivator = static_cast<entt::entity>(acMessage.Packet.ActivatorId);
    const auto* pOwner = m_world.try_get<OwnerComponent>(cActivator);
    if (!pOwner || pOwner->GetOwner() != acMessage.pPlayer)
        return;

    // Only the sender's own character (what NPCs open is closed again by their AI without an activation), and only an
    // object the server has in the cell the sender is in.
    if (acMessage.pPlayer->GetCharacter() == cActivator && InSendersCell(acMessage.Packet.Id, acMessage.pPlayer))
        m_objectMemory.RecordActivation(acMessage.Packet.Id, acMessage.Packet.PreActivationOpenState,
                                        PlayersIn(acMessage.pPlayer->GetCellComponent().Cell, acMessage.pPlayer));

    NotifyActivate notifyActivate;
    notifyActivate.Id = acMessage.Packet.Id;
    notifyActivate.ActivatorId = acMessage.Packet.ActivatorId;
    notifyActivate.PreActivationOpenState = acMessage.Packet.PreActivationOpenState;

    for (auto pPlayer : m_world.GetPlayerManager())
    {
        if (pPlayer != acMessage.pPlayer && pPlayer->GetCellComponent().Cell == acMessage.Packet.CellId)
        {
            pPlayer->Send(notifyActivate);
        }
    }
}

void ObjectService::OnLockChange(const PacketEvent<LockChangeRequest>& acMessage) noexcept
{
    NotifyLockChange notifyLockChange;
    notifyLockChange.Id = acMessage.Packet.Id;
    notifyLockChange.IsLocked = acMessage.Packet.IsLocked;
    notifyLockChange.LockLevel = acMessage.Packet.LockLevel;

    // What the server keeps for later visitors only comes from a player in the cell the server has the object in
    const auto object = m_objects.Find(acMessage.Packet.Id);
    if (object && InSendersCell(acMessage.Packet.Id, acMessage.pPlayer))
    {
        LockData lock;
        lock.IsLocked = acMessage.Packet.IsLocked;
        lock.LockLevel = acMessage.Packet.LockLevel;

        if (auto* pObjectComponent = m_world.try_get<ObjectComponent>(*object))
            pObjectComponent->CurrentLockData = lock;

        m_objectMemory.RecordLock(acMessage.Packet.Id, lock, PlayersIn(acMessage.pPlayer->GetCellComponent().Cell, acMessage.pPlayer));
    }

    for (Player* pPlayer : m_world.GetPlayerManager())
    {
        if (pPlayer == acMessage.pPlayer)
            continue;

        if (pPlayer->GetCellComponent().Cell == acMessage.Packet.CellId)
            pPlayer->Send(notifyLockChange);
    }
}

void ObjectService::OnScriptAnimationRequest(const PacketEvent<ScriptAnimationRequest>& acMessage) noexcept
{
    auto& packet = acMessage.Packet;

    NotifyScriptAnimation message{};
    message.FormID = packet.FormID;
    message.Animation = packet.Animation;
    message.EventName = packet.EventName;

    for (Player* pPlayer : m_world.GetPlayerManager())
    {
        pPlayer->Send(message);
    }
}
