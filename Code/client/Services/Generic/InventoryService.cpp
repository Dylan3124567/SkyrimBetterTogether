#include <Services/InventoryService.h>

#include <Messages/RequestObjectInventoryChanges.h>
#include <Messages/NotifyObjectInventoryChanges.h>
#include <Messages/RequestInventoryChanges.h>
#include <Messages/NotifyInventoryChanges.h>
#include <Messages/RequestEquipmentChanges.h>
#include <Messages/NotifyEquipmentChanges.h>
#include <Messages/DrawWeaponRequest.h>
#include <Messages/NotifyDrawWeapon.h>
#include <Messages/DroppedItems.h>

#include <Events/UpdateEvent.h>
#include <Events/InventoryChangeEvent.h>
#include <Events/EquipmentChangeEvent.h>
#include <Events/DroppedItemEvent.h>
#include <Events/DroppedItemPickUpEvent.h>
#include <Events/DisconnectedEvent.h>
#include <Events/CellChangeEvent.h>

#include <World.h>
#include <Games/Skyrim/Interface/UI.h>
#include <PlayerCharacter.h>
#include <Forms/TESObjectCELL.h>
#include <Actor.h>
#include <Structs/ObjectData.h>
#include <Forms/TESWorldSpace.h>
#include <Games/TES.h>
#include <Games/Overrides.h>
#include <EquipManager.h>
#include <Games/ActorExtension.h>
#include <Forms/TESNPC.h>
#include <Forms/TESBoundObject.h>
#include <DefaultObjectManager.h>
#include <Systems/DropRules.h>
#include <Systems/SyncRules.h>

namespace
{
void RemoveDropReference(TESObjectREFR* apReference) noexcept
{
    apReference->DisableImpl();

    if (apReference->IsTemporary())
        apReference->Delete();
}
} // namespace

InventoryService::InventoryService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_dispatcher(aDispatcher)
    , m_transport(aTransport)
{
    m_updateConnection = m_dispatcher.sink<UpdateEvent>().connect<&InventoryService::OnUpdate>(this);
    m_droppedItemConnection = m_dispatcher.sink<DroppedItemEvent>().connect<&InventoryService::OnDroppedItem>(this);
    m_droppedItemPickUpConnection = m_dispatcher.sink<DroppedItemPickUpEvent>().connect<&InventoryService::OnDroppedItemPickUp>(this);
    m_disconnectedConnection = m_dispatcher.sink<DisconnectedEvent>().connect<&InventoryService::OnDisconnected>(this);
    m_cellChangeConnection = m_dispatcher.sink<CellChangeEvent>().connect<&InventoryService::OnCellChange>(this);
    m_notifyDroppedItemsConnection = m_dispatcher.sink<NotifyDroppedItems>().connect<&InventoryService::OnNotifyDroppedItems>(this);
    m_notifyDroppedItemRemovedConnection = m_dispatcher.sink<NotifyDroppedItemRemoved>().connect<&InventoryService::OnNotifyDroppedItemRemoved>(this);
    m_inventoryConnection = m_dispatcher.sink<InventoryChangeEvent>().connect<&InventoryService::OnInventoryChangeEvent>(this);
    m_equipmentConnection = m_dispatcher.sink<EquipmentChangeEvent>().connect<&InventoryService::OnEquipmentChangeEvent>(this);
    m_inventoryChangeConnection = m_dispatcher.sink<NotifyInventoryChanges>().connect<&InventoryService::OnNotifyInventoryChanges>(this);
    m_equipmentChangeConnection = m_dispatcher.sink<NotifyEquipmentChanges>().connect<&InventoryService::OnNotifyEquipmentChanges>(this);
}

void InventoryService::OnUpdate(const UpdateEvent& acUpdateEvent) noexcept
{
    RunWeaponStateUpdates();
    RunDropChecks();
    RunSharedDropUpdates();
}

void InventoryService::OnDroppedItem(const DroppedItemEvent& acEvent) noexcept
{
    m_dropChecks.push_back({acEvent.Handle, acEvent.CellId, acEvent.ExpectedPosition, std::chrono::steady_clock::now() + 5s, 0, acEvent.Item});
}

