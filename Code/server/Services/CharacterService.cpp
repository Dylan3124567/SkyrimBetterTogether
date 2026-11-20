#include <Services/CharacterService.h>
#include <Components.h>
#include <GameServer.h>
#include <World.h>

#include <Events/CharacterSpawnedEvent.h>
#include <Events/CharacterExteriorCellChangeEvent.h>
#include <Events/CharacterInteriorCellChangeEvent.h>
#include <Events/PlayerEnterWorldEvent.h>
#include <Events/UpdateEvent.h>
#include <Events/CharacterRemoveEvent.h>
#include <Events/OwnershipTransferEvent.h>

#include <Game/OwnerView.h>

#include <Messages/AssignCharacterRequest.h>
#include <Messages/AssignCharacterResponse.h>
#include <Messages/ServerReferencesMoveRequest.h>
#include <Messages/ClientReferencesMoveRequest.h>
#include <Messages/RequestActionEvents.h>
#include <Messages/NotifyActionEvents.h>
#include <Messages/CharacterSpawnRequest.h>
#include <Messages/RequestFactionsChanges.h>
#include <Messages/NotifyFactionsChanges.h>
#include <Messages/NotifyRemoveCharacter.h>
#include <Messages/RequestOwnershipTransfer.h>
#include <Messages/NotifyOwnershipTransfer.h>
#include <Messages/RequestOwnershipClaim.h>
#include <Messages/MountRequest.h>
#include <Messages/NotifyMount.h>
#include <Messages/NewPackageRequest.h>
#include <Messages/NotifyNewPackage.h>
#include <Messages/RequestRespawn.h>
#include <Messages/PlayerAppearance.h>
#include <Messages/NotifyRespawn.h>
#include <Messages/SyncExperienceRequest.h>
#include <Messages/NotifySyncExperience.h>
#include <Messages/DialogueRequest.h>
#include <Messages/NotifyDialogue.h>
#include <Messages/SubtitleRequest.h>
#include <Messages/NotifySubtitle.h>
#include <Messages/NotifyActorTeleport.h>

#include <Services/ActorValueService.h>
#include <Structs/DeathMemory.h>
#include <Structs/QuestRole.h>

#include <Setting.h>
#include <cmath>
namespace
{
Console::Setting bEnableXpSync{"Gameplay:bEnableXpSync", "Syncs combat XP within the party", true};

// Snapshots come every ~90 ms at worst, a client doesn't claim before its own wait of 5 seconds runs out either
constexpr uint64_t cOwnerSilentTime = 3000;
constexpr uint64_t cDialogueTransferInterval = 10000;

// Good enough to tell days apart, months are short of 31 days only by a day or two
int64_t GetGameHour(const CalendarService& acCalendar) noexcept
{
    const auto [day, month, year] = acCalendar.GetDate();
    return ((static_cast<int64_t>(year) * 12 + month) * 31 + day) * 24 + acCalendar.GetTime().first;
}
} // namespace

CharacterService::CharacterService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
    , m_updateConnection(aDispatcher.sink<UpdateEvent>().connect<&CharacterService::OnUpdate>(this))
    , m_interiorCellChangeEventConnection(aDispatcher.sink<CharacterInteriorCellChangeEvent>().connect<&CharacterService::OnCharacterInteriorCellChange>(this))
    , m_exteriorCellChangeEventConnection(aDispatcher.sink<CharacterExteriorCellChangeEvent>().connect<&CharacterService::OnCharacterExteriorCellChange>(this))
    , m_characterAssignRequestConnection(aDispatcher.sink<PacketEvent<AssignCharacterRequest>>().connect<&CharacterService::OnAssignCharacterRequest>(this))
    , m_transferOwnershipConnection(aDispatcher.sink<PacketEvent<RequestOwnershipTransfer>>().connect<&CharacterService::OnOwnershipTransferRequest>(this))
    , m_ownershipTransferEventConnection(aDispatcher.sink<OwnershipTransferEvent>().connect<&CharacterService::OnOwnershipTransferEvent>(this))
    , m_claimOwnershipConnection(aDispatcher.sink<PacketEvent<RequestOwnershipClaim>>().connect<&CharacterService::OnOwnershipClaimRequest>(this))
    , m_removeCharacterConnection(aDispatcher.sink<CharacterRemoveEvent>().connect<&CharacterService::OnCharacterRemoveEvent>(this))
    , m_characterSpawnedConnection(aDispatcher.sink<CharacterSpawnedEvent>().connect<&CharacterService::OnCharacterSpawned>(this))
    , m_referenceMovementSnapshotConnection(aDispatcher.sink<PacketEvent<ClientReferencesMoveRequest>>().connect<&CharacterService::OnReferencesMoveRequest>(this))
    , m_actionEventsConnection(aDispatcher.sink<PacketEvent<RequestActionEvents>>().connect<&CharacterService::OnActionEventsRequest>(this))
    , m_factionsChangesConnection(aDispatcher.sink<PacketEvent<RequestFactionsChanges>>().connect<&CharacterService::OnFactionsChanges>(this))
    , m_mountConnection(aDispatcher.sink<PacketEvent<MountRequest>>().connect<&CharacterService::OnMountRequest>(this))
    , m_newPackageConnection(aDispatcher.sink<PacketEvent<NewPackageRequest>>().connect<&CharacterService::OnNewPackageRequest>(this))
    , m_requestRespawnConnection(aDispatcher.sink<PacketEvent<RequestRespawn>>().connect<&CharacterService::OnRequestRespawn>(this))
    , m_playerAppearanceConnection(aDispatcher.sink<PacketEvent<RequestPlayerAppearance>>().connect<&CharacterService::OnPlayerAppearance>(this))
    , m_syncExperienceConnection(aDispatcher.sink<PacketEvent<SyncExperienceRequest>>().connect<&CharacterService::OnSyncExperienceRequest>(this))
    , m_dialogueConnection(aDispatcher.sink<PacketEvent<DialogueRequest>>().connect<&CharacterService::OnDialogueRequest>(this))
    , m_subtitleConnection(aDispatcher.sink<PacketEvent<SubtitleRequest>>().connect<&CharacterService::OnSubtitleRequest>(this))
{
}

