#include <PlayerCharacter.h>
#include <Games/ActorExtension.h>

#include <Structs/Skyrim/AnimationGraphDescriptor_Master_Behavior.h>
#include <Structs/Skyrim/AnimationGraphDescriptor_VampireLordBehavior.h>

#include <Games/Overrides.h>

#include <Events/InventoryChangeEvent.h>
#include <Events/BeastFormChangeEvent.h>
#include <Events/SetWaypointEvent.h>
#include <Events/RemoveWaypointEvent.h>

#include <World.h>
#include <Services/InventoryService.h>

#include <Games/Skyrim/Forms/ActorValueInfo.h>
#include <Games/ActorExtension.h>
#include <Games/TES.h>
#include <Games/References.h>
#include <Forms/TESWorldSpace.h>

#include <Forms/TESObjectCELL.h>
#include <Forms/TESQuest.h>
#include <Forms/TESGlobal.h>
#include <Forms/MagicItem.h>
#include <Games/Skyrim/CellRespawnOverrides.h>

#include <ModCompat/BehaviorVar.h>

#include <Systems/PartyExperience.h>

#include <atomic>

int32_t PlayerCharacter::LastUsedCombatSkill = -1;

// The skill hook and the death event aren't guaranteed to run on the same thread.
static std::atomic<float> s_killExperience = 0.f;
static std::atomic<double> s_lastCombatSkillUse = -1.0;

static double GetSkillClock() noexcept
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

TP_THIS_FUNCTION(TPickUpObject, char, PlayerCharacter, TESObjectREFR* apObject, int32_t aCount, bool aUnk1, bool aUnk2);
TP_THIS_FUNCTION(TSetBeastForm, void, void, void* apUnk1, void* apUnk2, bool aEntering);
// The last two are for perk conditions (the advance object and action), the function hands them on to the skills object
TP_THIS_FUNCTION(TAddSkillExperience, void, PlayerCharacter, int32_t aSkill, float aExperience, void* apAdvanceObject, uint32_t aAdvanceAction);
TP_THIS_FUNCTION(TCalculateExperience, bool, int32_t, float* aFactor, float* aBonus, float* aUnk1, float* aUnk2);
TP_THIS_FUNCTION(TSetWaypoint, void, PlayerCharacter, NiPoint3* apPosition, TESWorldSpace* apWorldSpace);
TP_THIS_FUNCTION(TRemoveWaypoint, void, PlayerCharacter);

static TPickUpObject* RealPickUpObject = nullptr;
static TSetBeastForm* RealSetBeastForm = nullptr;
static TAddSkillExperience* RealAddSkillExperience = nullptr;
static TCalculateExperience* RealCalculateExperience = nullptr;
static TSetWaypoint* RealSetWaypoint = nullptr;
static TRemoveWaypoint* RealRemoveWaypoint = nullptr;

PlayerCharacter* PlayerCharacter::Get() noexcept
{
    POINTER_SKYRIMSE(PlayerCharacter*, s_character, 401069);

    return *s_character.Get();
}

const GameArray<TintMask*>& PlayerCharacter::GetTints() const noexcept
{
    if (overlayTints)
        return *overlayTints;

    return baseTints;
}

void PlayerCharacter::SetGodMode(bool aSet) noexcept
{
    POINTER_SKYRIMSE(bool, bGodMode, 404238);
    *bGodMode.Get() = aSet;
}

void PlayerCharacter::SetDifficulty(const int32_t aDifficulty, bool aForceUpdate, bool aExpectGameDataLoaded) noexcept
{
    if (aDifficulty > 5 || aDifficulty < 0)
        return;

    int32_t* difficultySetting = Settings::GetDifficulty();
    *difficultySetting = difficulty = aDifficulty;
}

float PlayerCharacter::TakeKillExperience() noexcept
{
    return s_killExperience.exchange(0.f);
}