void InventoryService::RunDropChecks() noexcept
{
    if (m_dropChecks.empty())
        return;

    constexpr uint32_t cMaxCorrections = 3;
    const auto cNow = std::chrono::steady_clock::now();

    std::erase_if(m_dropChecks, [this, cNow](DropCheck& aItem) {
        auto* pItem = TESObjectREFR::GetByHandle(aItem.Handle);
        auto* pCell = Cast<TESObjectCELL>(TESForm::GetById(aItem.CellId));

        // Handles go stale when an item gets picked up, the rest of the time it's safe to leave it alone
        if (!pItem || !pCell || cNow >= aItem.Deadline || aItem.Corrections >= cMaxCorrections)
            return true;

        if (SyncRules::IsDropMisplaced(pItem->position, aItem.ExpectedPosition))
        {
            spdlog::warn("Dropped item {:X} landed at ({}, {}, {}), moving it to ({}, {}, {})", pItem->formID, pItem->position.x, pItem->position.y, pItem->position.z, aItem.ExpectedPosition.x, aItem.ExpectedPosition.y, aItem.ExpectedPosition.z);

            // MoveTo() disables and enables anything that isn't an actor, which throws away the 3D of a physics object and
            // leaves building a new one to a queue. Only an item with nothing to lose can go through there.
            if (pItem->GetNiNode())
                pItem->WarpTo(aItem.ExpectedPosition);
            else
                pItem->MoveTo(pCell, aItem.ExpectedPosition);

            ++aItem.Corrections;
        }

        if (aItem.Item.Count > 0)
        {
            ShareDrop(*pItem, *pCell, aItem.Item);
            aItem.Item.Count = 0;
        }

        return false;
    });
}

void InventoryService::ShareDrop(const TESObjectREFR& acItem, const TESObjectCELL& acCell, const Inventory::Entry& acEntry) noexcept
{
    if (!m_transport.IsConnected())
        return;

    auto& modSystem = m_world.GetModSystem();

    RequestItemDrop request;
    request.Item.Item = acEntry;
    request.Item.Position = acItem.position;
    request.Item.Coords = GridCellCoords::CalculateGridCellCoords(acItem.position.x, acItem.position.y);

    if (!modSystem.GetServerModId(acCell.formID, request.Item.CellId))
    {
        spdlog::error("Server cell id not found for cell form id {:X}", acCell.formID);
        return;
    }

    if (!(acCell.cellFlags & 1) && !modSystem.GetServerModId(acCell.worldspace->formID, request.Item.WorldSpaceId))
    {
        spdlog::error("Server world space id not found for world space form id {:X}", acCell.worldspace->formID);
        return;
    }

    const uint32_t cHandle = acItem.GetHandle().handle.iBits;

    request.Item.Cookie = m_nextDropCookie++;
    m_pendingDrops[request.Item.Cookie] = cHandle;
    SetSharedReference(cHandle, 0);

    m_transport.Send(request);

    spdlog::info("Sharing dropped item {:X}, item: {:X}, count: {}", acItem.formID, acEntry.BaseId.BaseId, acEntry.Count);
}

bool InventoryService::IsSharedDrop(const TESObjectREFR* apObject) const noexcept
{
    if (!apObject)
        return false;

    const uint32_t cHandle = apObject->GetHandle().handle.iBits;

    std::scoped_lock _{m_sharedReferencesLock};
    return m_sharedReferences.find(cHandle) != m_sharedReferences.end();
}

bool InventoryService::DeferDropPickUp(const TESObjectREFR* apObject, int32_t aCount, bool aUnk1, bool aUnk2) noexcept
{
    if (!IsSharedDrop(apObject))
        return false;

    World::Get().GetRunner().Trigger(DroppedItemPickUpEvent(apObject->GetHandle().handle.iBits, aCount, aUnk1, aUnk2));
    return true;
}

void InventoryService::SetSharedReference(uint32_t aHandle, uint32_t aDropId) noexcept
{
    std::scoped_lock _{m_sharedReferencesLock};
    m_sharedReferences[aHandle] = aDropId;
}

void InventoryService::ForgetSharedReference(uint32_t aHandle) noexcept
{
    std::scoped_lock _{m_sharedReferencesLock};
    m_sharedReferences.erase(aHandle);
}