void CharacterService::Serialize(World& aRegistry, entt::entity aEntity, CharacterSpawnRequest* apSpawnRequest) noexcept
{
    const auto& characterComponent = aRegistry.get<CharacterComponent>(aEntity);

    apSpawnRequest->ServerId = World::ToInteger(aEntity);
    apSpawnRequest->HelgenAliasId = characterComponent.HelgenAliasId;
    apSpawnRequest->AppearanceBuffer = characterComponent.SaveBuffer;
    apSpawnRequest->ChangeFlags = characterComponent.ChangeFlags;
    apSpawnRequest->FaceTints = characterComponent.FaceTints;
    apSpawnRequest->FactionsContent = characterComponent.FactionsContent;
    apSpawnRequest->IsDead = characterComponent.IsDead();
    apSpawnRequest->IsPlayer = characterComponent.IsPlayer();
    apSpawnRequest->IsWeaponDrawn = characterComponent.IsWeaponDrawn();
    apSpawnRequest->IsPlayerSummon = characterComponent.IsPlayerSummon();
    apSpawnRequest->PlayerId = characterComponent.PlayerId;

    const auto* pOwnerComponent = aRegistry.try_get<OwnerComponent>(aEntity);
    if (pOwnerComponent)
    {
        apSpawnRequest->OwnershipEpoch = pOwnerComponent->OwnershipEpoch;
    }

    const auto* pFormIdComponent = aRegistry.try_get<FormIdComponent>(aEntity);
    if (pFormIdComponent)
    {
        apSpawnRequest->FormId = pFormIdComponent->Id;
    }

    const auto* pInventoryComponent = aRegistry.try_get<InventoryComponent>(aEntity);
    if (pInventoryComponent)
    {
        apSpawnRequest->InventoryContent = pInventoryComponent->Content;
    }

    const auto* pActorValuesComponent = aRegistry.try_get<ActorValuesComponent>(aEntity);
    if (pActorValuesComponent)
    {
        apSpawnRequest->InitialActorValues = pActorValuesComponent->CurrentActorValues;
    }

    if (characterComponent.BaseId)
    {
        apSpawnRequest->BaseId = characterComponent.BaseId.Id;
    }

    if (characterComponent.LeveledNpcPickId)
    {
        apSpawnRequest->LeveledNpcPickId = characterComponent.LeveledNpcPickId.Id;
    }

    const auto* pMovementComponent = aRegistry.try_get<MovementComponent>(aEntity);
    if (pMovementComponent)
    {
        apSpawnRequest->Position = pMovementComponent->Position;
        apSpawnRequest->Rotation.x = pMovementComponent->Rotation.x;
        apSpawnRequest->Rotation.y = pMovementComponent->Rotation.z;
    }

    const auto* pCellIdComponent = aRegistry.try_get<CellIdComponent>(aEntity);
    if (pCellIdComponent)
    {
        apSpawnRequest->CellId = pCellIdComponent->Cell;
    }

    auto& animationComponent = aRegistry.get<AnimationComponent>(aEntity);
    apSpawnRequest->ActionsToReplay = animationComponent.ActionsReplayCache.FormRefinedReplayChain(pMovementComponent && pMovementComponent->IsHeldInPlace);
}

void CharacterService::OnUpdate(const UpdateEvent&) const noexcept
{
    ProcessFactionsChanges();
}

void CharacterService::OnCharacterExteriorCellChange(const CharacterExteriorCellChangeEvent& acEvent) const noexcept
{
    CharacterSpawnRequest spawnMessage;
    Serialize(m_world, acEvent.Entity, &spawnMessage);

    NotifyRemoveCharacter removeMessage;
    removeMessage.ServerId = World::ToInteger(acEvent.Entity);

    for (auto pPlayer : m_world.GetPlayerManager())
    {
        if (acEvent.Owner == pPlayer)
            continue;

        if (pPlayer->GetCellComponent().WorldSpaceId != acEvent.WorldSpaceId || pPlayer->GetCellComponent().WorldSpaceId == acEvent.WorldSpaceId && !GridCellCoords::IsCellInGridCell(acEvent.CurrentCoords, pPlayer->GetCellComponent().CenterCoords, false))
        {
            pPlayer->Send(removeMessage);
        }
        else if (pPlayer->GetCellComponent().WorldSpaceId == acEvent.WorldSpaceId && GridCellCoords::IsCellInGridCell(acEvent.CurrentCoords, pPlayer->GetCellComponent().CenterCoords, false))
        {
            pPlayer->Send(spawnMessage);
        }
    }
}

void CharacterService::OnCharacterInteriorCellChange(const CharacterInteriorCellChangeEvent& acEvent) const noexcept
{
    CharacterSpawnRequest spawnMessage;
    Serialize(m_world, acEvent.Entity, &spawnMessage);

    NotifyRemoveCharacter removeMessage;
    removeMessage.ServerId = World::ToInteger(acEvent.Entity);

    for (auto pPlayer : m_world.GetPlayerManager())
    {
        if (acEvent.Owner == pPlayer)
            continue;

        if (acEvent.NewCell == pPlayer->GetCellComponent().Cell)
            pPlayer->Send(spawnMessage);
        else
            pPlayer->Send(removeMessage);
    }
}

void CharacterService::OnAssignCharacterRequest(const PacketEvent<AssignCharacterRequest>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;
    const auto& refId = message.ReferenceId;

    const auto isPlayer = (refId.ModId == 0 && refId.BaseId == 0x14);
    const auto isCustom = isPlayer || refId.ModId == std::numeric_limits<uint32_t>::max();

    // Only temporary NPCs may identify themselves by a vanilla MQ101 alias.
    if (message.HelgenAliasId && (refId.ModId != UINT32_MAX || isPlayer || message.IsPlayerSummon || message.HelgenAliasId > 179 || message.HelgenAliasId == 119))
        return;

    if (!isCustom || message.HelgenAliasId)
    {
        // Look for the character
        auto view = m_world.view<ActorValuesComponent, CharacterComponent, MovementComponent, CellIdComponent, OwnerComponent, InventoryComponent>();

        const auto itor = std::find_if(
            std::begin(view), std::end(view),
            [this, view, refId, aliasId = message.HelgenAliasId](auto entity)
            {
                if (aliasId)
                    return view.get<CharacterComponent>(entity).HelgenAliasId == aliasId;
                const auto* pForm = m_world.try_get<FormIdComponent>(entity);
                return pForm && pForm->Id == refId;
            });

        if (itor != std::end(view))
        {
            spdlog::debug("FormId: {:x}:{:x} is already managed", refId.ModId, refId.BaseId);

            auto& ownerComponent = view.get<OwnerComponent>(*itor);
            auto& characterComponent = view.get<CharacterComponent>(*itor);
            const bool isOwner = ownerComponent.GetOwner() == acMessage.pPlayer;
            auto& cellComponent = view.get<CellIdComponent>(*itor);
            const CellIdComponent arrivalCell{message.CellId, {}, {}};
            // A delayed exterior owner must not drag the leader's door-arriving companion out of the keep.
            // ponytail: vanilla Helgen companions/keep only; other scripted transitions need their own evidence.
            const bool keepArrival = refId.ModId == 0 && (refId.BaseId == 0x2BF9E || refId.BaseId == 0x2BFA2) &&
                                     cellComponent.WorldSpaceId == GameId(0, 0x3C) && !message.WorldSpaceId && message.CellId == GameId(0, 0x5DE24) &&
                                     acMessage.pPlayer->GetCellComponent().IsInInteriorCell() && acMessage.pPlayer->GetCellComponent().Cell == message.CellId &&
                                     std::isfinite(message.Position.x) && std::isfinite(message.Position.y) && std::isfinite(message.Position.z);
            const bool transferToLeader = !isOwner && CanClaimOwnership(acMessage.pPlayer, *itor, ownerComponent.OwnershipEpoch, OwnershipTransferReason::LeaderAssignment, keepArrival ? &arrivalCell : nullptr);
            if (transferToLeader && keepArrival)
            {
                cellComponent = arrivalCell;
                view.get<MovementComponent>(*itor).Position = message.Position;
                spdlog::info("Accepted Helgen companion {:X} keep arrival from party leader {:X}", refId.BaseId, acMessage.pPlayer->GetId());
            }

            if (!characterComponent.LeveledNpcPickId && message.LeveledNpcPickId != GameId{})
            {
                characterComponent.LeveledNpcPickId = FormIdComponent(message.LeveledNpcPickId);
                spdlog::debug(
                    "Stored previously unknown leveled NPC pick {:x}:{:x} for FormId {:x}:{:x}",
                    message.LeveledNpcPickId.ModId,
                    message.LeveledNpcPickId.BaseId,
                    refId.ModId,
                    refId.BaseId);
            }

            AssignCharacterResponse response{};
            response.Cookie = message.Cookie;
            response.Owner = isOwner;
            PopulateAssignmentResponse(*itor, response);
            acMessage.pPlayer->Send(response);

            // The assignment response establishes a remote component before the grant arrives.
            if (transferToLeader)
                TransferOwnership(acMessage.pPlayer, *itor, OwnershipTransferReason::LeaderAssignment);

            return;
        }
    }

    // The same actor as one another party member runs, made by its game for the same quest alias: the requester's own one
    // shows that actor (QuestRole)
    if (isCustom && !isPlayer && !message.HelgenAliasId && !message.IsPlayerSummon && QuestRole::IsRole(message.RoleQuestId.BaseId, message.RoleAliasId, true))
    {
        const auto& requesterParty = acMessage.pPlayer->GetParty().JoinedPartyId;
        const auto view = m_world.view<CharacterComponent, CellIdComponent, OwnerComponent>();
        for (const auto entity : view)
        {
            const auto roleIt = m_questRoles.find(entity);
            const Player* pOwner = view.get<OwnerComponent>(entity).GetOwner();
            const auto& characterComponent = view.get<CharacterComponent>(entity);

            const bool cSameRole = roleIt != m_questRoles.end() && roleIt->second.first == message.RoleQuestId && roleIt->second.second == message.RoleAliasId;
            const bool cSharesParty = pOwner && requesterParty && pOwner->GetParty().JoinedPartyId == requesterParty;
            const bool cInRange = view.get<CellIdComponent>(entity).IsInRange(acMessage.pPlayer->GetCellComponent(), characterComponent.IsDragon());
            if (!QuestRole::IsSameSpawn(cSameRole, pOwner == acMessage.pPlayer, cSharesParty, cInRange, characterComponent.IsDead(), message.CurrentActorData.IsDead))
                continue;

            spdlog::info(
                "Actor of alias {} of quest {:x}:{:x} from player {:X} is {:X}, which another party member runs", message.RoleAliasId, message.RoleQuestId.ModId, message.RoleQuestId.BaseId,
                acMessage.pPlayer->GetId(), World::ToInteger(entity));

            AssignCharacterResponse response{};
            response.Cookie = message.Cookie;
            response.Owner = false;
            PopulateAssignmentResponse(entity, response);
            acMessage.pPlayer->Send(response);
            return;
        }
    }

    // This entity has no owner create it
    CreateCharacter(acMessage);
}

