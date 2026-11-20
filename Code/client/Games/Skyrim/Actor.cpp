#include <Games/References.h>
#include <Games/Skyrim/EquipManager.h>
#include <AI/AIProcess.h>
#include <Misc/MiddleProcess.h>
#include <Misc/GameVM.h>
#include <DefaultObjectManager.h>
#include <Forms/TESNPC.h>
#include <Forms/TESFaction.h>
#include <Forms/TESQuest.h>
#include <Components/TESActorBaseData.h>
#include <ExtraData/ExtraFactionChanges.h>
#include <ExtraData/ExtraLeveledCreature.h>
#include <Games/Memory.h>
#include <Combat/CombatController.h>

#include <Events/HealthChangeEvent.h>
#include <Events/InventoryChangeEvent.h>
#include <Events/MountEvent.h>
#include <Events/DialogueEvent.h>
#include <Games/Misc/MenuTopicManager.h>
#include <Events/HitEvent.h>
#include <Events/RemoveSpellEvent.h>

#include <Games/TES.h>
#include <World.h>
#include <Services/PapyrusService.h>

#include <Forms/ActorValueInfo.h>
#include <Forms/TESRace.h>

#include <Effects/ValueModifierEffect.h>

#include <Games/Skyrim/Misc/InventoryEntry.h>
#include <Games/Skyrim/ExtraData/ExtraCount.h>
#include <Games/Misc/ActorKnowledge.h>

#include <ExtraData/ExtraDataList.h>
#include <ExtraData/ExtraCharge.h>
#include <ExtraData/ExtraCount.h>
#include <ExtraData/ExtraEnchantment.h>
#include <ExtraData/ExtraHealth.h>
#include <ExtraData/ExtraPoison.h>
#include <ExtraData/ExtraSoul.h>
#include <ExtraData/ExtraTextDisplayData.h>
#include <Forms/EnchantmentItem.h>
#include <Forms/AlchemyItem.h>
#include <Forms/TESObjectCELL.h>

#include <Structs/Skyrim/AnimationGraphDescriptor_BHR_Master.h>

#include <Games/Overrides.h>
#include <Games/Skyrim/BSAnimationGraphManager.h>
#include <Havok/hkbStateMachine.h>
#include <Havok/hkbBehaviorGraph.h>
#include <Havok/bhkCharacterController.h>

#include <ModCompat/BehaviorVar.h>

#include <Events/DroppedItemEvent.h>
#include <Camera/PlayerCamera.h>
#include <Services/InventoryService.h>
#include <Systems/DropRules.h>
#include <Systems/RespawnFlow.h>
#include <Systems/SyncRules.h>
#include <Systems/NpcSyncRules.h>
#include <Systems/CombatRules.h>
#include <Services/ActorValueService.h>

namespace
{
// The engine centers the 3D of a dropped item on its drop position using the item's world bound, so a stale bound throws
// the item anywhere, up to twice the distance from the world origin. Check where it really ended up once it has settled.
void QueueDropCheck(const Actor* apActor, const BSPointerHandle<TESObjectREFR>& acHandle, const NiPoint3* apLocation, Inventory::Entry aItem = {})
{
    if (!acHandle.handle.iBits || !apActor->parentCell)
        return;

    const glm::vec3 cExpected = apLocation ? glm::vec3(*apLocation) : SyncRules::GetDropPosition(apActor->position, apActor->rotation.z);
    World::Get().GetRunner().Trigger(DroppedItemEvent(acHandle.handle.iBits, apActor->parentCell->formID, cExpected, std::move(aItem)));
}

void QueueActorInventoryChange(Actor* apActor, InventoryChangeEvent aEvent, TESObjectREFR* apTransferReference = nullptr)
{
    auto ownershipToken = Utils::GetLocalOwnershipToken(apActor->formID);
    if (!ownershipToken && apTransferReference == PlayerCharacter::Get())
        ownershipToken = Utils::GetRemoteOwnershipToken(apActor->formID);

    if (!ownershipToken)
        return;

    aEvent.ServerId = ownershipToken->ServerId;
    aEvent.OwnershipEpoch = ownershipToken->OwnershipEpoch;
    World::Get().GetRunner().Trigger(std::move(aEvent));
}
}

#ifdef SAVE_STUFF

#include <Games/Skyrim/SaveLoad.h>
#include "Actor.h"

void Actor::Save_Reversed(const uint32_t aChangeFlags, Buffer::Writer& aWriter)
{
    BGSSaveFormBuffer buffer;

    Save(&buffer);

    AIProcess* pProcess = currentProcess;
    const int32_t handlerId = pProcess != nullptr ? pProcess->handlerId : -1;

    aWriter.WriteBytes((uint8_t*)&handlerId, 4); // TODO: is this needed ?
    aWriter.WriteBytes((uint8_t*)&flags1, 4);

    //     if (!handlerId
    //         && (uint8_t)AIProcess::GetBoolInSubStructure(pProcess))
    //     {
    //         Actor::SaveSkinFar(this);
    //     }

    TESObjectREFR::Save_Reversed(aChangeFlags, aWriter);

    if (pProcess)
        ; // Skyrim saves the process manager state, but we don't give a shit so skip !

    aWriter.WriteBytes((uint8_t*)&unk194, 4);
    aWriter.WriteBytes((uint8_t*)&headTrackingUpdateDelay, 4);
    aWriter.WriteBytes((uint8_t*)&unk9C, 4);
    // We skip 0x180 as it's not something we care about, some timer related data

    aWriter.WriteBytes((uint8_t*)&unk98, 4);
    // skip A8 - related to timers
    // skip AC - related to timers as well
    aWriter.WriteBytes((uint8_t*)&unkB0, 4);
    // skip E4 - never seen this used
    // skip E8 - same as E4
    aWriter.WriteBytes((uint8_t*)&unk84, 4);
    aWriter.WriteBytes((uint8_t*)&unkA4, 4);
    // skip baseForm->weight
    // skip 12C

    // Save actor state sub_6F0FB0
}

#endif

TP_THIS_FUNCTION(TRemoveSpell, bool, Actor, MagicItem*);
TP_THIS_FUNCTION(TCharacterConstructor, Actor*, Actor);
TP_THIS_FUNCTION(TCharacterConstructor2, Actor*, Actor, uint8_t aUnk);
TP_THIS_FUNCTION(TCharacterDestructor, Actor*, Actor);
TP_THIS_FUNCTION(TAddInventoryItem, void, Actor, TESBoundObject* apItem, ExtraDataList* apExtraData, int32_t aCount, TESObjectREFR* apOldOwner);
TP_THIS_FUNCTION(TPickUpObject, void*, Actor, TESObjectREFR* apObject, int32_t aCount, bool aUnk1, float aUnk2);
TP_THIS_FUNCTION(TDropObject, void*, Actor, void* apResult, TESBoundObject* apObject, ExtraDataList* apExtraData, int32_t aCount, NiPoint3* apLocation, NiPoint3* apRotation);
TP_THIS_FUNCTION(TSetPosition, char, Actor, NiPoint3& acPosition);
TP_THIS_FUNCTION(TActorProcess, char, Actor, float aValue);

using TGetLocation = TESForm*(TESForm*);
static TGetLocation* FUNC_GetActorLocation;

TCharacterConstructor* RealCharacterConstructor;
TCharacterConstructor2* RealCharacterConstructor2;
TCharacterDestructor* RealCharacterDestructor;

static TRemoveSpell* RealRemoveSpell = nullptr;
static TAddInventoryItem* RealAddInventoryItem = nullptr;
static TPickUpObject* RealPickUpObject = nullptr;
static TDropObject* RealDropObject = nullptr;
static TSetPosition* RealSetPosition = nullptr;
static TActorProcess* RealActorProcess = nullptr;

float Actor::GetSpeed() noexcept
{
    static BSFixedString speedSampledStr("SpeedSampled");
    float speed = 0.f;
    animationGraphHolder.GetVariableFloat(&speedSampledStr, &speed);

    return speed;
}

void Actor::SetSpeed(float aSpeed) noexcept
{
    static BSFixedString speedSampledStr("SpeedSampled");
    animationGraphHolder.SetVariableFloat(&speedSampledStr, aSpeed);
}

TESNPC* Actor::GetLeveledPick() const noexcept
{
    const auto* pExtra = static_cast<ExtraLeveledCreature*>(extraData.GetByType(ExtraDataType::LeveledCreature));
    TESActorBase* pTemplate = pExtra ? pExtra->templateBase : nullptr;
    if (!pTemplate || pTemplate->formType != FormType::Npc || pTemplate->IsTemporary())
        return nullptr;

    return static_cast<TESNPC*>(pTemplate);
}

uint16_t Actor::GetLevel() const noexcept
{
    TP_THIS_FUNCTION(TGetLevel, uint16_t, const Actor);
    POINTER_SKYRIMSE(TGetLevel, s_getLevel, 37334);
    return TiltedPhoques::ThisCall(s_getLevel, this);
}