void InventoryService::OnDroppedItemPickUp(const DroppedItemPickUpEvent& acEvent) noexcept
{
    const auto it = std::find_if(m_sharedDrops.begin(), m_sharedDrops.end(), [&acEvent](const SharedDrop& acDrop) { return acDrop.Handle == acEvent.Handle; });

    // The item has no ID until the server has answered the dropper. That takes less time than closing the inventory does.
    if (it == m_sharedDrops.end() || it->PickUpRequested)
        return;

    it->PickUpRequested = true;
    it->PickUpCount = acEvent.Count;
    it->PickUpUnk1 = acEvent.Unk1;
    it->PickUpUnk2 = acEvent.Unk2;

    RequestDroppedItemPickUp request;
    request.Id = it->Data.Id;

    m_transport.Send(request);
}

void InventoryService::OnNotifyDroppedItems(const NotifyDroppedItems& acMessage) noexcept
{
    for (const DroppedItem& item : acMessage.Items)
    {
        if (std::any_of(m_sharedDrops.begin(), m_sharedDrops.end(), [&item](const SharedDrop& acDrop) { return acDrop.Data.Id == item.Id; }))
            continue;

        SharedDrop drop{item, 0, false, false, 0, false, false};

        if (const auto it = m_pendingDrops.find(item.Cookie); it != m_pendingDrops.end())
        {
            drop.Handle = it->second;
            drop.IsOwnReference = true;
            SetSharedReference(drop.Handle, item.Id);
            m_pendingDrops.erase(it);
        }

        m_sharedDrops.push_back(std::move(drop));
    }

    m_nextSharedDropUpdate = {};
}

void InventoryService::OnNotifyDroppedItemRemoved(const NotifyDroppedItemRemoved& acMessage) noexcept
{
    const auto it = std::find_if(m_sharedDrops.begin(), m_sharedDrops.end(), [&acMessage](const SharedDrop& acDrop) { return acDrop.Data.Id == acMessage.Id; });
    if (it == m_sharedDrops.end())
        return;

    const SharedDrop drop = std::move(*it);
    m_sharedDrops.erase(it);

    // No longer shared, which is also what lets the pick up below through
    ForgetSharedReference(drop.Handle);

    TESObjectREFR* pReference = drop.Handle ? TESObjectREFR::GetByHandle(drop.Handle) : nullptr;

    if (!acMessage.Granted)
    {
        if (pReference)
            RemoveDropReference(pReference);

        return;
    }

    PlayerCharacter* pPlayer = PlayerCharacter::Get();

    if (pReference && DropRules::CanPickUpReference(drop.IsOwnReference, drop.Data.Item.ContainsExtraData()))
    {
        pPlayer->PickUpSharedDrop(pReference, drop.PickUpCount, drop.PickUpUnk1, drop.PickUpUnk2);
        return;
    }

    if (pReference)
        RemoveDropReference(pReference);

    {
        ScopedInventoryOverride _;
        pPlayer->AddOrRemoveItem(drop.Data.Item);
    }

    if (const auto cToken = Utils::GetLocalOwnershipToken(pPlayer->formID))
    {
        InventoryChangeEvent event(pPlayer->formID, drop.Data.Item);
        event.ServerId = cToken->ServerId;
        event.OwnershipEpoch = cToken->OwnershipEpoch;
        OnInventoryChangeEvent(event);
    }
}

void InventoryService::OnCellChange(const CellChangeEvent& acEvent) noexcept
{
    if (!m_transport.IsConnected())
        return;

    RequestDroppedItems request;
    request.CellId = acEvent.CellId;
    request.WorldSpaceId = acEvent.WorldSpaceId;
    request.Coords = acEvent.CurrentCoords;

    m_transport.Send(request);
}

void InventoryService::OnDisconnected(const DisconnectedEvent& acEvent) noexcept
{
    // The dropper's references stay, as ordinary items. Without a server nothing stops a copy being taken as well.
    for (const SharedDrop& drop : m_sharedDrops)
    {
        if (drop.IsOwnReference || !drop.Handle)
            continue;

        if (TESObjectREFR* pReference = TESObjectREFR::GetByHandle(drop.Handle))
            RemoveDropReference(pReference);
    }

    m_sharedDrops.clear();
    m_pendingDrops.clear();

    std::scoped_lock _{m_sharedReferencesLock};
    m_sharedReferences.clear();
}