void CharacterService::OnOwnershipTransferRequest(const PacketEvent<RequestOwnershipTransfer>& acMessage) const noexcept
{
    const auto& message = acMessage.Packet;

    const entt::entity cEntity = static_cast<entt::entity>(message.ServerId);
    const auto view = m_world.view<OwnerComponent, CharacterComponent, CellIdComponent, MovementComponent>();
    const auto it = view.find(cEntity);
    if (it == view.end())
    {
        spdlog::debug("Ignored ownership release from player {:X} for missing actor {:X}", acMessage.pPlayer->GetId(), message.ServerId);
        return;
    }

    auto& ownerComponent = view.get<OwnerComponent>(*it);
    if (ownerComponent.GetOwner() != acMessage.pPlayer || ownerComponent.OwnershipEpoch != message.OwnershipEpoch)
    {
        const uint32_t ownerId = ownerComponent.GetOwner() ? ownerComponent.GetOwner()->GetId() : 0;
        spdlog::debug(
            "Ignored ownership release from player {:X} for actor {:X}; current owner is {:X} and requested epoch {} does not match {}",
            acMessage.pPlayer->GetId(), message.ServerId, ownerId, message.OwnershipEpoch, ownerComponent.OwnershipEpoch);
        return;
    }

    if (message.Reason != OwnershipReleaseReason::Relinquish && message.Reason != OwnershipReleaseReason::DeclineGrant)
    {
        spdlog::warn("Ignored ownership release with invalid reason from player {:X} for actor {:X}", acMessage.pPlayer->GetId(), message.ServerId);
        return;
    }

    auto& characterComponent = view.get<CharacterComponent>(*it);
    if (characterComponent.IsPlayerSummon())
    {
        spdlog::info("Removing summon {:X} after player {:X} relinquished ownership", message.ServerId, acMessage.pPlayer->GetId());
        m_world.GetDispatcher().trigger(CharacterRemoveEvent(message.ServerId));
        return;
    }

    if (message.Reason == OwnershipReleaseReason::Relinquish && (message.WorldSpaceId || message.CellId))
    {
        const auto* pFormIdComponent = m_world.try_get<FormIdComponent>(cEntity);
        if (pFormIdComponent)
        {
            NotifyActorTeleport notify{};
            notify.FormId = pFormIdComponent->Id;
            notify.WorldSpaceId = message.WorldSpaceId;
            notify.CellId = message.CellId;
            notify.Position = message.Position;

            GameServer::Get()->SendToPlayers(notify, acMessage.pPlayer);
        }

        auto& cellIdComponent = view.get<CellIdComponent>(*it);
        cellIdComponent.WorldSpaceId = message.WorldSpaceId;
        cellIdComponent.Cell = message.CellId;
        cellIdComponent.CenterCoords = GridCellCoords::CalculateGridCellCoords(message.Position);

        auto& movementComponent = view.get<MovementComponent>(*it);
        movementComponent.Position = message.Position;
    }

    // A normal release starts a fresh search. A declined grant continues the current
    // search, retaining failed candidates so unloaded clients cannot bounce ownership.
    if (message.Reason == OwnershipReleaseReason::Relinquish)
        ownerComponent.InvalidOwners.clear();

    ownerComponent.InvalidOwners.push_back(acMessage.pPlayer);

    TransferToNextOwner(cEntity, OwnershipTransferReason::Relinquish);
}

void CharacterService::OnOwnershipTransferEvent(const OwnershipTransferEvent& acEvent) const noexcept
{
    // A disconnect starts a fresh search; previously unavailable clients may be ready now.
    const auto view = m_world.view<OwnerComponent>();
    if (const auto it = view.find(acEvent.Entity); it != view.end())
        view.get<OwnerComponent>(*it).InvalidOwners.clear();

    TransferToNextOwner(acEvent.Entity, OwnershipTransferReason::OwnerUnavailable);
}

void CharacterService::OnCharacterRemoveEvent(const CharacterRemoveEvent& acEvent) const noexcept
{
    const auto view = m_world.view<OwnerComponent>();
    const auto it = view.find(static_cast<entt::entity>(acEvent.ServerId));
    if (it == view.end())
        return;

    GameServer::Get()->GetWorld().GetScriptService().HandleCharacterDestoy(*it);

    // The death would go with the entity, and the next client to come by still has the actor alive in its save
    const auto* pCharacterComponent = m_world.try_get<CharacterComponent>(*it);
    const auto* pFormIdComponent = m_world.try_get<FormIdComponent>(*it);
    if (pCharacterComponent && pFormIdComponent && pCharacterComponent->IsDead())
        m_deadActors[pFormIdComponent->Id] = GetGameHour(m_world.GetCalendarService());

    NotifyRemoveCharacter response;
    response.ServerId = acEvent.ServerId;

    for (auto pPlayer : m_world.GetPlayerManager())
        pPlayer->Send(response);

    // Entity ids are reused
    m_questRoles.erase(*it);

    // A player whose character is gone has none until the client assigns a new one; the id may soon name another entity
    for (Player* pPlayer : m_world.GetPlayerManager())
    {
        if (pPlayer->GetCharacter() == *it)
            pPlayer->ClearCharacter();
    }

    m_world.destroy(*it);
    spdlog::debug("Character destroyed {:X}", acEvent.ServerId);
}