void Actor::ForcePosition(const NiPoint3& acPosition) noexcept
{
    ScopedReferencesOverride recursionGuard;

    bool updateController = true;
    if (GetExtension()->IsRemote() && currentProcess)
    {
        if (auto* pController = currentProcess->GetCharController())
        {
            // A newly created controller may be positioned before ActorProcess.
            // Both SetPositionImpl and the following velocity reset need a step
            updateController = pController->UpdateStepTiming();
        }
    }

    // With no usable step yet, update the reference/3D now; interpolation will
    // catch the controller up once physics timing becomes available
    SetPosition(acPosition, updateController);
}

void Actor::QueueUpdate() noexcept
{
    auto* pSetting = INISettingCollection::Get()->GetSetting("bUseFaceGenPreprocessedHeads:General");
    const auto originalValue = pSetting->data;
    pSetting->data = 0;

    TP_THIS_FUNCTION(TQueueUpdate, void, Actor, bool);
    POINTER_SKYRIMSE(TQueueUpdate, QueueUpdate, 40255);

    TiltedPhoques::ThisCall(QueueUpdate, this, true);

    pSetting->data = originalValue;
}

GamePtr<Actor> Actor::Create(TESNPC* apBaseForm) noexcept
{
    auto pActor = New();
    // Prevent saving
    pActor->SetSkipSaveFlag(true);
    pActor->GetExtension()->SetRemote(true);

    const auto pPlayer = static_cast<Actor*>(GetById(0x14));
    auto pCell = pPlayer->parentCell;
    const auto pWorldSpace = pPlayer->GetWorldSpace();

    pActor->SetLevelMod(4);
    pActor->MarkChanged(0x40000000);
    pActor->SetParentCell(pCell);
    pActor->SetObjectReference(apBaseForm);

    auto position = pPlayer->position;
    auto rotation = pPlayer->rotation;

    if (pCell && !(pCell->cellFlags & 1))
        pCell = nullptr;

    ModManager::Get()->Spawn(position, rotation, pCell, pWorldSpace, pActor);

    pActor->ForcePosition(position);

    pActor->GetMagicCaster(MagicSystem::CastingSource::LEFT_HAND);
    pActor->GetMagicCaster(MagicSystem::CastingSource::RIGHT_HAND);
    pActor->GetMagicCaster(MagicSystem::CastingSource::OTHER);

    pActor->flags &= 0xFFDFFFFF;

    return pActor;
}

GamePtr<Actor> Actor::Spawn(uint32_t aBaseFormId) noexcept
{
    TESNPC* pNpc = Cast<TESNPC>(TESForm::GetById(aBaseFormId));
    return Actor::Create(pNpc);
}

void Actor::SetLevelMod(uint32_t aLevel) noexcept
{
    TP_THIS_FUNCTION(TActorSetLevelMod, void, ExtraDataList, uint32_t);
    POINTER_SKYRIMSE(TActorSetLevelMod, realSetLevelMod, 11806);

    const auto pExtraDataList = &extraData;

    TiltedPhoques::ThisCall(realSetLevelMod, pExtraDataList, aLevel);
}

ActorExtension* Actor::GetExtension() noexcept
{
    if (AsExActor())
    {
        return static_cast<ActorExtension*>(AsExActor());
    }

    if (AsExPlayerCharacter())
    {
        return static_cast<ActorExtension*>(AsExPlayerCharacter());
    }

    return nullptr;
}

ExActor* Actor::AsExActor() noexcept
{
    if (formType == Type && this != PlayerCharacter::Get())
        return static_cast<ExActor*>(this);

    return nullptr;
}

ExPlayerCharacter* Actor::AsExPlayerCharacter() noexcept
{
    if (this == PlayerCharacter::Get())
        return static_cast<ExPlayerCharacter*>(this);

    return nullptr;
}

extern thread_local bool g_forceAnimation;

void Actor::SetWeaponDrawnEx(bool aDraw) noexcept
{
    spdlog::debug("Setting weapon drawn: {:X}:{}, current state: {}", formID, aDraw, actorState.IsWeaponDrawn());

    if (actorState.IsWeaponDrawn() == aDraw)
    {
        actorState.SetWeaponDrawn(!aDraw);

        spdlog::debug("Setting weapon drawn after update: {:X}:{}, current state: {}", formID, aDraw, actorState.IsWeaponDrawn());
    }

    g_forceAnimation = true;
    SetWeaponDrawn(aDraw);
    g_forceAnimation = false;
}

static thread_local bool s_execInitPackage = false;

void Actor::SetPackage(TESPackage* apPackage) noexcept
{
    s_execInitPackage = true;
    PutCreatedPackage(apPackage);
    s_execInitPackage = false;
}

void Actor::SetPlayerRespawnMode(bool aSet) noexcept
{
    SetEssentialEx(aSet);
    // Makes the player go in an unrecoverable bleedout state
    SetNoBleedoutRecovery(aSet);

    if (formID != 0x14)
    {
        auto pPlayerFaction = Cast<TESFaction>(TESForm::GetById(0xDB1));
        SetFactionRank(pPlayerFaction, 1);
    }
}

void Actor::SetEssentialEx(bool aSet) noexcept
{
    SetEssential(aSet);
    TESNPC* pBase = Cast<TESNPC>(baseForm);
    if (pBase)
        pBase->actorData.SetEssential(aSet);
}

void Actor::SetNoBleedoutRecovery(bool aSet) noexcept
{
    TP_THIS_FUNCTION(TSetNoBleedoutRecovery, void, Actor, bool);
    POINTER_SKYRIMSE(TSetNoBleedoutRecovery, s_setNoBleedoutRecovery, 38533);
    TiltedPhoques::ThisCall(s_setNoBleedoutRecovery, this, aSet);
}

void Actor::DispelAllSpells(bool aNow) noexcept
{
    magicTarget.DispelAllSpells(aNow);
}

bool Actor::IsInCombat() const noexcept
{
    PAPYRUS_FUNCTION(bool, Actor, IsInCombat);
    return s_pIsInCombat(this);
}

Actor* Actor::GetCombatTarget() const noexcept
{
    PAPYRUS_FUNCTION(Actor*, Actor, GetCombatTarget);
    return s_pGetCombatTarget(this);
}

TESObjectREFR* Actor::GetOccupiedFurniture() const noexcept
{
    if (!currentProcess)
        return nullptr;

    // AIProcess::GetOccupiedFurniture, the IsCurrentFurnitureRef condition reads it: the middle high process data's
    // furniture handle at 0x208, the null handle without that data
    TP_THIS_FUNCTION(TGetOccupiedFurniture, BSPointerHandle<TESObjectREFR>*, AIProcess, BSPointerHandle<TESObjectREFR>*);
    POINTER_SKYRIMSE(TGetOccupiedFurniture, s_getOccupiedFurniture, 39916);

    BSPointerHandle<TESObjectREFR> handle{};
    TiltedPhoques::ThisCall(s_getOccupiedFurniture, currentProcess, &handle);

    return handle ? TESObjectREFR::GetByHandle(handle.handle.iBits) : nullptr;
}

// TODO: this is a really hacky solution.
// The internal targeting system should be disabled instead.
void Actor::StartCombatEx(Actor* apTarget) noexcept
{
    if (GetCombatTarget() != apTarget)
    {
        StopCombat();
        StartCombat(apTarget);
    }
}

void Actor::SetCombatTargetEx(Actor* apTarget) noexcept
{
    if (pCombatController)
        pCombatController->SetTarget(apTarget);
}

void Actor::StartCombat(Actor* apTarget) noexcept
{
    PAPYRUS_FUNCTION(void, Actor, StartCombat, Actor*);
    s_pStartCombat(this, apTarget);
}

void Actor::StopCombat() noexcept
{
    PAPYRUS_FUNCTION(void, Actor, StopCombat);
    s_pStopCombat(this);
}

bool Actor::RemoveSpell(MagicItem* apSpell) noexcept
{
    if (!apSpell)
    {
        spdlog::error(__FUNCTION__ ": apSpell is null");
        return false;
    }
    // spdlog::info(__FUNCTION__ ": removing: {} from actor: {}", apSpell->formID, formID);
    return TiltedPhoques::ThisCall(RealRemoveSpell, this, apSpell);
}

bool Actor::HasPerk(uint32_t aPerkFormId) const noexcept
{
    return GetPerkRank(aPerkFormId) != 0;
}

uint8_t Actor::GetPerkRank(uint32_t aPerkFormId) const noexcept
{
    BGSPerk* pPerk = Cast<BGSPerk>(TESForm::GetById(aPerkFormId));
    if (!pPerk)
        return 0;

    TP_THIS_FUNCTION(TGetPerkRank, uint8_t, const Actor, BGSPerk*);
    POINTER_SKYRIMSE(TGetPerkRank, getPerkRank, 37698);

    return TiltedPhoques::ThisCall(getPerkRank, this, pPerk);
}