void InventoryService::RunSharedDropUpdates() noexcept
{
    if (m_sharedDrops.empty())
        return;

    const auto cNow = std::chrono::steady_clock::now();
    if (cNow < m_nextSharedDropUpdate)
        return;

    m_nextSharedDropUpdate = cNow + 1s;

    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    TESObjectCELL* pCell = pPlayer->GetParentCellEx();
    if (!pCell)
        return;

    auto& modSystem = m_world.GetModSystem();
    TESWorldSpace* pWorldSpace = pPlayer->GetWorldSpace();

    GameId cellId{};
    GameId worldSpaceId{};
    if (!modSystem.GetServerModId(pCell->formID, cellId) || (pWorldSpace && !modSystem.GetServerModId(pWorldSpace->formID, worldSpaceId)))
        return;

    for (SharedDrop& drop : m_sharedDrops)
    {
        if (drop.Handle)
        {
            if (TESObjectREFR::GetByHandle(drop.Handle))
                continue;

            // Cells forget temporary references when they unload. The item is still out there, so it can come back as a copy.
            ForgetSharedReference(drop.Handle);
            drop.Handle = 0;
            drop.IsOwnReference = false;
            drop.PickUpRequested = false;
        }

        const DroppedItem& item = drop.Data;
        if (!DropRules::ShouldShowDrop(!item.WorldSpaceId, item.CellId == cellId, item.WorldSpaceId == worldSpaceId, item.Position, pPlayer->position))
            continue;

        auto* pObject = Cast<TESBoundObject>(TESForm::GetById(modSystem.GetGameId(item.Item.BaseId)));
        if (!pObject)
        {
            spdlog::debug("Dropped item {:X}:{:X} doesn't exist on this client", item.Item.BaseId.ModId, item.Item.BaseId.BaseId);
            continue;
        }

        TESObjectREFR* pReference = TESObjectREFR::PlaceItem(pObject, item.Item.Count, pCell, pWorldSpace, item.Position);
        if (!pReference)
        {
            spdlog::warn("Failed to place dropped item {:X} at ({}, {}, {})", pObject->formID, item.Position.x, item.Position.y, item.Position.z);
            continue;
        }

        drop.Handle = pReference->GetHandle().handle.iBits;
        SetSharedReference(drop.Handle, item.Id);

        spdlog::info("Placed dropped item {:X} as {:X} at ({}, {}, {})", pObject->formID, pReference->formID, item.Position.x, item.Position.y, item.Position.z);
    }
}

void InventoryService::OnInventoryChangeEvent(const InventoryChangeEvent& acEvent) noexcept
{
    if (!m_transport.IsConnected())
        return;

    auto view = m_world.view<FormIdComponent>();

    const auto iter = std::find_if(std::begin(view), std::end(view), [view, formId = acEvent.FormId](auto entity) { return view.get<FormIdComponent>(entity).Id == formId; });

    if (iter == std::end(view))
        return;

    uint32_t serverId = 0;
    if (acEvent.OwnershipEpoch != 0)
    {
        const auto* pLocalComponent = m_world.try_get<LocalComponent>(*iter);
        const auto* pRemoteComponent = m_world.try_get<RemoteComponent>(*iter);
        const bool ownershipMatches = (pLocalComponent && pLocalComponent->Id == acEvent.ServerId && pLocalComponent->OwnershipEpoch == acEvent.OwnershipEpoch)
            || (pRemoteComponent && pRemoteComponent->Id == acEvent.ServerId && pRemoteComponent->OwnershipEpoch == acEvent.OwnershipEpoch);

        if (!ownershipMatches)
        {
            spdlog::debug("Discarded an inventory change for actor {:X} because ownership changed after it was queued (epoch {})", acEvent.ServerId, acEvent.OwnershipEpoch);
            return;
        }

        serverId = acEvent.ServerId;
    }
    else
    {
        if (Cast<Actor>(TESForm::GetById(acEvent.FormId)))
            return;

        const std::optional<uint32_t> serverIdRes = Utils::GetServerId(*iter);
        if (!serverIdRes)
        {
            spdlog::warn(
                "Discarded inventory change for form {:X} because it has no server entity (item {:X}, count {})", acEvent.FormId, acEvent.Item.BaseId.BaseId, acEvent.Item.Count);
            return;
        }
        serverId = *serverIdRes;
    }

    RequestInventoryChanges request;
    request.ServerId = serverId;
    request.OwnershipEpoch = acEvent.OwnershipEpoch;
    request.Item = acEvent.Item;
    request.Drop = acEvent.Drop;
    request.UpdateClients = acEvent.UpdateClients;

    m_transport.Send(request);

    spdlog::info("Sending item request, item: {:X}, count: {}, target object: {:X}", acEvent.Item.BaseId.BaseId, acEvent.Item.Count, acEvent.FormId);
}