void CharacterService::OnOwnershipClaimRequest(const PacketEvent<RequestOwnershipClaim>& acMessage) const noexcept
{
    const auto& message = acMessage.Packet;
    const entt::entity cEntity = static_cast<entt::entity>(message.ServerId);

    if (CanClaimOwnership(acMessage.pPlayer, cEntity, message.ExpectedOwnershipEpoch, OwnershipTransferReason::LeaderClaim))
    {
        TransferOwnership(acMessage.pPlayer, cEntity, OwnershipTransferReason::LeaderClaim);
        return;
    }

    const auto view = m_world.view<OwnerComponent, CharacterComponent, CellIdComponent, MovementComponent>();
    const auto it = view.find(cEntity);
    if (it == view.end())
        return;

    const auto& ownerComponent = view.get<OwnerComponent>(*it);
    const auto& characterComponent = view.get<CharacterComponent>(*it);
    const auto& cellIdComponent = view.get<CellIdComponent>(*it);
    const auto& movementComponent = view.get<MovementComponent>(*it);
    Player* const pOwner = ownerComponent.GetOwner();

    if (pOwner == acMessage.pPlayer || characterComponent.IsMount() || characterComponent.IsPlayer() || characterComponent.IsPlayerSummon() ||
        (!m_world.all_of<FormIdComponent>(*it) && !characterComponent.HelgenAliasId) ||
        !acMessage.pPlayer->GetCellComponent().IsInRange(cellIdComponent, characterComponent.IsDragon()))
    {
        return;
    }

    // Its AI is what makes an actor stop and face the player talking to it, so that player's client runs it. Only from
    // another member of the owner's party, who could already have it as leader.
    // Two members talking to the same actor would hand it back and forth, each hand-over resending its data: one per 10 s.
    if (message.ForDialogue && pOwner && message.ExpectedOwnershipEpoch == ownerComponent.OwnershipEpoch &&
        GameServer::Get()->GetTick() - ownerComponent.LastDialogueTransfer >= cDialogueTransferInterval)
    {
        const auto* pParty = m_world.GetPartyService().GetPlayerParty(acMessage.pPlayer);
        if (pParty && std::find(pParty->Members.begin(), pParty->Members.end(), pOwner) != pParty->Members.end())
        {
            TransferOwnership(acMessage.pPlayer, cEntity, OwnershipTransferReason::Dialogue);
            return;
        }
    }

    // Any client around may take over an actor its owner stopped running, or left asleep while the actor woke up on that client.
    // Handing over a sleeping actor costs nothing, and its ambush scripts only run where the AI does.
    const bool cOwnerSilent = GameServer::Get()->GetTick() - movementComponent.LastReceived >= cOwnerSilentTime;
    const bool cOwnerGone = !pOwner || !pOwner->GetCellComponent().IsInRange(cellIdComponent, characterComponent.IsDragon());
    if (cOwnerSilent || cOwnerGone || movementComponent.IsHeldInPlace)
    {
        TransferOwnership(acMessage.pPlayer, cEntity, OwnershipTransferReason::OwnerUnavailable);
        return;
    }

    // Nothing wrong with the owner, the client missed the update telling it who that is
    if (message.ExpectedOwnershipEpoch != ownerComponent.OwnershipEpoch)
    {
        NotifyOwnershipTransfer notify{};
        notify.ServerId = message.ServerId;
        notify.OwnerPlayerId = pOwner->GetId();
        notify.OwnershipEpoch = ownerComponent.OwnershipEpoch;
        notify.CurrentActorData = BuildActorData(*it);
        notify.LeveledNpcPickId = characterComponent.LeveledNpcPickId.Id;
        acMessage.pPlayer->Send(notify);
    }
}

void CharacterService::OnCharacterSpawned(const CharacterSpawnedEvent& acEvent) const noexcept
{
    CharacterSpawnRequest message;
    Serialize(m_world, acEvent.Entity, &message);

    const auto& ownerComp = m_world.get<OwnerComponent>(acEvent.Entity);
    if (!GameServer::Get()->SendToPlayersInRange(message, acEvent.Entity, ownerComp.GetOwner()))
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);

    GameServer::Get()->GetWorld().GetScriptService().HandleCharacterSpawn(acEvent.Entity);
}

void CharacterService::OnReferencesMoveRequest(const PacketEvent<ClientReferencesMoveRequest>& acMessage) const noexcept
{
    OwnerView<MovementComponent, CellIdComponent, CharacterComponent> view(m_world, acMessage.GetSender());

    auto& message = acMessage.Packet;

    // Relayed as soon as it arrives, with the owner's capture tick, so receivers can schedule it on the owner's timeline.
    // Snapshots can arrive out of order, receivers sort them out.
    Map<Player*, ServerReferencesMoveRequest> messages;

    for (auto& [id, movement] : message.Updates)
    {
        const auto entity = static_cast<entt::entity>(id);

        auto itor = view.find(entity);
        if (itor == std::end(view))
        {
            spdlog::debug("{:x} requested move of {:x} but does not exist", acMessage.pPlayer->GetConnectionId(), World::ToInteger(entity));
            continue;
        }

        if (!view.get<OwnerComponent>(*itor).IsCurrentOwner(acMessage.pPlayer, movement.OwnershipEpoch))
            continue;

        auto& movementComponent = view.get<MovementComponent>(*itor);
        auto& cellIdComponent = view.get<CellIdComponent>(*itor);
        const auto& characterComponent = view.get<CharacterComponent>(*itor);
        const CellIdComponent cPreviousCell = cellIdComponent;

        movementComponent.Tick = message.Tick;
        movementComponent.Position = movement.Position;
        movementComponent.Rotation = glm::vec3(movement.Rotation.x, 0.f, movement.Rotation.y);
        movementComponent.Variables = movement.Variables;
        movementComponent.Direction = movement.Direction;
        movementComponent.LastReceived = GameServer::Get()->GetTick();
        movementComponent.IsHeldInPlace = movement.IsHeldInPlace;

        // The owner sends no cell while the actor has no parent cell it can name (mid-load): the actor is still where it was
        if (movement.CellId != GameId{} || movement.WorldSpaceId != GameId{})
        {
            cellIdComponent.Cell = movement.CellId;
            cellIdComponent.WorldSpaceId = movement.WorldSpaceId;
            cellIdComponent.CenterCoords = GridCellCoords::CalculateGridCellCoords(movement.Position.x, movement.Position.y);
        }

        // Players are spawned and removed through their own cell requests, NPCs only ever were when a player moved. Exterior
        // range follows the grid cell of the position, which can cross a grid edge in another snapshot than the cell id.
        if (!characterComponent.IsPlayer() &&
            (cPreviousCell.Cell != cellIdComponent.Cell || cPreviousCell.WorldSpaceId != cellIdComponent.WorldSpaceId || cPreviousCell.CenterCoords != cellIdComponent.CenterCoords))
            NotifyRangeChange(entity, cPreviousCell, acMessage.pPlayer);

        for (auto pPlayer : m_world.GetPlayerManager())
        {
            if (pPlayer == acMessage.pPlayer || !cellIdComponent.IsInRange(pPlayer->GetCellComponent(), characterComponent.IsDragon()))
                continue;

            auto& relay = messages[pPlayer];
            relay.Tick = message.Tick;
            relay.Updates[id] = movement;
        }
    }

    // Sent right away, Nagle would hold every relay back by up to 5 ms waiting for more to share the packet with
    for (auto& [pPlayer, relay] : messages)
        pPlayer->Send(relay, TiltedPhoques::kUnreliableNoNagle);
}