bool TP_MAKE_THISCALL(HookRemoveSpell, Actor, MagicItem* apSpell)
{
    bool result = TiltedPhoques::ThisCall(RealRemoveSpell, apThis, apSpell);
    if (apThis->GetExtension()->IsLocalPlayer() && result)
    {
        //spdlog::info(__FUNCTION__ ": spell: {}, ID: {} from local player", apSpell->GetName() , apSpell->formID);
       RemoveSpellEvent removalEvent;

        removalEvent.TargetId = apThis->formID;
        removalEvent.SpellId = apSpell->formID;
        World::Get().GetRunner().Trigger(removalEvent);
    }

    return result;
}

Actor* TP_MAKE_THISCALL(HookCharacterConstructor, Actor)
{
    TP_EMPTY_HOOK_PLACEHOLDER;

    TiltedPhoques::ThisCall(RealCharacterConstructor, apThis);

    return apThis;
}

Actor* TP_MAKE_THISCALL(HookCharacterConstructor2, Actor, uint8_t aUnk)
{
    TP_EMPTY_HOOK_PLACEHOLDER;

    TiltedPhoques::ThisCall(RealCharacterConstructor2, apThis, aUnk);

    return apThis;
}

Actor* TP_MAKE_THISCALL(HookCharacterDestructor, Actor)
{
    TP_EMPTY_HOOK_PLACEHOLDER;

    auto pExtension = apThis->GetExtension();

    if (pExtension)
    {
        pExtension->~ActorExtension();
    }

    TiltedPhoques::ThisCall(RealCharacterDestructor, apThis);

    return apThis;
}

GamePtr<Actor> Actor::New() noexcept
{
    auto* const pActor = Memory::Allocate<Actor>();

    TiltedPhoques::ThisCall(RealCharacterConstructor, pActor);

    return {pActor};
}

void Actor::InterruptCast(bool abRefund) noexcept
{
    TP_THIS_FUNCTION(TInterruptCast, void, Actor, bool abRefund);

    POINTER_SKYRIMSE(TInterruptCast, s_interruptCast, 38757);

    TiltedPhoques::ThisCall(s_interruptCast, this, abRefund);
}

TESForm* Actor::GetEquippedWeapon(uint32_t aSlotId) const noexcept
{
    if (currentProcess && currentProcess->middleProcess)
    {
        auto pMiddleProcess = currentProcess->middleProcess;

        if (aSlotId == 0 && pMiddleProcess->leftEquippedObject)
            return pMiddleProcess->leftEquippedObject->pObject;

        else if (aSlotId == 1 && pMiddleProcess->rightEquippedObject)
            return pMiddleProcess->rightEquippedObject->pObject;
    }

    return nullptr;
}

TESForm* Actor::GetEquippedAmmo() const noexcept
{
    if (currentProcess && currentProcess->middleProcess && currentProcess->middleProcess->ammoEquippedObject)
    {
        // TODO: rtti cast to check if is ammo object? or actually, just call AIProcess::GetCurrentAmmo()
        return currentProcess->middleProcess->ammoEquippedObject->pObject;
    }

    return nullptr;
}

// Get owner of a summon or raised corpse
Actor* Actor::GetCommandingActor() const noexcept
{
    if (currentProcess && currentProcess->middleProcess && currentProcess->middleProcess->commandingActor)
    {
        auto handle = currentProcess->middleProcess->commandingActor.handle;
        auto* pOwner = Cast<Actor>(TESObjectREFR::GetByHandle(handle.iBits));
        return pOwner;
    }

    return nullptr;
}

// Get owner of a summon or raised corpse
void Actor::SetCommandingActor(BSPointerHandle<TESObjectREFR> aCommandingActor) noexcept
{
    if (currentProcess && currentProcess->middleProcess)
    {
        currentProcess->middleProcess->commandingActor = aCommandingActor;
        flags2 |= ActorFlags::IS_COMMANDED_ACTOR;
    }
}

bool Actor::IsPlayerSummon() const noexcept
{
    const Actor* pCommandingActor = GetCommandingActor();
    return pCommandingActor && pCommandingActor->formID == 0x14;
}

TESForm* Actor::GetCurrentLocation()
{
    // we use the safe function which also
    // checks the form type
    return FUNC_GetActorLocation(this);
}

Factions Actor::GetFactions() const noexcept
{
    Factions result;

    auto& modSystem = World::Get().GetModSystem();

    auto* pNpc = Cast<TESNPC>(baseForm);
    if (pNpc)
    {
        auto& factions = pNpc->actorData.factions;

        for (auto i = 0u; i < factions.length; ++i)
        {
            Faction faction;

            modSystem.GetServerModId(factions[i].faction->formID, faction.Id);
            faction.Rank = factions[i].rank;

            result.NpcFactions.push_back(faction);
        }
    }

    auto* pChanges = Cast<ExtraFactionChanges>(extraData.GetByType(ExtraDataType::Faction));
    if (pChanges)
    {
        for (auto i = 0u; i < pChanges->entries.length; ++i)
        {
            Faction faction;

            modSystem.GetServerModId(pChanges->entries[i].faction->formID, faction.Id);
            faction.Rank = pChanges->entries[i].rank;

            result.ExtraFactions.push_back(faction);
        }
    }

    return result;
}

ActorValues Actor::GetEssentialActorValues() const noexcept
{
    ActorValues actorValues;

    int essentialValues[] = {ActorValueInfo::kHealth, ActorValueInfo::kStamina, ActorValueInfo::kMagicka};
    for (auto i : essentialValues)
    {
        float value = actorValueOwner.GetValue(i);
        actorValues.ActorValuesList.insert({i, value});
        float maxValue = actorValueOwner.GetPermanentValue(i);
        actorValues.ActorMaxValuesList.insert({i, maxValue});
    }

    return actorValues;
}

float Actor::GetActorValue(uint32_t aId) const noexcept
{
    return actorValueOwner.GetValue(aId);
}

float Actor::GetActorPermanentValue(uint32_t aId) const noexcept
{
    return actorValueOwner.GetPermanentValue(aId);
}

static bool IsLocalRoomRental(const Actor* apActor, uint32_t aId)
{
    if (aId != ActorValueInfo::kVariable09)
        return false;
    const auto* npc = Cast<TESNPC>(apActor->baseForm);
    if (npc)
        for (const auto& faction : npc->actorData.factions)
            if (faction.faction && faction.faction->formID == 0x5091B)
                return true; // RentRoomScript grants bed ownership locally; its rented flag must stay local too.
    return false;
}

void Actor::SetActorValue(uint32_t aId, float aValue) noexcept
{
    if (IsLocalRoomRental(this, aId))
        return;
    actorValueOwner.SetValue(aId, aValue);
}

void Actor::ForceActorValue(ActorValueOwner::ForceMode aMode, uint32_t aId, float aValue) noexcept
{
    if (IsLocalRoomRental(this, aId))
        return;
    float initialValue = aMode == ActorValueOwner::ForceMode::PERMANENT 
                         ? GetActorPermanentValue(aId)
                         : GetActorValue(aId);

    if (aValue == initialValue)
        return;

    actorValueOwner.ForceCurrent(aMode, aId, aValue - initialValue);
}

Inventory Actor::GetActorInventory() const noexcept
{
    Inventory inventory = GetInventory();

    inventory.CurrentMagicEquipment = GetMagicEquipment();

    return inventory;
}

MagicEquipment Actor::GetMagicEquipment() const noexcept
{
    MagicEquipment equipment;

    auto& modSystem = World::Get().GetModSystem();

    uint32_t mainId = magicItems[0] ? magicItems[0]->formID : 0;
    modSystem.GetServerModId(mainId, equipment.LeftHandSpell);

    uint32_t secondaryId = magicItems[1] ? magicItems[1]->formID : 0;
    modSystem.GetServerModId(secondaryId, equipment.RightHandSpell);

    uint32_t shoutId = equippedShout ? equippedShout->formID : 0;
    modSystem.GetServerModId(shoutId, equipment.Shout);

    return equipment;
}

Inventory Actor::GetEquipment() const noexcept
{
    Inventory inventory = GetInventory();
    inventory.RemoveByFilter([](const auto& entry) { return !entry.IsWorn(); });
    inventory.CurrentMagicEquipment = GetMagicEquipment();
    return inventory;
}

int32_t Actor::GetGoldAmount() const noexcept
{
    TP_THIS_FUNCTION(TGetGoldAmount, int32_t, const Actor);
    POINTER_SKYRIMSE(TGetGoldAmount, s_getGoldAmount, 37527);
    return TiltedPhoques::ThisCall(s_getGoldAmount, this);
}