void InventoryService::OnEquipmentChangeEvent(const EquipmentChangeEvent& acEvent) noexcept
{
    if (!m_transport.IsConnected())
        return;

    auto view = m_world.view<FormIdComponent>();

    const auto iter = std::find_if(std::begin(view), std::end(view), [view, formId = acEvent.ActorId](auto entity) { return view.get<FormIdComponent>(entity).Id == formId; });

    if (iter == std::end(view))
        return;

    const auto* pLocalComponent = m_world.try_get<LocalComponent>(*iter);
    if (acEvent.OwnershipEpoch == 0 || !pLocalComponent || pLocalComponent->Id != acEvent.ServerId || pLocalComponent->OwnershipEpoch != acEvent.OwnershipEpoch)
    {
        spdlog::debug("Discarded an equipment change for actor {:X} because ownership changed after it was queued (epoch {})", acEvent.ServerId, acEvent.OwnershipEpoch);
        return;
    }

    Actor* pActor = Cast<Actor>(TESForm::GetById(acEvent.ActorId));
    if (!pActor)
        return;

    auto& modSystem = World::Get().GetModSystem();

    RequestEquipmentChanges request;
    request.ServerId = acEvent.ServerId;
    request.OwnershipEpoch = acEvent.OwnershipEpoch;

    if (!modSystem.GetServerModId(acEvent.EquipSlotId, request.EquipSlotId))
        return;
    if (!modSystem.GetServerModId(acEvent.ItemId, request.ItemId))
        return;

    request.Count = acEvent.Count;
    request.Unequip = acEvent.Unequip;
    request.IsSpell = acEvent.IsSpell;
    request.IsShout = acEvent.IsShout;
    request.IsAmmo = acEvent.IsAmmo;
    request.CurrentInventory = pActor->GetEquipment();

    m_transport.Send(request);

    spdlog::info("Sending equipment request, item: {:X}, count: {}, target object: {:X}", acEvent.ItemId, acEvent.Count, acEvent.ActorId);
}

void InventoryService::OnNotifyInventoryChanges(const NotifyInventoryChanges& acMessage) noexcept
{
    if (acMessage.OwnershipEpoch != 0)
    {
        Actor* pActor = nullptr;

        auto remoteView = m_world.view<RemoteComponent, FormIdComponent>(entt::exclude<LocalComponent>);
        const auto remoteIt = std::find_if(remoteView.begin(), remoteView.end(), [remoteView, &acMessage](const entt::entity aEntity)
        {
            const auto& remoteComponent = remoteView.get<RemoteComponent>(aEntity);
            return remoteComponent.Id == acMessage.ServerId && remoteComponent.OwnershipEpoch == acMessage.OwnershipEpoch;
        });

        if (remoteIt != remoteView.end())
            pActor = Cast<Actor>(TESForm::GetById(remoteView.get<FormIdComponent>(*remoteIt).Id));
        else
        {
            auto localView = m_world.view<LocalComponent, FormIdComponent>();
            const auto localIt = std::find_if(localView.begin(), localView.end(), [localView, &acMessage](const entt::entity aEntity)
            {
                const auto& localComponent = localView.get<LocalComponent>(aEntity);
                return localComponent.Id == acMessage.ServerId && localComponent.OwnershipEpoch == acMessage.OwnershipEpoch;
            });

            if (localIt != localView.end())
                pActor = Cast<Actor>(TESForm::GetById(localView.get<FormIdComponent>(*localIt).Id));
        }

        if (!pActor)
        {
            spdlog::debug("Discarded an inventory update for actor {:X} because epoch {} is no longer current", acMessage.ServerId, acMessage.OwnershipEpoch);
            return;
        }

        ScopedInventoryOverride _;

        if (acMessage.Drop)
            pActor->DropOrPickUpObject(acMessage.Item, nullptr, nullptr);
        else
            pActor->AddOrRemoveItem(acMessage.Item);

        return;
    }

    TESObjectREFR* pObject = Utils::GetByServerId<TESObjectREFR>(acMessage.ServerId);
    if (!pObject)
        return;

    ScopedInventoryOverride _;
    pObject->AddOrRemoveItem(acMessage.Item);
}