void CharacterService::NotifyRangeChange(const entt::entity aEntity, const CellIdComponent& acPreviousCell, const Player* apOwner) const noexcept
{
    // An NPC its owner takes into another player's range was never shown there, that player's game has it wherever its own
    // save left it: someone walking out of an interior wasn't seen outside. One that leaves the range stayed frozen there.
    const auto& cellIdComponent = m_world.get<CellIdComponent>(aEntity);
    const bool cIsDragon = m_world.get<CharacterComponent>(aEntity).IsDragon();

    CharacterSpawnRequest spawnMessage;
    bool isSpawnSerialized = false;

    NotifyRemoveCharacter removeMessage;
    removeMessage.ServerId = World::ToInteger(aEntity);

    for (Player* pPlayer : m_world.GetPlayerManager())
    {
        if (pPlayer == apOwner)
            continue;

        const bool cWasInRange = acPreviousCell.IsInRange(pPlayer->GetCellComponent(), cIsDragon);
        const bool cIsInRange = cellIdComponent.IsInRange(pPlayer->GetCellComponent(), cIsDragon);

        if (cIsInRange && !cWasInRange)
        {
            if (!isSpawnSerialized)
            {
                Serialize(m_world, aEntity, &spawnMessage);
                isSpawnSerialized = true;
            }

            pPlayer->Send(spawnMessage);
        }
        else if (cWasInRange && !cIsInRange)
            pPlayer->Send(removeMessage);
    }
}

void CharacterService::OnActionEventsRequest(const PacketEvent<RequestActionEvents>& acMessage) const noexcept
{
    OwnerView<AnimationComponent> view(m_world, acMessage.GetSender());

    auto& message = acMessage.Packet;

    for (auto& [id, actions] : message.Actions)
    {
        const auto entity = static_cast<entt::entity>(id);

        auto itor = view.find(entity);
        if (itor == std::end(view))
        {
            spdlog::debug("{:x} requested actions of {:x} but does not exist", acMessage.pPlayer->GetConnectionId(), World::ToInteger(entity));
            continue;
        }

        NotifyActionEvents notify;
        auto& acceptedActions = notify.Actions[id];

        for (auto& action : actions)
        {
            auto [canceled, reason] = GameServer::Get()->GetWorld().GetScriptService().HandleCharacterMove(entity);
            if (!canceled)
                acceptedActions.push_back(action);
        }

        view.get<AnimationComponent>(*itor).ActionsReplayCache.AppendAll(actions);

        if (!acceptedActions.empty())
            GameServer::Get()->SendToPlayersInRange(notify, entity, acMessage.pPlayer);
    }
}

void CharacterService::OnFactionsChanges(const PacketEvent<RequestFactionsChanges>& acMessage) const noexcept
{
    OwnerView<CharacterComponent> view(m_world, acMessage.GetSender());

    auto& message = acMessage.Packet;

    for (auto& [id, factions] : message.Changes)
    {
        auto it = view.find(static_cast<entt::entity>(id));

        if (it == std::end(view) || view.get<OwnerComponent>(*it).GetOwner() != acMessage.pPlayer)
            continue;

        auto& characterComponent = view.get<CharacterComponent>(*it);
        characterComponent.FactionsContent = factions;
        characterComponent.SetDirtyFactions(true);
    }
}

void CharacterService::OnMountRequest(const PacketEvent<MountRequest>& acMessage) const noexcept
{
    const auto& message = acMessage.Packet;
    const entt::entity cRiderEntity = static_cast<entt::entity>(message.RiderId);
    const entt::entity cMountEntity = static_cast<entt::entity>(message.MountId);
    const auto view = m_world.view<OwnerComponent, CharacterComponent, CellIdComponent>();
    const auto riderIt = view.find(cRiderEntity);
    const auto mountIt = view.find(cMountEntity);

    if (riderIt == view.end() || mountIt == view.end() || cRiderEntity == cMountEntity)
    {
        spdlog::debug("Rejected mount request from player {:X} because rider {:X} or mount {:X} is invalid", acMessage.pPlayer->GetId(), message.RiderId, message.MountId);
        return;
    }

    if (!view.get<CharacterComponent>(*mountIt).IsMount())
    {
        spdlog::warn("Rejected mount request from player {:X} because actor {:X} is not a mount", acMessage.pPlayer->GetId(), message.MountId);
        return;
    }

    const auto& riderOwner = view.get<OwnerComponent>(*riderIt);
    const auto& mountOwner = view.get<OwnerComponent>(*mountIt);
    if (riderOwner.GetOwner() != acMessage.pPlayer || riderOwner.OwnershipEpoch != message.RiderOwnershipEpoch || mountOwner.OwnershipEpoch != message.MountOwnershipEpoch)
    {
        spdlog::debug(
            "Rejected stale mount request from player {:X} for rider {:X} at epoch {} and mount {:X} at epoch {}; current epochs are {} and {}",
            acMessage.pPlayer->GetId(), message.RiderId, message.RiderOwnershipEpoch, message.MountId, message.MountOwnershipEpoch,
            riderOwner.OwnershipEpoch, mountOwner.OwnershipEpoch);
        return;
    }

    const auto& mountCell = view.get<CellIdComponent>(*mountIt);
    if (!acMessage.pPlayer->GetCellComponent().IsInRange(mountCell, view.get<CharacterComponent>(*mountIt).IsDragon()))
    {
        spdlog::debug("Rejected mount request from player {:X} because mount {:X} is out of range", acMessage.pPlayer->GetId(), message.MountId);
        return;
    }

    if (!TransferOwnership(acMessage.pPlayer, *mountIt, OwnershipTransferReason::Mount))
        return;

    NotifyMount notify;
    notify.RiderId = message.RiderId;
    notify.MountId = message.MountId;

    if (!GameServer::Get()->SendToPlayersInRange(notify, cMountEntity, acMessage.GetSender()))
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
}

void CharacterService::OnNewPackageRequest(const PacketEvent<NewPackageRequest>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;
    const auto* pOwner = m_world.try_get<OwnerComponent>(static_cast<entt::entity>(message.ActorId));
    if (!pOwner || pOwner->GetOwner() != acMessage.pPlayer)
        return;

    NotifyNewPackage notify;
    notify.ActorId = message.ActorId;
    notify.PackageId = message.PackageId;

    const entt::entity cEntity = static_cast<entt::entity>(message.ActorId);
    if (!GameServer::Get()->SendToPlayersInRange(notify, cEntity, acMessage.GetSender()))
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
}