void Actor::SetActorInventory(const Inventory& acInventory) noexcept
{
    // Something worn from a plugin only the owner has can't be added here, this copy is better off with its own gear
    // than with what's left, which is often no weapon at all.
    if (!GetExtension()->IsPlayer())
    {
        auto& modSystem = World::Get().GetModSystem();
        for (const auto& entry : acInventory.Entries)
        {
            if (entry.Count > 0 && (entry.ExtraWorn || entry.ExtraWornLeft) && !modSystem.GetGameId(entry.BaseId))
            {
                spdlog::info("Keeping inventory of actor {:X}, its owner wears {:X}:{:X} which isn't installed here", formID, entry.BaseId.ModId, entry.BaseId.BaseId);
                return;
            }
        }
    }

    spdlog::info("Setting inventory for actor {:X}", formID);

    // The UnEquipAll() that used to be here is redundant,
    // as RemoveAllItems() unequips every item if needed.
    // Placing this UnEquipAll() here seems to trigger a Skyrim bug/race.

    Inventory currentInventory = GetActorInventory();

    if (!this->GetExtension()->IsPlayer() && currentInventory.ContainsQuestItems())
        SetInventoryRetainingQuestItems(currentInventory, acInventory);
    else
        SetInventory(acInventory);

    SetMagicEquipment(acInventory.CurrentMagicEquipment);
}

void Actor::SetMagicEquipment(const MagicEquipment& acEquipment) noexcept
{
    auto* pEquipManager = EquipManager::Get();
    auto& modSystem = World::Get().GetModSystem();

    if (acEquipment.LeftHandSpell)
    {
        uint32_t mainHandWeaponId = modSystem.GetGameId(acEquipment.LeftHandSpell);
        spdlog::debug("Setting left hand spell: {:X}", mainHandWeaponId);
        pEquipManager->EquipSpell(this, TESForm::GetById(mainHandWeaponId), 0);
    }

    if (acEquipment.RightHandSpell)
    {
        uint32_t secondaryHandWeaponId = modSystem.GetGameId(acEquipment.RightHandSpell);
        spdlog::debug("Setting right hand spell: {:X}", secondaryHandWeaponId);
        pEquipManager->EquipSpell(this, TESForm::GetById(secondaryHandWeaponId), 1);
    }

    if (acEquipment.Shout)
    {
        uint32_t shoutId = modSystem.GetGameId(acEquipment.Shout);
        spdlog::debug("Setting shout: {:X}", shoutId);
        pEquipManager->EquipShout(this, TESForm::GetById(shoutId));
    }
}

void Actor::SetActorValues(const ActorValues& acActorValues) noexcept
{
    for (auto& value : acActorValues.ActorMaxValuesList)
        ForceActorValue(ActorValueOwner::ForceMode::PERMANENT, value.first, value.second);

    for (auto& value : acActorValues.ActorValuesList)
        ForceActorValue(ActorValueOwner::ForceMode::DAMAGE, value.first, value.second);
}

void Actor::SetFactions(const Factions& acFactions) noexcept
{
    // How the actor stands with this client's player stays this client's own, see NpcSyncRules::IsPlayerRelationshipFaction.
    // The rank in effect is the change if there is one, -1 meaning removed, otherwise the base's.
    Vector<std::pair<const TESFaction*, int8_t>> ownRanks;
    const auto keepOwnRank = [&ownRanks](const TESFaction* apFaction, const int8_t aRank)
    {
        if (!apFaction || !NpcSyncRules::IsPlayerRelationshipFaction(apFaction->formID))
            return;

        const auto it = std::find_if(ownRanks.begin(), ownRanks.end(), [apFaction](const auto& acEntry) { return acEntry.first == apFaction; });
        if (it != ownRanks.end())
            it->second = aRank;
        else
            ownRanks.emplace_back(apFaction, aRank);
    };

    if (const auto* pNpc = Cast<TESNPC>(baseForm))
    {
        for (const auto& faction : pNpc->actorData.factions)
            keepOwnRank(faction.faction, faction.rank);
    }

    if (const auto* pChanges = Cast<ExtraFactionChanges>(extraData.GetByType(ExtraDataType::Faction)))
    {
        for (const auto& change : pChanges->entries)
            keepOwnRank(change.faction, change.rank);
    }

    RemoveFromAllFactions();

    auto& modSystem = World::Get().GetModSystem();

    const auto applyRank = [this, &modSystem](const Faction& acEntry)
    {
        const auto* pFaction = Cast<TESFaction>(GetById(modSystem.GetGameId(acEntry.Id)));
        if (pFaction && !NpcSyncRules::IsPlayerRelationshipFaction(pFaction->formID))
            SetFactionRank(pFaction, acEntry.Rank);
    };

    for (const auto& entry : acFactions.NpcFactions)
        applyRank(entry);

    for (const auto& entry : acFactions.ExtraFactions)
        applyRank(entry);

    for (const auto& [pFaction, rank] : ownRanks)
        SetFactionRank(pFaction, rank);
}

void Actor::SetFactionRank(const TESFaction* apFaction, int8_t aRank) noexcept
{
    TP_THIS_FUNCTION(TSetFactionRankInternal, void, Actor, const TESFaction*, int8_t);

    POINTER_SKYRIMSE(TSetFactionRankInternal, s_setFactionRankInternal, 37677);

    TiltedPhoques::ThisCall(s_setFactionRankInternal, this, apFaction, aRank);
}

void Actor::SetPlayerTeammate(bool aSet) noexcept
{
    TP_THIS_FUNCTION(TSetPlayerTeammate, void, Actor, bool aSet, bool abCanDoFavor);
    POINTER_SKYRIMSE(TSetPlayerTeammate, setPlayerTeammate, 37717);
    return TiltedPhoques::ThisCall(setPlayerTeammate, this, aSet, true);
}

void Actor::UnEquipAll() noexcept
{
    EquipManager::Get()->UnequipAll(this);

    // Taken from skyrim's code shouts can be two form types apparently
    if (equippedShout && ((int)equippedShout->formType - 41) <= 1)
    {
        EquipManager::Get()->UnEquipShout(this, equippedShout);
        equippedShout = nullptr;
    }
}

void Actor::RemoveFromAllFactions() noexcept
{
    PAPYRUS_FUNCTION(void, Actor, RemoveFromAllFactions);

    s_pRemoveFromAllFactions(this);
}

TP_THIS_FUNCTION(TInitiateMountPackage, bool, Actor, Actor* apMount);
static TInitiateMountPackage* RealInitiateMountPackage = nullptr;

bool Actor::InitiateMountPackage(Actor* apMount) noexcept
{
    return TiltedPhoques::ThisCall(RealInitiateMountPackage, this, apMount);
}

void Actor::GenerateMagicCasters() noexcept
{
    using CS = MagicSystem::CastingSource;

    for (int i = 0; i < 4; i++)
    {
        if (casters[i] == nullptr)
            casters[i] = Cast<ActorMagicCaster>(GetMagicCaster(static_cast<CS>(i)));
    }
}

bool Actor::IsDead() const noexcept
{
    PAPYRUS_FUNCTION(bool, Actor, IsDead);

    return s_pIsDead(this);
}

bool Actor::IsDragon() const noexcept
{
    const ActorExtension* pExtension = const_cast<Actor*>(this)->GetExtension();
    return BehaviorVar::IsDragon(pExtension->GraphDescriptorHash);
}

// Actor::UpdateEssentialFlags (37227): flags2 bits 18 and 19, essential and protected, from the base (a leveled actor's
// original base) and the actor's aliases. The game only runs it in the actor's update and at the start of KillImpl.
static void UpdateLifeFlags(Actor* apActor) noexcept
{
    TP_THIS_FUNCTION(TUpdateEssentialFlags, void, Actor);
    POINTER_SKYRIMSE(TUpdateEssentialFlags, s_updateEssentialFlags, 37227);
    TiltedPhoques::ThisCall(s_updateEssentialFlags, apActor);
}

bool Actor::CanBeKilledBy(Actor* apAttacker) noexcept
{
    // An alias that came or went since the last update, as a brawl's essential opponent, counts already
    UpdateLifeFlags(this);

    TP_THIS_FUNCTION(TCanBeKilledBy, bool, Actor, Actor*);
    POINTER_SKYRIMSE(TCanBeKilledBy, s_canBeKilledBy, 37229);
    return TiltedPhoques::ThisCall(s_canBeKilledBy, this, apAttacker);
}

// Set while the sync brings an actor back, HookResurrect tells those apart from a revival on this client
static thread_local bool s_syncedResurrect = false;

// ACBS flags of TESActorBaseData::actorBaseFlags
constexpr uint32_t cBaseEssential = 1u << 1;
constexpr uint32_t cBaseProtected = 1u << 11;

/**
 * KillImpl starts by working out essential and protected again (Actor::UpdateEssentialFlags, 37227: flags2 bits 18 and 19
 * from the base, a leveled actor's original base, and the actor's aliases), before CanBeKilledBy decides between death and
 * a knock down. The base gets the flags for the one call. An alias of this client still has its say.
 */