void InventoryService::OnNotifyEquipmentChanges(const NotifyEquipmentChanges& acMessage) noexcept
{
    auto view = m_world.view<RemoteComponent, FormIdComponent>(entt::exclude<LocalComponent>);
    const auto it = std::find_if(view.begin(), view.end(), [view, &acMessage](const entt::entity aEntity)
    {
        const auto& remoteComponent = view.get<RemoteComponent>(aEntity);
        return remoteComponent.Id == acMessage.ServerId && remoteComponent.OwnershipEpoch == acMessage.OwnershipEpoch;
    });
    if (it == view.end())
    {
        spdlog::debug("Discarded an equipment update for actor {:X} because epoch {} is no longer current", acMessage.ServerId, acMessage.OwnershipEpoch);
        return;
    }

    Actor* pActor = Cast<Actor>(TESForm::GetById(view.get<FormIdComponent>(*it).Id));
    if (!pActor)
        return;

    auto& modSystem = World::Get().GetModSystem();

    uint32_t itemId = modSystem.GetGameId(acMessage.ItemId);
    TESForm* pItem = TESForm::GetById(itemId);

    if (!pItem)
    {
        spdlog::error("Could not find inventory item {:X}:{:X}", acMessage.ItemId.ModId, acMessage.ItemId.BaseId);
        return;
    }

    uint32_t equipSlotId = modSystem.GetGameId(acMessage.EquipSlotId);
    TESForm* pEquipSlot = TESForm::GetById(equipSlotId);

    uint32_t slotId = 0;
    if (pEquipSlot == DefaultObjectManager::Get().rightEquipSlot)
        slotId = 1;

    auto* pEquipManager = EquipManager::Get();

    if (acMessage.IsSpell)
    {
        if (acMessage.Unequip)
            pEquipManager->UnEquipSpell(pActor, pItem, slotId);
        else
            pEquipManager->EquipSpell(pActor, pItem, slotId);

        return;
    }
    else if (acMessage.IsShout)
    {
        if (acMessage.Unequip)
            pEquipManager->UnEquipShout(pActor, pItem);
        else
            pEquipManager->EquipShout(pActor, pItem);

        return;
    }

    // TODO: ExtraData necessary? probably
    if (acMessage.Unequip)
    {
        pEquipManager->UnEquip(pActor, pItem, nullptr, acMessage.Count, pEquipSlot, false, true, false, false, nullptr);
    }
    else
    {
        // Unequip all armor first, since the game won't auto unequip armor
        Inventory wornArmor{};
        if (pItem->formType == FormType::Armor)
        {
            wornArmor = pActor->GetWornArmor();
            for (const auto& armor : wornArmor.Entries)
            {
                uint32_t armorId = modSystem.GetGameId(armor.BaseId);
                TESForm* pArmor = TESForm::GetById(armorId);
                if (pArmor)
                    pEquipManager->UnEquip(pActor, pArmor, nullptr, 1, pEquipSlot, false, true, false, false, nullptr);
            }
        }

        pEquipManager->Equip(pActor, pItem, nullptr, acMessage.Count, pEquipSlot, false, true, false, false);

        for (const auto& armor : wornArmor.Entries)
        {
            uint32_t armorId = modSystem.GetGameId(armor.BaseId);
            TESForm* pArmor = TESForm::GetById(armorId);
            if (pArmor)
                pEquipManager->Equip(pActor, pArmor, nullptr, 1, pEquipSlot, false, true, false, false);
        }
    }
}

void InventoryService::RunWeaponStateUpdates() noexcept
{
    if (!m_transport.IsConnected())
        return;

    static std::chrono::steady_clock::time_point lastSendTimePoint;
    constexpr auto cDelayBetweenUpdates = 500ms;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSendTimePoint < cDelayBetweenUpdates)
        return;

    lastSendTimePoint = now;

    auto view = m_world.view<FormIdComponent, LocalComponent>();

    for (auto entity : view)
    {
        const auto& formIdComponent = view.get<FormIdComponent>(entity);
        Actor* const pActor = Cast<Actor>(TESForm::GetById(formIdComponent.Id));
        auto& localComponent = view.get<LocalComponent>(entity);

        bool isWeaponDrawn = pActor->actorState.IsWeaponDrawn();
        if (isWeaponDrawn != localComponent.IsWeaponDrawn)
        {
            localComponent.IsWeaponDrawn = isWeaponDrawn;

            DrawWeaponRequest request;
            request.Id = localComponent.Id;
            request.IsWeaponDrawn = isWeaponDrawn;

            m_transport.Send(request);
        }
    }
}