void CharacterService::OnPlayerAppearance(const PacketEvent<RequestPlayerAppearance>& acMessage) const noexcept
{
    const auto& appearance = acMessage.Packet.Appearance;
    const auto entity = static_cast<entt::entity>(appearance.ActorId);
    auto view = m_world.view<OwnerComponent, CharacterComponent>();
    if (!view.contains(entity) || !view.get<OwnerComponent>(entity).IsCurrentOwner(acMessage.pPlayer, appearance.OwnershipEpoch) ||
        !view.get<CharacterComponent>(entity).IsPlayer() || appearance.AppearanceBuffer.empty() || appearance.AppearanceBuffer.size() > 32768)
        return;
    auto& character = view.get<CharacterComponent>(entity);
    character.SaveBuffer = appearance.AppearanceBuffer;
    character.ChangeFlags = appearance.ChangeFlags;
    character.FaceTints = appearance.FaceTints;
    NotifyPlayerAppearance notify;
    notify.Appearance = appearance;
    GameServer::Get()->SendToPlayersInRange(notify, entity, acMessage.GetSender());
}

void CharacterService::OnRequestRespawn(const PacketEvent<RequestRespawn>& acMessage) const noexcept
{
    auto view = m_world.view<OwnerComponent, CharacterComponent>();
    auto it = view.find(static_cast<entt::entity>(acMessage.Packet.ActorId));
    if (it == view.end())
    {
        spdlog::warn("No OwnerComponent found for actor id {:X}", acMessage.Packet.ActorId);
        return;
    }

    auto& ownerComponent = view.get<OwnerComponent>(*it);

    // Replay cache needs to be cleared when a character respawns
    if (auto* pAnimationComponent = m_world.try_get<AnimationComponent>(*it))
        pAnimationComponent->ActionsReplayCache.Clear();

    if (ownerComponent.GetOwner() == acMessage.pPlayer)
    {
        if (!acMessage.Packet.AppearanceBuffer.empty())
        {
            auto& characterComponent = view.get<CharacterComponent>(*it);
            characterComponent.SaveBuffer = acMessage.Packet.AppearanceBuffer;
            characterComponent.ChangeFlags = acMessage.Packet.ChangeFlags;
        }

        NotifyRespawn notify;
        notify.ActorId = acMessage.Packet.ActorId;

        if (!GameServer::Get()->SendToPlayersInRange(notify, *it, acMessage.GetSender()))
            spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
    }
    else
    {
        CharacterSpawnRequest message;
        Serialize(m_world, *it, &message);

        acMessage.GetSender()->Send(message);
    }
}

void CharacterService::OnSyncExperienceRequest(const PacketEvent<SyncExperienceRequest>& acMessage) const noexcept
{
    if (!bEnableXpSync)
        return;

    if (!acMessage.pPlayer->GetCharacter())
        return;

    NotifySyncExperience notify;
    notify.Experience = acMessage.Packet.Experience;

    // A kill at the other end of the map isn't a fight this player is in.
    const auto& partyComponent = acMessage.pPlayer->GetParty();
    GameServer::Get()->SendToPartyInRange(notify, partyComponent, *acMessage.pPlayer->GetCharacter(), acMessage.GetSender());
}

void CharacterService::OnDialogueRequest(const PacketEvent<DialogueRequest>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    NotifyDialogue notify{};
    notify.ServerId = message.ServerId;
    notify.SoundFilename = message.SoundFilename;
    notify.SceneId = message.SceneId;
    notify.ScenePhase = message.ScenePhase;

    const entt::entity cEntity = static_cast<entt::entity>(message.ServerId);
    if (!GameServer::Get()->SendToPlayersInRange(notify, cEntity, acMessage.GetSender()))
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
}

void CharacterService::OnSubtitleRequest(const PacketEvent<SubtitleRequest>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    NotifySubtitle notify{};
    notify.ServerId = message.ServerId;
    notify.Text = message.Text;
    notify.SceneId = message.SceneId;
    notify.ScenePhase = message.ScenePhase;

    const entt::entity cEntity = static_cast<entt::entity>(message.ServerId);
    if (!GameServer::Get()->SendToPlayersInRange(notify, cEntity, acMessage.GetSender()))
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
}

void CharacterService::CreateCharacter(const PacketEvent<AssignCharacterRequest>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    const auto gameId = message.ReferenceId;
    const auto baseId = message.FormId;

    const auto isTemporary = gameId.ModId == std::numeric_limits<uint32_t>::max();
    const auto isPlayer = (gameId.ModId == 0 && gameId.BaseId == 0x14);
    const auto isCustom = isPlayer || isTemporary;

    // For player characters and temporary forms
    if (isCustom && baseId != GameId{} && !isTemporary)
    {
        spdlog::warn("Unexpected NpcId, player {:x} might be forging packets", acMessage.pPlayer->GetConnectionId());
        return;
    }

    // A player has one character. One left from an earlier assignment stays on every other client as a copy nobody moves,
    // and each assignment adds another. Removed before anything is created, so nothing below holds on to a component it moves.
    if (const auto oldCharacter = acMessage.pPlayer->GetCharacter(); isPlayer && oldCharacter && m_world.valid(*oldCharacter))
    {
        spdlog::warn("Removing character {:X} of player {:X}, the player was given a new one", World::ToInteger(*oldCharacter), acMessage.pPlayer->GetId());
        m_world.GetDispatcher().trigger(CharacterRemoveEvent(World::ToInteger(*oldCharacter)));
    }

    const auto cEntity = m_world.create();

    if (!isCustom)
        m_world.emplace<FormIdComponent>(cEntity, gameId.BaseId, gameId.ModId);

    auto* const pServer = GameServer::Get();

    m_world.emplace<OwnerComponent>(cEntity, acMessage.pPlayer);

    auto& cellIdComponent = m_world.emplace<CellIdComponent>(cEntity, message.CellId);
    if (message.WorldSpaceId != GameId{})
    {
        cellIdComponent.WorldSpaceId = message.WorldSpaceId;
        cellIdComponent.CenterCoords = GridCellCoords::CalculateGridCellCoords(message.Position);
    }

    auto& characterComponent = m_world.emplace<CharacterComponent>(cEntity);
    characterComponent.HelgenAliasId = message.HelgenAliasId;
    if (isTemporary && !message.HelgenAliasId && QuestRole::IsRole(message.RoleQuestId.BaseId, message.RoleAliasId, true))
        m_questRoles[cEntity] = {message.RoleQuestId, message.RoleAliasId};
    characterComponent.ChangeFlags = message.ChangeFlags;
    characterComponent.SaveBuffer = std::move(message.AppearanceBuffer);
    characterComponent.BaseId = FormIdComponent(message.FormId);
    // Client-authoritative like BaseId; worst case a forged id changes which NPC identity renders.
    if (message.LeveledNpcPickId != GameId{})
        characterComponent.LeveledNpcPickId = FormIdComponent(message.LeveledNpcPickId);

    if (characterComponent.LeveledNpcPickId)
        spdlog::debug("Stored leveled NPC pick {:x}:{:x} for FormId {:x}:{:x}", message.LeveledNpcPickId.ModId, message.LeveledNpcPickId.BaseId, gameId.ModId, gameId.BaseId);
    characterComponent.FaceTints = message.FaceTints;
    characterComponent.FactionsContent = message.FactionsContent;
    bool isDead = message.CurrentActorData.IsDead;
    if (!isCustom)
    {
        if (const auto deathIt = m_deadActors.find(gameId); deathIt != m_deadActors.end())
        {
            // A player who was there when it died and has it alive brought it back since, the console's resurrect say
            const bool cSawDeath = m_world.ctx().at<ActorValueService>().TakeWitness(gameId, *acMessage.pPlayer);
            isDead = isDead || DeathMemory::KeepsDeath(GetGameHour(m_world.GetCalendarService()) - deathIt->second, cSawDeath);
            m_deadActors.erase(deathIt);
        }
    }

    characterComponent.SetDead(isDead);
    characterComponent.SetPlayer(isPlayer);
    characterComponent.SetWeaponDrawn(message.CurrentActorData.IsWeaponDrawn);
    characterComponent.SetDragon(message.IsDragon);
    characterComponent.SetMount(message.IsMount);
    characterComponent.SetPlayerSummon(message.IsPlayerSummon);

    auto& inventoryComponent = m_world.emplace<InventoryComponent>(cEntity);
    inventoryComponent.Content = message.CurrentActorData.InitialInventory;

    auto& actorValuesComponent = m_world.emplace<ActorValuesComponent>(cEntity);
    actorValuesComponent.CurrentActorValues = message.CurrentActorData.InitialActorValues;

    spdlog::debug("FormId: {:x}:{:x} - NpcId: {:x}:{:x} assigned to {:x}", gameId.ModId, gameId.BaseId, baseId.ModId, baseId.BaseId, acMessage.pPlayer->GetConnectionId());

    auto& movementComponent = m_world.emplace<MovementComponent>(cEntity);
    movementComponent.Tick = pServer->GetTick();
    movementComponent.LastReceived = pServer->GetTick();
    movementComponent.Position = message.Position;
    movementComponent.Rotation = {message.Rotation.x, 0.f, message.Rotation.y};

    m_world.emplace<AnimationComponent>(cEntity);

    // If this is a player character store a ref and trigger an event
    if (isPlayer)
    {
        const auto pPlayer = acMessage.pPlayer;

        pPlayer->SetCharacter(cEntity);
        pPlayer->GetQuestLogComponent().QuestContent = message.QuestContent;
        characterComponent.PlayerId = pPlayer->GetId();

        auto& dispatcher = m_world.GetDispatcher();
        dispatcher.trigger(PlayerEnterWorldEvent(pPlayer));
    }

    AssignCharacterResponse response{};
    response.Cookie = message.Cookie;
    response.Owner = true;
    PopulateAssignmentResponse(cEntity, response);

    pServer->Send(acMessage.pPlayer->GetConnectionId(), response);

    auto& dispatcher = m_world.GetDispatcher();
    dispatcher.trigger(CharacterSpawnedEvent(cEntity));
}