template <class T> static void WithBaseLifeFlags(Actor* apActor, const uint32_t aSet, const uint32_t aClear, const T& acCall) noexcept
{
    const auto* pLeveled = static_cast<ExtraLeveledCreature*>(apActor->extraData.GetByType(ExtraDataType::LeveledCreature));
    TESActorBase* pBase = pLeveled && pLeveled->originalBase ? pLeveled->originalBase : Cast<TESNPC>(apActor->baseForm);
    if (!pBase)
    {
        acCall();
        return;
    }

    const uint32_t cSaved = pBase->actorData.actorBaseFlags;
    pBase->actorData.actorBaseFlags = (cSaved | aSet) & ~aClear;

    acCall();

    pBase->actorData.actorBaseFlags = cSaved;
    UpdateLifeFlags(apActor);
}

void Actor::Kill(bool aForceDeathAction) noexcept
{
    // Never kill players
    ActorExtension* pExtension = GetExtension();
    if (pExtension->IsPlayer())
        return;

    // A death another client had: knocked down here instead for being essential or protected in this game, the actor stayed
    // down for good. The killer comes with that death (CharacterService::KillWhenReady), OnDeath scripts check it.
    Actor* pKiller = aForceDeathAction ? Cast<Actor>(TESObjectREFR::GetByHandle(killerHandle)) : nullptr;

    // The body drops through a death action, which remote actors have refused. That's fine when the owner's action
    // replays too, a copy of an actor that died before this client got there would stay on its feet instead.
    g_forceAnimation = aForceDeathAction;

    WithBaseLifeFlags(this, 0, aForceDeathAction ? cBaseEssential | cBaseProtected : 0, [this, pKiller]() { KillImpl(pKiller, 100.f, true, true); });

    g_forceAnimation = false;
}

void Actor::KillByPlayer(Actor* apPlayer) noexcept
{
    if (GetExtension()->IsPlayer())
        return;

    // CanBeKilledBy only lets this client's player through protected, the other client's player is an NPC here
    if (apPlayer)
        killerHandle = apPlayer->GetHandle().handle.iBits;

    WithBaseLifeFlags(this, 0, cBaseProtected, [this, apPlayer]() { KillImpl(apPlayer, 100.f, true, true); });
}

static void SetLifeState(Actor* apActor, CombatRules::LifeState aState) noexcept
{
    TP_THIS_FUNCTION(TSetLifeState, void, Actor, uint32_t);
    POINTER_SKYRIMSE(TSetLifeState, s_setLifeState, 37612);
    TiltedPhoques::ThisCall(s_setLifeState, apActor, static_cast<uint32_t>(aState));
}

void Actor::KnockDown() noexcept
{
    if (GetExtension()->IsPlayer() || IsDead())
        return;

    // Replayed actions write the owner's life state into a copy without the game ever putting it down, so its scripts
    // never heard of it (OnEnterBleedout). Up again first, it goes down through the game.
    if (actorState.IsBleedingOut())
        actorState.flags1 &= ~ActorState::kLifeStateMask;

    // A copy refuses its own bleedout action otherwise. The owner's goes out to the copies as usual.
    g_forceAnimation = GetExtension()->IsRemote();

    // An essential actor at zero health: KillImpl sees CanBeKilledBy refuse and knocks it down (life state 7)
    WithBaseLifeFlags(this, cBaseEssential, 0, [this]() { KillImpl(nullptr, 0.f, true, false); });

    g_forceAnimation = false;
}

void Actor::StandUp(float aHealth) noexcept
{
    if (!actorState.IsBleedingOut())
        return;

    g_forceAnimation = GetExtension()->IsRemote();

    // A rise in health is what gets the game's downed actors up (health callbacks 38492 and 38633), so it starts below
    const float cHealth = GetActorValue(ActorValueInfo::kHealth);
    ForceActorValue(ActorValueOwner::ForceMode::DAMAGE, ActorValueInfo::kHealth, std::min(cHealth, 0.f));
    ForceActorValue(ActorValueOwner::ForceMode::DAMAGE, ActorValueInfo::kHealth, std::isfinite(aHealth) ? std::max(aHealth, 1.f) : std::max(cHealth, 1.f));

    // A bleedout with no recovery of its own (SetNoBleedoutRecovery) or a higher threshold still follows its owner
    if (actorState.IsBleedingOut())
        SetLifeState(this, CombatRules::kAlive);

    g_forceAnimation = false;
}

void Actor::Revive() noexcept
{
    const bool cIsRemote = GetExtension()->IsRemote();

    s_syncedResurrect = true;
    // A copy refuses its own actions otherwise, getting off the ground among them
    g_forceAnimation = cIsRemote;
    Resurrect(false);
    g_forceAnimation = false;
    s_syncedResurrect = false;

    // A copy's AI runs on its owner's client
    if (!cIsRemote)
        EvaluatePackage();
}

void Actor::Reset() noexcept
{
    using ObjectReference = TESObjectREFR;

    PAPYRUS_FUNCTION(void, ObjectReference, Reset, int, TESObjectREFR*);

    s_pReset(this, 0, nullptr);
}

bool Actor::PlayIdle(TESIdleForm* apIdle) noexcept
{
    PAPYRUS_FUNCTION(bool, Actor, PlayIdle, TESIdleForm*);
    return s_pPlayIdle(this, apIdle);
}

void Actor::Respawn() noexcept
{
    // Every caller brings back a copy its owner has alive. Reset sent it to its editor location and reloaded its 3D, which
    // dropped it out of the high process: discovery released it and it ran its own AI. A copy comes back where it lies.
    if (GetExtension()->IsRemote())
    {
        Revive();
        return;
    }

    s_syncedResurrect = true;
    Resurrect(false);
    s_syncedResurrect = false;

    Reset();
}

bool Actor::IsVampireLord() const noexcept
{
    return race && race->formID == 0x200283A;
}

extern thread_local bool g_forceAnimation;

void Actor::FixVampireLordModel() noexcept
{
    TESBoundObject* pLordArmor = Cast<TESBoundObject>(TESForm::GetById(0x2011a84));
    if (!pLordArmor)
        return;

    {
        ScopedInventoryOverride _;
        AddObjectToContainer(pLordArmor, nullptr, 1, nullptr);
    }

    EquipManager::Get()->Equip(this, pLordArmor, nullptr, 1, nullptr, false, true, false, false);

    g_forceAnimation = true;

    BSFixedString str("isLevitating");
    uint32_t isLevitating = GetAnimationVariableInt(&str);
    spdlog::critical("isLevitating {}", isLevitating);

    // By default, a loaded vampire lord is not levitating.
    if (isLevitating)
    {
        BSFixedString levitation("LevitationToggle");
        SendAnimationEvent(&levitation);
    }

    // TODO: weapon draw code does not seem to take care of this
    //BSFixedString weapEquip("WeapEquip");
    //SendAnimationEvent(&weapEquip);

    g_forceAnimation = false;
}

char TP_MAKE_THISCALL(HookSetPosition, Actor, NiPoint3& aPosition)
{
    const auto pExtension = apThis ? apThis->GetExtension() : nullptr;
    const auto bIsRemote = pExtension && pExtension->IsRemote();

    if (bIsRemote && !ScopedReferencesOverride::IsOverriden())
        return 1;

    // Don't interfere with non actor references, or the player, or if we are calling our self
    if (apThis->formType != Actor::Type || apThis == PlayerCharacter::Get() || ScopedReferencesOverride::IsOverriden())
        return TiltedPhoques::ThisCall(RealSetPosition, apThis, aPosition);

    ScopedReferencesOverride recursionGuard;

    // It just works TM
    apThis->SetPosition(aPosition, false);

    return 1;
}

TP_THIS_FUNCTION(TForceState, void, Actor, const NiPoint3&, float, float, TESObjectCELL*, TESWorldSpace*, bool);
static TForceState* RealForceState = nullptr;

void TP_MAKE_THISCALL(HookForceState, Actor, const NiPoint3& acPosition, float aX, float aZ, TESObjectCELL* apCell, TESWorldSpace* apWorldSpace, bool aUnkBool)
{
    /*const auto pNpc = Cast<TESNPC>(apThis->baseForm);
    if (pNpc)
    {
        spdlog::info("For TESNPC: {}, spawn at {} {} {}", pNpc->fullName.value, apPosition->m_x, apPosition->m_y,
                     apPosition->m_z);
    }*/

    // if (apThis != PlayerCharacter::Get())
    //     return;

    return TiltedPhoques::ThisCall(RealForceState, apThis, acPosition, aX, aZ, apCell, apWorldSpace, aUnkBool);
}

TP_THIS_FUNCTION(TSpawnActorInWorld, bool, Actor);
static TSpawnActorInWorld* RealSpawnActorInWorld = nullptr;