void PlayerCharacter::AddSkillExperience(int32_t aSkill, float aExperience) noexcept
{
    Skills::Skill skill = Skills::GetSkillFromActorValue(aSkill);
    if (skill == Skills::kTotal)
        return;

    // A party member's kill is no reason for a skill to go up while this player isn't fighting.
    if (!PartyExperience::IsFighting(GetSkillClock(), s_lastCombatSkillUse))
    {
        spdlog::info("Party kill shared {} experience, ignored as the player isn't fighting", aExperience);
        return;
    }

    aExperience = PartyExperience::GetKillCredit(aExperience, (*pSkills)->skills[skill].levelThreshold);
    if (aExperience <= 0.f)
        return;

    float oldExperience = GetSkillExperience(skill);

    ScopedExperienceOverride _;

    TiltedPhoques::ThisCall(RealAddSkillExperience, this, aSkill, aExperience, static_cast<void*>(nullptr), 0u);

    float newExperience = GetSkillExperience(skill);
    float deltaExperience = newExperience - oldExperience;

    spdlog::info("Party kill added {} experience to skill {}, progress change: {}", aExperience, aSkill, deltaExperience);
}

void PlayerCharacter::WarmUp() noexcept
{
    const Mod* pSurvival = ModManager::Get()->GetByName("ccQDRSSE001-SurvivalMode.esl");
    if (!pSurvival || !pSurvival->IsLoaded())
        return;

    // Survival_ColdNeedValue, the NeedValue of Survival_NeedColdQuest. The quest reads it back on its next tick.
    auto* pCold = Cast<TESGlobal>(TESForm::GetById(pSurvival->GetFormId(0x81B)));
    if (!pCold || pCold->f <= 0.f)
        return;

    spdlog::info("Respawning at cold {}, warming the player up", pCold->f);
    pCold->f = 0.f;

    // Survival_GlobalFunctions.ColdHealthPenaltyAV(), the health the cold takes away
    SetActorValue(ActorValueInfo::kVariable04, 0.f);

    // needSpells of Survival_NeedColdQuest, cold stages 0 to 5. The quest adds the right one back on its next tick.
    for (const uint32_t cSpellId : {0x890u, 0x86Eu, 0x891u, 0x86Du, 0x870u, 0x871u})
    {
        if (auto* pSpell = Cast<MagicItem>(TESForm::GetById(pSurvival->GetFormId(cSpellId))))
            RemoveSpell(pSpell);
    }
}

NiPoint3 PlayerCharacter::RespawnPlayer() noexcept
{
    // Make bleedout state recoverable, so the player gets up from it on their own once healed
    SetNoBleedoutRecovery(false);

    // Survival Mode kills the player at maximum cold, and that same cold takes away all of their health. Respawning still
    // that cold kills them again within seconds, over and over. Death restores warmth the way a fire does.
    WarmUp();

    DispelAllSpells();

    // Reset health to max
    // TODO(cosideci): there's a cleaner way to do this
    ForceActorValue(ActorValueOwner::ForceMode::DAMAGE, ActorValueInfo::kHealth, 1000000);

    // Helgen closes one-way collision barriers behind the player. A cell's COC
    // point can be behind them; recover in place while the intro is active.
    auto* pIntro = Cast<TESQuest>(TESForm::GetById(0x3372B));
    if (pIntro && !pIntro->IsStopped() && pIntro->IsStageReached(160) && !pIntro->IsStageReached(1000))
        return position;

    TESObjectCELL* pCell = nullptr;

    if (GetWorldSpace())
    {
        // TP to Whiterun temple when killed in world space
        TES* pTes = TES::Get();
        pCell = ModManager::Get()->GetCellFromCoordinates(pTes->centerGridX, pTes->centerGridY, GetWorldSpace(), false);
    }
    else
    {
        // TP to start of cell when killed in an interior
        pCell = GetSaveParentCell();
    }

    NiPoint3 pos{};
    NiPoint3 rot{};

    if (!pCell)
        return pos;

    // If we find an override position for this respawn cell, we will use that instead of COC.
    if (!CellRespawnOverrides::GetRespawnPos(pCell, pos))
    {
        pCell->GetCOCPlacementInfo(&pos, &rot, true);
    }

    MoveTo(pCell, pos);

    return pos;
}