void CharacterService::PopulateAssignmentResponse(const entt::entity aEntity, AssignCharacterResponse& aResponse) const noexcept
{
    aResponse.ServerId = World::ToInteger(aEntity);

    if (const auto* pOwnerComponent = m_world.try_get<OwnerComponent>(aEntity))
        aResponse.OwnershipEpoch = pOwnerComponent->OwnershipEpoch;

    if (const auto* pActorValuesComponent = m_world.try_get<ActorValuesComponent>(aEntity))
        aResponse.AllActorValues = pActorValuesComponent->CurrentActorValues;

    if (const auto* pInventoryComponent = m_world.try_get<InventoryComponent>(aEntity))
        aResponse.CurrentInventory = pInventoryComponent->Content;

    if (const auto* pCharacterComponent = m_world.try_get<CharacterComponent>(aEntity))
    {
        aResponse.PlayerId = pCharacterComponent->PlayerId;
        aResponse.IsDead = pCharacterComponent->IsDead();
        aResponse.IsWeaponDrawn = pCharacterComponent->IsWeaponDrawn();
        aResponse.LeveledNpcPickId = pCharacterComponent->LeveledNpcPickId.Id;

        if (pCharacterComponent->LeveledNpcPickId)
        {
            spdlog::debug(
                "Including leveled NPC pick in assignment response for actor {:X}, pick: {:x}:{:x}, owner: {}, epoch: {}",
                aResponse.ServerId,
                aResponse.LeveledNpcPickId.ModId,
                aResponse.LeveledNpcPickId.BaseId,
                aResponse.Owner,
                aResponse.OwnershipEpoch);
        }
    }

    const auto* pMovementComponent = m_world.try_get<MovementComponent>(aEntity);
    if (pMovementComponent)
        aResponse.Position = pMovementComponent->Position;

    if (const auto* pCellIdComponent = m_world.try_get<CellIdComponent>(aEntity))
    {
        aResponse.CellId = pCellIdComponent->Cell;
        aResponse.WorldSpaceId = pCellIdComponent->WorldSpaceId;
    }

    if (auto* pAnimationComponent = m_world.try_get<AnimationComponent>(aEntity))
        aResponse.ActionsToReplay = pAnimationComponent->ActionsReplayCache.FormRefinedReplayChain(pMovementComponent && pMovementComponent->IsHeldInPlace);
}

const char* CharacterService::GetOwnershipTransferReasonName(const OwnershipTransferReason aReason) noexcept
{
    switch (aReason)
    {
    case OwnershipTransferReason::LeaderAssignment:
        return "party leader assignment";
    case OwnershipTransferReason::LeaderClaim:
        return "party leader claim";
    case OwnershipTransferReason::Mount:
        return "mounting";
    case OwnershipTransferReason::Relinquish:
        return "owner relinquished control";
    case OwnershipTransferReason::OwnerUnavailable:
        return "owner became unavailable";
    case OwnershipTransferReason::Dialogue:
        return "dialogue with a party member";
    }

    return "unknown reason";
}

bool CharacterService::CanClaimOwnership(Player* apPlayer, const entt::entity aEntity, const uint32_t aExpectedOwnershipEpoch, const OwnershipTransferReason aReason, const CellIdComponent* apArrivalCell) const noexcept
{
    const uint32_t serverId = World::ToInteger(aEntity);
    const char* pReasonName = GetOwnershipTransferReasonName(aReason);
    const auto view = m_world.view<OwnerComponent, CharacterComponent, CellIdComponent>();
    const auto it = view.find(aEntity);
    if (it == view.end())
    {
        spdlog::debug("Rejected {} from player {:X} because actor {:X} is unavailable or temporary", pReasonName, apPlayer->GetId(), serverId);
        return false;
    }

    const auto& ownerComponent = view.get<OwnerComponent>(*it);
    const auto& characterComponent = view.get<CharacterComponent>(*it);
    const auto& cellIdComponent = view.get<CellIdComponent>(*it);
    Player* const pCurrentOwner = ownerComponent.GetOwner();
    const uint32_t currentOwnerId = pCurrentOwner ? pCurrentOwner->GetId() : 0;

    const auto reject = [&](const char* apReason)
    {
        spdlog::debug(
            "Rejected {} from player {:X} for actor {:X}: {} (requested epoch {}, current owner {:X}, current epoch {})",
            pReasonName, apPlayer->GetId(), serverId, apReason, aExpectedOwnershipEpoch, currentOwnerId, ownerComponent.OwnershipEpoch);
        return false;
    };

    if (aExpectedOwnershipEpoch == 0 || ownerComponent.OwnershipEpoch != aExpectedOwnershipEpoch)
        return reject("the ownership epoch is stale");

    if (!pCurrentOwner || pCurrentOwner == apPlayer)
        return reject("the player already owns the actor");

    if (characterComponent.IsMount() || characterComponent.IsPlayer() ||
        (!m_world.all_of<FormIdComponent>(aEntity) && !characterComponent.HelgenAliasId))
        return reject("the actor cannot be claimed");

    if (!apPlayer->GetCellComponent().IsInRange(apArrivalCell ? *apArrivalCell : cellIdComponent, characterComponent.IsDragon()))
        return reject("the actor is out of range");

    auto& partyService = m_world.GetPartyService();
    if (!partyService.IsPlayerInParty(apPlayer) || !partyService.IsPlayerLeader(apPlayer))
        return reject("the player is not the party leader");

    PartyService::Party* const pParty = partyService.GetPlayerParty(apPlayer);
    if (!pParty || std::find(pParty->Members.begin(), pParty->Members.end(), pCurrentOwner) == pParty->Members.end())
        return reject("the current owner is not in the party");

    return true;
}