// TODO: this isn't SpawnActorInWorld, this is TESObjectREFR::UpdateReference3D()
bool TP_MAKE_THISCALL(HookSpawnActorInWorld, Actor)
{
    const auto* pNpc = Cast<TESNPC>(apThis->baseForm);
    if (pNpc)
    {
        spdlog::info("Spawn Actor: {:X}, and NPC {}", apThis->formID, pNpc->fullName.value);
    }

    return TiltedPhoques::ThisCall(RealSpawnActorInWorld, apThis);
}

TP_THIS_FUNCTION(TDamageActor, bool, Actor, float aDamage, Actor* apHitter, bool aKillMove);
static TDamageActor* RealDamageActor = nullptr;

// TODO: this is flawed, since it does not account for invulnerable actors
bool TP_MAKE_THISCALL(HookDamageActor, Actor, float aDamage, Actor* apHitter, bool aKillMove)
{
    if (RespawnProtection::IsActive())
    {
        if (apThis->GetExtension()->IsLocalPlayer())
            return false;

        // Spawn protection is not for fighting with. The hit is lost, and the player is fair game from now on.
        if (apHitter && apHitter->GetExtension()->IsLocalPlayer())
        {
            RespawnProtection::Break();
            return false;
        }
    }

    if (apHitter)
        World::Get().GetRunner().Trigger(HitEvent(apHitter->formID, apThis->formID));

    float realDamage = GameplayFormulas::CalculateRealDamage(apThis, aDamage, aKillMove);

    float currentHealth = apThis->GetActorValue(ActorValueInfo::kHealth);
    bool wouldKill = (currentHealth - realDamage) <= 0.f;

    const auto* pExHittee = apThis->GetExtension();
    if (pExHittee->IsLocalPlayer())
    {
        if (!World::Get().GetServerSettings().PvpEnabled)
        {
            if (apHitter && apHitter->GetExtension()->IsRemotePlayer())
                return false;
        }

        World::Get().GetRunner().Trigger(HealthChangeEvent(apThis->formID, -realDamage));
        return TiltedPhoques::ThisCall(RealDamageActor, apThis, aDamage, apHitter, aKillMove);
    }
    else if (pExHittee->IsRemotePlayer())
    {
        return wouldKill;
    }

    if (apHitter)
    {
        const auto* pExHitter = apHitter->GetExtension();
        if (pExHitter->IsLocalPlayer())
        {
            World::Get().GetRunner().Trigger(HealthChangeEvent(apThis->formID, -realDamage));

            // The killing blow on another client's actor goes to its owner, whose copy dies and tells everyone. Dying here
            // first, on health that doesn't regenerate, left a corpse here while the owner's copy fought on.
            if (pExHittee->IsRemote() && wouldKill && realDamage > 0.f)
            {
                const float cAllowed = std::max(currentHealth - 1.f, 0.f);
                TiltedPhoques::ThisCall(RealDamageActor, apThis, aDamage * cAllowed / realDamage, apHitter, aKillMove);
                return false;
            }

            return TiltedPhoques::ThisCall(RealDamageActor, apThis, aDamage, apHitter, aKillMove);
        }
        if (pExHitter->IsRemotePlayer())
        {
            return wouldKill;
        }
    }

    if (pExHittee->IsLocal())
    {
        World::Get().GetRunner().Trigger(HealthChangeEvent(apThis->formID, -realDamage));
        return TiltedPhoques::ThisCall(RealDamageActor, apThis, aDamage, apHitter, aKillMove);
    }
    else
    {
        return wouldKill;
    }
}

// Actor vfunc 0x104, which every health loss goes through (38468, from hits, magic and ForceActorValue alike). At zero health
// it queues the death that KillImpl turns into a knock down for an essential actor.
TP_THIS_FUNCTION(THandleHealthDamage, void, Actor, Actor* apAttacker, float aDamage);
static THandleHealthDamage* RealHandleHealthDamage = nullptr;

void TP_MAKE_THISCALL(HookHandleHealthDamage, Actor, Actor* apAttacker, float aDamage)
{
    // A copy's life follows its owner (CombatRules). On its own health, which misses what the owner's game restores, and with
    // this client's essential flags it died where its owner's actor only went down, and never got up.
    const auto* pExtension = apThis->GetExtension();
    if (!CombatRules::ReactsToHealth(pExtension->IsRemote(), pExtension->IsPlayer()))
        return;

    TiltedPhoques::ThisCall(RealHandleHealthDamage, apThis, apAttacker, aDamage);
}

TP_THIS_FUNCTION(TResurrect, void, Actor, bool aResetInventory);
static TResurrect* RealResurrect = nullptr;

void TP_MAKE_THISCALL(HookResurrect, Actor, bool aResetInventory)
{
    const bool cWasDead = apThis->IsDead();

    TiltedPhoques::ThisCall(RealResurrect, apThis, aResetInventory);

    // The console's resurrect or a script on this client brought back a copy its owner still has dead. Papyrus runs this on its
    // own thread, the request goes out from the main one.
    const auto* pExtension = apThis->GetExtension();
    if (cWasDead && !s_syncedResurrect && pExtension->IsRemote() && !pExtension->IsPlayer() && !apThis->IsDead())
        World::Get().GetRunner().Queue([cFormId = apThis->formID]() { World::Get().ctx().at<ActorValueService>().OnLocalRevival(cFormId); });
}

static TiltedPhoques::Initializer s_deathHooks(
    []()
    {
        POINTER_SKYRIMSE(THandleHealthDamage, s_handleHealthDamage, 38606);
        POINTER_SKYRIMSE(TResurrect, s_resurrect, 37321);

        RealHandleHealthDamage = s_handleHealthDamage.Get();
        RealResurrect = s_resurrect.Get();

        TP_HOOK(&RealHandleHealthDamage, HookHandleHealthDamage);
        TP_HOOK(&RealResurrect, HookResurrect);
    });

TP_THIS_FUNCTION(TApplyActorEffect, void, ActiveEffect, Actor* apTarget, float aEffectValue, unsigned int unk1);
static TApplyActorEffect* RealApplyActorEffect = nullptr;

void TP_MAKE_THISCALL(HookApplyActorEffect, ActiveEffect, Actor* apTarget, float aEffectValue, unsigned int unk1)
{
    const auto* pValueModEffect = Cast<ValueModifierEffect>(apThis);

    if (pValueModEffect)
    {
        if (pValueModEffect->actorValueIndex == ActorValueInfo::kHealth && aEffectValue > 0.0f)
        {
            if (apTarget && apTarget->GetExtension())
            {
                const auto pExTarget = apTarget->GetExtension();
                if (pExTarget->IsLocal())
                {
                    World::Get().GetRunner().Trigger(HealthChangeEvent(apTarget->formID, aEffectValue));
                    return TiltedPhoques::ThisCall(RealApplyActorEffect, apThis, apTarget, aEffectValue, unk1);
                }
                return;
            }
        }
    }

    return TiltedPhoques::ThisCall(RealApplyActorEffect, apThis, apTarget, aEffectValue, unk1);
}

void TP_MAKE_THISCALL(HookAddInventoryItem, Actor, TESBoundObject* apItem, ExtraDataList* apExtraData, int32_t aCount, TESObjectREFR* apOldOwner)
{
    if (!ScopedInventoryOverride::IsOverriden())
    {
        auto& modSystem = World::Get().GetModSystem();

        Inventory::Entry item{};
        modSystem.GetServerModId(apItem->formID, item.BaseId);
        item.Count = aCount;

        if (apExtraData)
            apThis->GetItemFromExtraData(item, apExtraData);

        QueueActorInventoryChange(apThis, InventoryChangeEvent(apThis->formID, std::move(item)), apOldOwner);
    }

    TiltedPhoques::ThisCall(RealAddInventoryItem, apThis, apItem, apExtraData, aCount, apOldOwner);
}

void* TP_MAKE_THISCALL(HookPickUpObject, Actor, TESObjectREFR* apObject, int32_t aCount, bool aUnk1, float aUnk2)
{
    // Players have to ask the server for an item other clients also show. NPCs can't, so they have to leave it be.
    if (World::Get().ctx().at<InventoryService>().IsSharedDrop(apObject))
        return nullptr;

    if (!ScopedInventoryOverride::IsOverriden())
    {
        auto& modSystem = World::Get().GetModSystem();

        Inventory::Entry item{};
        modSystem.GetServerModId(apObject->baseForm->formID, item.BaseId);
        item.Count = aCount;

        if (apObject->GetExtraDataList())
            apThis->GetItemFromExtraData(item, apObject->GetExtraDataList());

        // This is here so that objects that are picked up on both clients, aka non temps, are synced through activation sync.
        // The inventory change event should always be sent to the server, otherwise the server inventory won't be updated.
        bool shouldUpdateClients = apObject->IsTemporary() && !ScopedActivateOverride::IsOverriden();

        QueueActorInventoryChange(apThis, InventoryChangeEvent(apThis->formID, std::move(item), false, shouldUpdateClients));
    }

    return TiltedPhoques::ThisCall(RealPickUpObject, apThis, apObject, aCount, aUnk1, aUnk2);
}