void PlayerCharacter::PayCrimeGoldToAllFactions() noexcept
{
    // Yes, yes, this isn't great, but there's no "pay fines everywhere" function
    const uint32_t crimeFactionIds[]{0x28170, 0x267E3, 0x29DB0, 0x2816D, 0x2816e, 0x2816C, 0x2816B, 0x267EA, 0x2816F, 0x4018279};

    for (uint32_t crimeFactionId : crimeFactionIds)
    {
        TESFaction* pCrimeFaction = Cast<TESFaction>(TESForm::GetById(crimeFactionId));
        if (!pCrimeFaction)
        {
            spdlog::error("This isn't a crime faction! {:X}", crimeFactionId);
            continue;
        }

        PayFine(pCrimeFaction, false, false);
    }
}

void PlayerCharacter::SetWaypoint(NiPoint3* apPosition, TESWorldSpace* apWorldSpace) noexcept
{
    return TiltedPhoques::ThisCall(RealSetWaypoint, this, apPosition, apWorldSpace);
}

void PlayerCharacter::RemoveWaypoint() noexcept
{
    return TiltedPhoques::ThisCall(RealRemoveWaypoint, this);
}

char TP_MAKE_THISCALL(HookPickUpObject, PlayerCharacter, TESObjectREFR* apObject, int32_t aCount, bool aUnk1, bool aUnk2)
{
    // Two players must not both end up with an item they can each see, so the server decides who gets it.
    if (World::Get().ctx().at<InventoryService>().DeferDropPickUp(apObject, aCount, aUnk1, aUnk2))
        return 0;

    auto& modSystem = World::Get().GetModSystem();

    Inventory::Entry item{};
    modSystem.GetServerModId(apObject->baseForm->formID, item.BaseId);
    item.Count = aCount;

    if (apObject->GetExtraDataList() && !ScopedExtraDataOverride::IsOverriden())
    {
        ScopedExtraDataOverride _;
        apThis->GetItemFromExtraData(item, apObject->GetExtraDataList());
    }

    // This is here so that objects that are picked up on both clients, aka non temps, are synced through activation sync.
    // The inventory change event should always be sent to the server, otherwise the server inventory won't be updated.
    bool shouldUpdateClients = apObject->IsTemporary() && !ScopedActivateOverride::IsOverriden();

    // The player still needs its server entity and ownership epoch so stale inventory events can be rejected.
    if (const auto ownershipToken = Utils::GetLocalOwnershipToken(apThis->formID))
    {
        InventoryChangeEvent event(apThis->formID, std::move(item), false, shouldUpdateClients);
        event.ServerId = ownershipToken->ServerId;
        event.OwnershipEpoch = ownershipToken->OwnershipEpoch;
        World::Get().GetRunner().Trigger(std::move(event));
    }

    ScopedInventoryOverride _;

    return TiltedPhoques::ThisCall(RealPickUpObject, apThis, apObject, aCount, aUnk1, aUnk2);
}

void PlayerCharacter::PickUpSharedDrop(TESObjectREFR* apObject, int32_t aCount, bool aUnk1, bool aUnk2) noexcept
{
    HookPickUpObject(this, apObject, aCount, aUnk1, aUnk2);
}

void TP_MAKE_THISCALL(HookSetBeastForm, void, void* apUnk1, void* apUnk2, bool aEntering)
{
    if (!aEntering)
    {
        PlayerCharacter::Get()->GetExtension()->GraphDescriptorHash = BehaviorVar::GetHumanoidHash();
        World::Get().GetRunner().Trigger(BeastFormChangeEvent());
    }

    TiltedPhoques::ThisCall(RealSetBeastForm, apThis, apUnk1, apUnk2, aEntering);
}