bool CharacterService::TransferOwnership(Player* apPlayer, const entt::entity aEntity, const OwnershipTransferReason aReason, const bool aResetInvalidOwners) const noexcept
{
    const char* pReasonName = GetOwnershipTransferReasonName(aReason);
    const auto view = m_world.view<OwnerComponent, CharacterComponent, CellIdComponent>();
    const auto it = view.find(aEntity);
    if (!apPlayer || it == view.end())
    {
        spdlog::warn("Cannot transfer ownership of actor {:X} for {} because the target is invalid", World::ToInteger(aEntity), pReasonName);
        return false;
    }

    auto& ownerComponent = view.get<OwnerComponent>(*it);
    Player* const pOldOwner = ownerComponent.GetOwner();
    if (pOldOwner == apPlayer)
        return true;

    const uint32_t oldOwnerId = pOldOwner ? pOldOwner->GetId() : 0;
    const uint32_t oldEpoch = ownerComponent.OwnershipEpoch;
    uint32_t newEpoch = oldEpoch + 1;
    if (newEpoch == 0)
        newEpoch = 1;

    NotifyOwnershipTransfer notify{};
    notify.ServerId = World::ToInteger(aEntity);
    notify.OwnerPlayerId = apPlayer->GetId();
    notify.OwnershipEpoch = newEpoch;
    notify.CurrentActorData = BuildActorData(aEntity);
    notify.LeveledNpcPickId = view.get<CharacterComponent>(*it).LeveledNpcPickId.Id;

    ownerComponent.SetOwner(apPlayer);
    ownerComponent.OwnershipEpoch = newEpoch;
    if (aReason == OwnershipTransferReason::Dialogue)
        ownerComponent.LastDialogueTransfer = GameServer::Get()->GetTick();
    if (aResetInvalidOwners)
        ownerComponent.InvalidOwners.clear();

    if (auto* pMovementComponent = m_world.try_get<MovementComponent>(aEntity))
    {
        pMovementComponent->LastReceived = GameServer::Get()->GetTick();
        pMovementComponent->IsHeldInPlace = false;
    }

    if (!GameServer::Get()->SendToPlayersInRange(notify, aEntity, pOldOwner))
        spdlog::error("Failed to broadcast ownership transfer for actor {:X}", notify.ServerId);

    // The former owner may already be out of range, so notify it directly as well.
    if (pOldOwner)
        pOldOwner->Send(notify);

    spdlog::debug(
        "Transferred ownership of actor {:X} from player {:X} to player {:X} for {} (epoch {} to {})",
        notify.ServerId, oldOwnerId, notify.OwnerPlayerId, pReasonName, oldEpoch, newEpoch);

    return true;
}

void CharacterService::TransferToNextOwner(const entt::entity aEntity, const OwnershipTransferReason aReason) const noexcept
{
    const char* pReasonName = GetOwnershipTransferReasonName(aReason);
    const auto view = m_world.view<OwnerComponent, CharacterComponent, CellIdComponent>();
    const auto it = view.find(aEntity);
    if (it == view.end())
    {
        spdlog::warn("Cannot select a new owner for actor {:X} after {} because the actor is missing", World::ToInteger(aEntity), pReasonName);
        return;
    }

    auto& ownerComponent = view.get<OwnerComponent>(*it);
    const auto& characterComponent = view.get<CharacterComponent>(*it);
    const auto& cellIdComponent = view.get<CellIdComponent>(*it);

    // A player's character goes with its player. Handed to someone else it became an NPC copy of that player there.
    if (characterComponent.IsPlayer())
    {
        spdlog::warn("Removing character {:X} after {}, it belongs to a player", World::ToInteger(aEntity), pReasonName);
        m_world.GetDispatcher().trigger(CharacterRemoveEvent(World::ToInteger(aEntity)));
        return;
    }

    for (Player* pPlayer : m_world.GetPlayerManager())
    {
        if (pPlayer == ownerComponent.GetOwner())
            continue;

        if (std::find(ownerComponent.InvalidOwners.begin(), ownerComponent.InvalidOwners.end(), pPlayer) != ownerComponent.InvalidOwners.end())
            continue;

        if (!pPlayer->GetCellComponent().IsInRange(cellIdComponent, characterComponent.IsDragon()))
            continue;

        // Retain every owner that declined this handoff chain so the actor cannot bounce between unloaded clients.
        if (TransferOwnership(pPlayer, aEntity, aReason, false))
            return;
    }

    spdlog::debug("Removing actor {:X} after {} because no eligible owner remains", World::ToInteger(aEntity), pReasonName);
    m_world.GetDispatcher().trigger(CharacterRemoveEvent(World::ToInteger(aEntity)));
}

ActorData CharacterService::BuildActorData(const entt::entity acEntity) const noexcept
{
    ActorData actorData{};

    const auto* pActorValuesComponent = m_world.try_get<ActorValuesComponent>(acEntity);
    if (pActorValuesComponent)
    {
        actorData.InitialActorValues = pActorValuesComponent->CurrentActorValues;
    }

    const auto* pInventoryComponent = m_world.try_get<InventoryComponent>(acEntity);
    if (pInventoryComponent)
    {
        actorData.InitialInventory = pInventoryComponent->Content;
    }

    actorData.IsDead = false;
    const auto* pCharacterComponent = m_world.try_get<CharacterComponent>(acEntity);
    if (pCharacterComponent)
    {
        actorData.IsDead = pCharacterComponent->IsDead();
        actorData.IsWeaponDrawn = pCharacterComponent->IsWeaponDrawn();
    }

    return actorData;
}

void CharacterService::ProcessFactionsChanges() const noexcept
{
    static std::chrono::steady_clock::time_point lastSendTimePoint;
    constexpr auto cDelayBetweenSnapshots = 2000ms;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSendTimePoint < cDelayBetweenSnapshots)
        return;

    lastSendTimePoint = now;

    const auto characterView = m_world.view<CellIdComponent, CharacterComponent, OwnerComponent>();

    TiltedPhoques::Map<Player*, NotifyFactionsChanges> messages;

    for (auto entity : characterView)
    {
        auto& characterComponent = characterView.get<CharacterComponent>(entity);
        auto& cellIdComponent = characterView.get<CellIdComponent>(entity);
        auto& ownerComponent = characterView.get<OwnerComponent>(entity);

        // If we have nothing new to send skip this
        if (!characterComponent.IsDirtyFactions())
            continue;

        for (auto pPlayer : m_world.GetPlayerManager())
        {
            if (pPlayer == ownerComponent.GetOwner())
                continue;

            if (!cellIdComponent.IsInRange(pPlayer->GetCellComponent(), characterComponent.IsDragon()))
                continue;

            auto& message = messages[pPlayer];
            auto& change = message.Changes[World::ToInteger(entity)];

            change = characterComponent.FactionsContent;
        }

        characterComponent.SetDirtyFactions(false);
    }

    for (auto [pPlayer, message] : messages)
    {
        if (!message.Changes.empty())
            pPlayer->Send(message);
    }
}