void Actor::PickUpObject(TESObjectREFR* apObject, int32_t aCount, bool aUnk1, float aUnk2) noexcept
{
    TiltedPhoques::ThisCall(RealPickUpObject, this, apObject, aCount, aUnk1, aUnk2);
}

void* TP_MAKE_THISCALL(HookDropObject, Actor, void* apResult, TESBoundObject* apObject, ExtraDataList* apExtraData, int32_t aCount, NiPoint3* apLocation, NiPoint3* apRotation)
{
    auto& modSystem = World::Get().GetModSystem();

    Inventory::Entry item{};
    modSystem.GetServerModId(apObject->formID, item.BaseId);
    item.Count = -aCount;

    if (apExtraData)
        apThis->GetItemFromExtraData(item, apExtraData);

    // What lies on the ground, as opposed to what left the inventory
    Inventory::Entry droppedItem = item;
    droppedItem.Count = aCount;
    droppedItem.ExtraWorn = false;
    droppedItem.ExtraWornLeft = false;

    QueueActorInventoryChange(apThis, InventoryChangeEvent(apThis->formID, std::move(item), true));

    // Without a location the engine casts along the view from wherever the camera object says it is. Sometimes that is
    // near the world origin, and the item is created out there. Fixing that afterwards means moving a live physics object.
    NiPoint3 safeLocation{};
    if (auto* pCamera = PlayerCamera::Get(); !apLocation && pCamera)
    {
        const NiPoint3 cCameraPosition = pCamera->GetPosition();
        if (DropRules::IsDropOriginStale(cCameraPosition, apThis->position))
        {
            spdlog::warn("Camera is at ({}, {}, {}) for a drop by the player at ({}, {}, {}), not using it", cCameraPosition.x, cCameraPosition.y, cCameraPosition.z, apThis->position.x, apThis->position.y, apThis->position.z);
            safeLocation = SyncRules::GetDropPosition(apThis->position, apThis->rotation.z);
            apLocation = &safeLocation;
        }
    }

    ScopedInventoryOverride _;

    void* pResult = TiltedPhoques::ThisCall(RealDropObject, apThis, apResult, apObject, apExtraData, aCount, apLocation, apRotation);
    QueueDropCheck(apThis, *static_cast<BSPointerHandle<TESObjectREFR>*>(apResult), apLocation, std::move(droppedItem));
    return pResult;
}

void Actor::DropOrPickUpObject(const Inventory::Entry& arEntry, NiPoint3* apLocation, NiPoint3* apRotation) noexcept
{
    ExtraDataList* pExtraData = GetExtraDataFromItem(arEntry);

    ModSystem& modSystem = World::Get().GetModSystem();

    uint32_t objectId = modSystem.GetGameId(arEntry.BaseId);
    TESBoundObject* pObject = Cast<TESBoundObject>(TESForm::GetById(objectId));
    if (!pObject)
    {
        spdlog::warn("Object to drop not found, {:X}:{:X}.", arEntry.BaseId.ModId, arEntry.BaseId.BaseId);
        return;
    }

    if (arEntry.Count < 0)
        DropObject(pObject, pExtraData, -arEntry.Count, apLocation, apRotation);
    // TODO: pick up
}

void Actor::DropObject(TESBoundObject* apObject, ExtraDataList* apExtraData, int32_t aCount, NiPoint3* apLocation, NiPoint3* apRotation) noexcept
{
    spdlog::debug("Dropping object, form id: {:X}, count: {}, actor: {:X}", apObject->formID, aCount, formID);
    BSPointerHandle<TESObjectREFR> result{};

    if (this == PlayerCharacter::Get())
    {
        TiltedPhoques::ThisCall(RealDropObject, this, &result, apObject, apExtraData, aCount, apLocation, apRotation);
    }
    else
    {
        // RealDropObject is PlayerCharacter's override, which reads and writes past the end of any other actor
        POINTER_SKYRIMSE(TDropObject, s_actorDropObject, 37520);
        TiltedPhoques::ThisCall(s_actorDropObject.Get(), this, &result, apObject, apExtraData, aCount, apLocation, apRotation);
    }

    QueueDropCheck(this, result, apLocation);
}

void Actor::EvaluatePackage() noexcept
{
    PAPYRUS_FUNCTION(void, Actor, EvaluatePackage);
    s_pEvaluatePackage(this);
}

void Actor::ReleaseFromFurniture(bool aEvaluateAi) noexcept
{
    PAPYRUS_FUNCTION(void, Actor, SetRestrained, bool);

    spdlog::info("Releasing actor {:X} from furniture, state: {:X}", formID, actorState.flags1);

    if (actorState.IsRestrained())
        s_pSetRestrained(this, false);

    // Furniture the actor really occupies is let go of the way MoveTo, Disable and ForceState do it: the engine's quick
    // exit (38697 with true) sets the sit state back, plays the furniture's exit action and clears the handle. Clearing the
    // state alone left the furniture held, and the AI kept treating the actor as seated so it never got up.
    if (GetOccupiedFurniture())
    {
        TP_THIS_FUNCTION(TStopInteractingQuick, void, Actor, bool aQuick);
        POINTER_SKYRIMSE(TStopInteractingQuick, s_stopInteractingQuick, 38697);

        TiltedPhoques::ThisCall(s_stopInteractingQuick, this, true);
    }

    actorState.ClearFurnitureState();

    // Creature behaviors don't have the humanoid event, draugr answer to the second and spiders to the rest. Without
    // them the pose stays while the actor moves on, like a draugr floating out of its alcove.
    for (const char* pEvent : {"IdleForceDefaultState", "forceFurnExit", "idleBedGetUp", "idleChairGetUp"})
    {
        BSFixedString event(pEvent);
        SendAnimationEvent(&event);
    }

    if (aEvaluateAi)
        EvaluatePackage();
}

TP_THIS_FUNCTION(TUpdateDetectionState, void, ActorKnowledge, void*);
static TUpdateDetectionState* RealUpdateDetectionState = nullptr;

void TP_MAKE_THISCALL(HookUpdateDetectionState, ActorKnowledge, void* apState)
{
    auto pOwner = TESObjectREFR::GetByHandle(apThis->hOwner);
    auto pTarget = TESObjectREFR::GetByHandle(apThis->hTarget);

    if (pOwner && pTarget)
    {
        auto pOwnerActor = Cast<Actor>(pOwner);
        auto pTargetActor = Cast<Actor>(pTarget);
        if (pOwnerActor && pTargetActor)
        {
            if (pOwnerActor->GetExtension()->IsRemotePlayer() && pTargetActor->GetExtension()->IsLocalPlayer())
            {
                spdlog::debug("Cancelling detection from remote player to local player, owner: {:X}, target: {:X}", pOwner->formID, pTarget->formID);
                return;
            }
        }
    }

    return TiltedPhoques::ThisCall(RealUpdateDetectionState, apThis, apState);
}

struct DialogueItem;

// TODO: This is an AIProcess function
TP_THIS_FUNCTION(TProcessResponse, uint64_t, void, DialogueItem* apVoice, Actor* apTalkingActor, Actor* apTalkedToActor);
static TProcessResponse* RealProcessResponse = nullptr;

uint64_t TP_MAKE_THISCALL(HookProcessResponse, void, DialogueItem* apVoice, Actor* apTalkingActor, Actor* apTalkedToActor)
{
    if (apTalkingActor)
    {
        if (apTalkingActor->GetExtension()->IsRemotePlayer())
            return 0;

        // The AI of a remote copy picks barks of its own. They talk over the lines the owner relays, each one cutting the
        // other off and taking the mouth with it. Scene lines stay as every client plays its scenes itself.
        if (apTalkingActor->GetExtension()->IsRemote() && !MenuTopicManager::IsPlayerDialogueSpeaker(apTalkingActor))
        {
            const BGSScene* pScene = apTalkingActor->GetCurrentScene();
            if (!pScene || !pScene->isPlaying)
                return 0;
        }
    }
    return TiltedPhoques::ThisCall(RealProcessResponse, apThis, apVoice, apTalkingActor, apTalkedToActor);
}

bool TP_MAKE_THISCALL(HookInitiateMountPackage, Actor, Actor* apMount)
{
    if (apMount && apThis->GetExtension()->IsLocal())
        World::Get().GetRunner().Trigger(MountEvent(apThis->formID, apMount->formID));

    return TiltedPhoques::ThisCall(RealInitiateMountPackage, apThis, apMount);
}

TP_THIS_FUNCTION(TUnequipObject, void, Actor, void* apUnk1, TESBoundObject* apObject, int32_t aUnk2, void* apUnk3);
static TUnequipObject* RealUnequipObject = nullptr;

