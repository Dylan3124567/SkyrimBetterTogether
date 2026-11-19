#include <Services/HelgenService.h>
#include <Forms/TESQuest.h>

#include <Services/PapyrusService.h>

#include <Games/Overrides.h>

TESObjectREFR* TESQuest::GetAliasedRef(uint32_t aAliasID) noexcept
{
    TP_THIS_FUNCTION(TGetAliasedRef, BSPointerHandle<TESObjectREFR>*, TESQuest, BSPointerHandle<TESObjectREFR>*, uint32_t);
    POINTER_SKYRIMSE(TGetAliasedRef, getAliasedRef, 25066);

    BSPointerHandle<TESObjectREFR> result{};
    TiltedPhoques::ThisCall(getAliasedRef, this, &result, aAliasID);

    return TESObjectREFR::GetByHandle(result.handle.iBits);
}

void TESQuest::ForceRefIntoAlias(uint32_t aAliasId, TESObjectREFR* apRef) noexcept
{
    TP_THIS_FUNCTION(TForceRefIntoAlias, void, TESQuest, uint32_t, TESObjectREFR*);
    POINTER_SKYRIMSE(TForceRefIntoAlias, forceRefIntoAlias, 25052);
    TiltedPhoques::ThisCall(forceRefIntoAlias, this, aAliasId, apRef);
}

TESQuest::State TESQuest::getState()
{
    if (flags >= 0)
    {
        if (unkFlags)
            return State::WaitingPromotion;
        else if (flags & 1)
            return State::Running;
        else
            return State::Stopped;
    }

    return State::WaitingForStage;
}

void TESQuest::SetCompleted(bool force)
{
    TP_THIS_FUNCTION(TSetCompleted, void, TESQuest, bool);
    POINTER_SKYRIMSE(TSetCompleted, SetCompleted, 24991);
    SetCompleted(this, force);
}

void TESQuest::CompleteAllObjectives()
{
    TP_THIS_FUNCTION(TCompleteAllObjectives, void, TESQuest);
    POINTER_SKYRIMSE(TCompleteAllObjectives, CompleteAll, 23231);
    CompleteAll(this);
}

void TESQuest::SetActive(bool toggle)
{
    if (toggle)
        flags |= 0x800;
    else
        flags &= 0xF7FF;
}

bool TESQuest::IsStageDone(uint16_t stageIndex)
{
    TP_THIS_FUNCTION(TIsStageDone, bool, TESQuest, uint16_t);
    POINTER_SKYRIMSE(TIsStageDone, IsStageDone, 25011);
    return IsStageDone(this, stageIndex);
}

bool TESQuest::Kill()
{
    using TSetStopped = void(TESQuest*, bool);
    POINTER_SKYRIMSE(TSetStopped, SetStopped, 24987);

    if (flags & Flags::Enabled)
    {
        unkFlags = 0;
        flags = Flags::Completed;
        MarkChanged(2);

        // SetStopped(this, false);
        return true;
    }

    return false;
}

bool TESQuest::EnsureQuestStarted(bool& success, bool force)
{
    TP_THIS_FUNCTION(TSetRunning, bool, TESQuest, bool*, bool);
    POINTER_SKYRIMSE(TSetRunning, SetRunning, 25003);
    return SetRunning(this, &success, force);
}

bool TESQuest::SetStage(uint16_t newStage)
{
    ScopedQuestOverride _;

    TP_THIS_FUNCTION(TSetStage, bool, TESQuest, uint16_t);
    POINTER_SKYRIMSE(TSetStage, SetStage, 25004);
    return SetStage(this, newStage);
}

bool TESQuest::ScriptSetStage(uint16_t stageIndex)
{
    if (IsStageReached(stageIndex))
        return false;

    using Quest = TESQuest;
    PAPYRUS_FUNCTION(void, Quest, SetCurrentStageID, int);
    s_pSetCurrentStageID(this, stageIndex);
    return IsStageReached(stageIndex);
}

void TESQuest::SetStopped()
{
    flags &= 0xFFFE;
    MarkChanged(2);
}

bool TESQuest::IsStageReached(uint16_t aStage) noexcept
{
    return currentStage == aStage || IsStageDone(aStage);
}

uint32_t TESQuest::GetStageItemCount(uint16_t aStage) noexcept
{
    for (Stage* pStage : stages)
    {
        if (!pStage || pStage->stageIndex != aStage)
            continue;

        uint32_t count = 0;
        for (void* pItem : pStage->items)
        {
            if (pItem)
                ++count;
        }

        return count;
    }

    return 0;
}

// Address Library 24126 advances scene phases; verified against 1.7.104 in scene-update-disassembly.txt.
// Suspending only this scene leaves actor loading and character previews running.
TP_THIS_FUNCTION(TUpdateScene, void, BGSScene);
TUpdateScene* RealUpdateScene = nullptr;
void TP_MAKE_THISCALL(HookUpdateScene, BGSScene)
{
    if (!HelgenService::HoldScene(apThis->formID) && World::Get().ctx().at<HelgenService>().PrepareSceneUpdate(apThis))
        TiltedPhoques::ThisCall(RealUpdateScene, apThis);
}

static TiltedPhoques::Initializer s_questInitHooks(
    []()
    {
        POINTER_SKYRIMSE(TUpdateScene, updateScene, 24126);
        RealUpdateScene = updateScene.Get();
        TP_HOOK(&RealUpdateScene, HookUpdateScene);
        // kill quest init in cold blood
        // TiltedPhoques::Write<uint8_t>(25003, 0xC3);
    });