void TP_MAKE_THISCALL(HookAddSkillExperience, PlayerCharacter, int32_t aSkill, float aExperience, void* apAdvanceObject, uint32_t aAdvanceAction)
{
    // TODO: armor skills? sneak?
    static const Set<int32_t> combatSkills{ActorValueInfo::kAlteration, ActorValueInfo::kConjuration, ActorValueInfo::kDestruction, ActorValueInfo::kIllusion, ActorValueInfo::kRestoration, ActorValueInfo::kOneHanded, ActorValueInfo::kTwoHanded, ActorValueInfo::kMarksman, ActorValueInfo::kBlock};

    Skills::Skill skill = Skills::GetSkillFromActorValue(aSkill);
    float oldExperience = apThis->GetSkillExperience(skill);

    TiltedPhoques::ThisCall(RealAddSkillExperience, apThis, aSkill, aExperience, apAdvanceObject, aAdvanceAction);

    float newExperience = apThis->GetSkillExperience(skill);
    float deltaExperience = newExperience - oldExperience;

    spdlog::debug("Skill (AVI): {}, experience: {}", aSkill, deltaExperience);

    if (combatSkills.contains(aSkill))
    {
        spdlog::debug("Set new last used combat skill to {}.", aSkill);
        PlayerCharacter::LastUsedCombatSkill = aSkill;

        s_lastCombatSkillUse = GetSkillClock();
        s_killExperience = PartyExperience::AddSkillUse(s_killExperience, deltaExperience);
    }
}

bool TP_MAKE_THISCALL(HookCalculateExperience, int32_t, float* aFactor, float* aBonus, float* aUnk1, float* aUnk2)
{
    bool result = TiltedPhoques::ThisCall(RealCalculateExperience, apThis, aFactor, aBonus, aUnk1, aUnk2);

    if (ScopedExperienceOverride::IsOverriden())
    {
        *aFactor = 1.f;
        *aBonus = 0.f;
    }

    return result;
}

void TP_MAKE_THISCALL(HookSetWaypoint, PlayerCharacter, NiPoint3* apPosition, TESWorldSpace* apWorldSpace)
{
    Vector3_NetQuantize position{};
    position.x = apPosition->x;
    position.y = apPosition->y;
    position.z = apPosition->z;

    World::Get().GetRunner().Trigger(SetWaypointEvent(position, apWorldSpace ? apWorldSpace->formID : 0));

    return TiltedPhoques::ThisCall(RealSetWaypoint, apThis, apPosition, apWorldSpace);
}

void TP_MAKE_THISCALL(HookRemoveWaypoint, PlayerCharacter)
{
    World::Get().GetRunner().Trigger(RemoveWaypointEvent());

    return TiltedPhoques::ThisCall(RealRemoveWaypoint, apThis);
}

static TiltedPhoques::Initializer s_playerCharacterHooks(
    []()
    {
        POINTER_SKYRIMSE(TPickUpObject, s_pickUpObject, 40533);
        POINTER_SKYRIMSE(TSetBeastForm, s_setBeastForm, 55497);
        POINTER_SKYRIMSE(TAddSkillExperience, s_addSkillExperience, 40488);
        POINTER_SKYRIMSE(TCalculateExperience, s_calculateExperience, 27244);
        POINTER_SKYRIMSE(TSetWaypoint, s_setWaypoint, 40535);
        POINTER_SKYRIMSE(TRemoveWaypoint, s_removeWaypoint, 40536);

        RealPickUpObject = s_pickUpObject.Get();
        RealSetBeastForm = s_setBeastForm.Get();
        RealAddSkillExperience = s_addSkillExperience.Get();
        RealCalculateExperience = s_calculateExperience.Get();
        RealSetWaypoint = s_setWaypoint.Get();
        RealRemoveWaypoint = s_removeWaypoint.Get();

        TP_HOOK(&RealPickUpObject, HookPickUpObject);
        TP_HOOK(&RealSetBeastForm, HookSetBeastForm);
        TP_HOOK(&RealAddSkillExperience, HookAddSkillExperience);
        TP_HOOK(&RealCalculateExperience, HookCalculateExperience);
        TP_HOOK(&RealSetWaypoint, HookSetWaypoint);
        TP_HOOK(&RealRemoveWaypoint, HookRemoveWaypoint);
    });