void TP_MAKE_THISCALL(HookUnequipObject, Actor, void* apUnk1, TESBoundObject* apObject, int32_t aUnk2, void* apUnk3)
{
    TiltedPhoques::ThisCall(RealUnequipObject, apThis, apUnk1, apObject, aUnk2, apUnk3);
}

// Returns a float in xmm0, 0 when nothing was played. Callers check it for -1.0 (lip file failed to load).
TP_THIS_FUNCTION(TSpeakSoundFunction, float, Actor, const char* apName, uint32_t* a3, uint32_t a4, uint32_t a5, uint32_t a6, uint64_t a7, uint64_t a8, uint64_t a9, bool a10, uint64_t a11, bool a12, bool a13, bool a14);
static TSpeakSoundFunction* RealSpeakSoundFunction = nullptr;

float TP_MAKE_THISCALL(HookSpeakSoundFunction, Actor, const char* apName, uint32_t* a3, uint32_t a4, uint32_t a5, uint32_t a6, uint64_t a7, uint64_t a8, uint64_t a9, bool a10, uint64_t a11, bool a12, bool a13, bool a14)
{
    spdlog::debug("a3: {:X}, a4: {}, a5: {}, a6: {}, a7: {}, a8: {:X}, a9: {:X}, a10: {}, a11: {:X}, a12: {}, a13: {}, a14: {}", (uint64_t)a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14);

    // The player having the conversation may not own this NPC. Ambient
    // speech still comes only from the actor's simulation owner.
    const bool cIsPlayerDialogue = MenuTopicManager::IsPlayerDialogueSpeaker(apThis);
    const bool cShouldSync = apThis->GetExtension()->IsLocal() || cIsPlayerDialogue;

    // Lines from a scene let other clients that play the same scene skip them, unless their copy is stuck in an earlier
    // phase. Menu dialogue always plays.
    BGSScene* pScene = cIsPlayerDialogue ? nullptr : apThis->GetCurrentScene();
    uint32_t sceneId = 0;
    uint32_t scenePhase = 0;
    if (pScene && pScene->isPlaying)
    {
        sceneId = pScene->formID;
        scenePhase = pScene->currentPhaseIndex;
    }

    if (cShouldSync)
        World::Get().GetRunner().Trigger(DialogueEvent(apThis->formID, apName, sceneId, scenePhase));

    return TiltedPhoques::ThisCall(RealSpeakSoundFunction, apThis, apName, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14);
}

void Actor::SpeakSound(const char* pFile)
{
    uint32_t handle[3]{};
    handle[0] = -1;
    TiltedPhoques::ThisCall(RealSpeakSoundFunction, this, pFile, handle, 0, 0x32, 0, 0, 0, 0, 0, 0, 0, 1, 1);
}

char TP_MAKE_THISCALL(HookActorProcess, Actor, float aDeltaTime)
{
    // Suppress local movement for remote actors, but preserve controller timing.
    if (apThis->GetExtension()->IsRemote())
    {
        if (apThis->currentProcess)
        {
            if (auto* pController = apThis->currentProcess->GetCharController())
                pController->UpdateStepTiming(aDeltaTime);
        }
        return 0;
    }

    return TiltedPhoques::ThisCall(RealActorProcess, apThis, aDeltaTime);
}

TP_THIS_FUNCTION(TAddDeathItems, void, Actor);
static TAddDeathItems* RealAddDeathItems = nullptr;

void TP_MAKE_THISCALL(HookAddDeathItems, Actor)
{
    if (apThis->GetExtension()->IsRemote())
        return;

    TiltedPhoques::ThisCall(RealAddDeathItems, apThis);
}

TP_THIS_FUNCTION(TIsFleeing, bool, Actor);
static TIsFleeing* RealIsFleeing = nullptr;

bool TP_MAKE_THISCALL(HookIsFleeing, Actor)
{
    // TODO: Player or RemotePlayer? Can players be in fleeing mode in skyrim?
    // TODO: investigate why the flee flag is set at all on remote players sometimes.
    if (apThis->GetExtension()->IsPlayer())
        return false;
    
    return TiltedPhoques::ThisCall(RealIsFleeing, apThis);
}

static TiltedPhoques::Initializer s_actorHooks(
    []()
    {
        POINTER_SKYRIMSE(TActorProcess, s_actorProcess, 37356);
        POINTER_SKYRIMSE(TSetPosition, s_setPosition, 19790);
        POINTER_SKYRIMSE(TRemoveSpell, s_removeSpell, 38717);
        POINTER_SKYRIMSE(TCharacterConstructor, s_characterCtor, 40245);
        POINTER_SKYRIMSE(TCharacterConstructor2, s_characterCtor2, 40246);
        POINTER_SKYRIMSE(TCharacterDestructor, s_characterDtor, 37175);
        POINTER_SKYRIMSE(TGetLocation, s_GetActorLocation, 19812);
        POINTER_SKYRIMSE(TForceState, s_ForceState, 37313);
        POINTER_SKYRIMSE(TSpawnActorInWorld, s_SpawnActorInWorld, 19742);
        POINTER_SKYRIMSE(TDamageActor, s_damageActor, 37335);
        POINTER_SKYRIMSE(TApplyActorEffect, s_applyActorEffect, 35086);
        POINTER_SKYRIMSE(TAddInventoryItem, s_addInventoryItem, 37525);
        POINTER_SKYRIMSE(TPickUpObject, s_pickUpObject, 37521);
        POINTER_SKYRIMSE(TDropObject, s_dropObject, 40454);
        POINTER_SKYRIMSE(TUpdateDetectionState, s_updateDetectionState, 42704);
        POINTER_SKYRIMSE(TProcessResponse, s_processResponse, 39643);
        POINTER_SKYRIMSE(TInitiateMountPackage, s_initiateMountPackage, 37905);
        POINTER_SKYRIMSE(TUnequipObject, s_unequipObject, 37975);
        POINTER_SKYRIMSE(TSpeakSoundFunction, s_speakSoundFunction, 37542);
        POINTER_SKYRIMSE(TAddDeathItems, addDeathItems, 37198);
        POINTER_SKYRIMSE(TIsFleeing, isFleeing, 37577);

        RealActorProcess = s_actorProcess.Get();
        RealSetPosition = s_setPosition.Get();
        RealRemoveSpell = s_removeSpell.Get();
        FUNC_GetActorLocation = s_GetActorLocation.Get();
        RealCharacterConstructor = s_characterCtor.Get();
        RealCharacterConstructor2 = s_characterCtor2.Get();
        RealForceState = s_ForceState.Get();
        RealSpawnActorInWorld = s_SpawnActorInWorld.Get();
        RealDamageActor = s_damageActor.Get();
        RealApplyActorEffect = s_applyActorEffect.Get();
        RealAddInventoryItem = s_addInventoryItem.Get();
        RealPickUpObject = s_pickUpObject.Get();
        RealDropObject = s_dropObject.Get();
        RealUpdateDetectionState = s_updateDetectionState.Get();
        RealProcessResponse = s_processResponse.Get();
        RealInitiateMountPackage = s_initiateMountPackage.Get();
        RealUnequipObject = s_unequipObject.Get();
        RealSpeakSoundFunction = s_speakSoundFunction.Get();
        RealAddDeathItems = addDeathItems.Get();
        RealIsFleeing = isFleeing.Get();

        TP_HOOK(&RealActorProcess, HookActorProcess);
        TP_HOOK(&RealSetPosition, HookSetPosition);
        TP_HOOK(&RealRemoveSpell, HookRemoveSpell);
        TP_HOOK(&RealCharacterConstructor, HookCharacterConstructor);
        TP_HOOK(&RealCharacterConstructor2, HookCharacterConstructor2);
        TP_HOOK(&RealForceState, HookForceState);
        TP_HOOK(&RealSpawnActorInWorld, HookSpawnActorInWorld);
        TP_HOOK(&RealDamageActor, HookDamageActor);
        TP_HOOK(&RealApplyActorEffect, HookApplyActorEffect);
        TP_HOOK(&RealAddInventoryItem, HookAddInventoryItem);
        TP_HOOK(&RealPickUpObject, HookPickUpObject);
        TP_HOOK(&RealDropObject, HookDropObject);
        TP_HOOK(&RealUpdateDetectionState, HookUpdateDetectionState);
        TP_HOOK(&RealProcessResponse, HookProcessResponse);
        TP_HOOK(&RealInitiateMountPackage, HookInitiateMountPackage);
        TP_HOOK(&RealUnequipObject, HookUnequipObject);
        TP_HOOK(&RealSpeakSoundFunction, HookSpeakSoundFunction);
        TP_HOOK(&RealAddDeathItems, HookAddDeathItems);
        TP_HOOK(&RealIsFleeing, HookIsFleeing);
    });
