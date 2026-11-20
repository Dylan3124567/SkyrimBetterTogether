#include <TiltedOnlinePCH.h>

#include <Events/ConnectedEvent.h>
#include <Events/DisconnectedEvent.h>
#include <Events/PartyLeftEvent.h>
#include <Events/PartyJoinedEvent.h>
#include <Events/UpdateEvent.h>

#include <Services/QuestService.h>
#include <Services/HelgenService.h>
#include <Services/ImguiService.h>
#include <Services/OverlayService.h>
#include <Services/PapyrusService.h>
#include <Services/TransportService.h>

#include <OverlayApp.hpp>

#include <PlayerCharacter.h>
#include <TESObjectREFR.h>
#include <Forms/ImageSpaceModifierInstanceForm.h>
#include <Forms/TESImageSpaceModifier.h>
#include <AI/Movement/PlayerControls.h>
#include <Forms/TESQuest.h>
#include <Interface/UI.h>
#include <Games/TES.h>
#include <Games/Overrides.h>

#include <Events/EventDispatcher.h>

#include <Messages/RequestQuestUpdate.h>
#include <Messages/NotifyQuestUpdate.h>
#include <Structs/QuestSharing.h>

using HeldStages = QuestService::HeldStages;

static void ReleaseStagesForWaitingPhases(TESQuest* apQuest, HeldStages& aHeld) noexcept;
static void ReleaseUncheckedStages(TESQuest* apQuest, HeldStages& aHeld) noexcept;
static void ReleaseStalledStages(TESQuest* apQuest, HeldStages& aHeld, double aDelta) noexcept;

QuestService::QuestService(World& aWorld, entt::dispatcher& aDispatcher)
    : m_world(aWorld)
{
    m_joinedConnection = aDispatcher.sink<ConnectedEvent>().connect<&QuestService::OnConnected>(this);
    m_updateConnection = aDispatcher.sink<UpdateEvent>().connect<&QuestService::OnUpdate>(this);
    m_disconnectedConnection = aDispatcher.sink<DisconnectedEvent>().connect<&QuestService::OnDisconnected>(this);
    m_leftConnection = aDispatcher.sink<PartyLeftEvent>().connect<&QuestService::OnPartyLeft>(this);
    m_partyJoinedConnection = aDispatcher.sink<PartyJoinedEvent>().connect<&QuestService::OnPartyJoined>(this);
    m_questUpdateConnection = aDispatcher.sink<NotifyQuestUpdate>().connect<&QuestService::OnQuestUpdate>(this);

    // Game events:
    // Starts, stops and stages are sent from TESQuestStartStopEvent and TESQuestStageEvent, which fire while the quest
    // changes. Outside a party they only prompt the overlay.
    // TESQuestStageItemDoneEvent fires once for each of a stage's log entries whose conditions passed, after its fragment
    // has run. That's too late to send on, but it tells when an applied member stage is done.
    // Scene events help diagnose held stages; phase completion also restores the local Helgen cuff-cutting controls.
    // OnUpdate explains why scene events can't trigger the release of held stages.

    auto* pEventList = EventDispatcherManager::Get();
    pEventList->questStartStopEvent.RegisterSink(this);
    pEventList->questStageEvent.RegisterSink(this);
    pEventList->questStageItemDoneEvent.RegisterSink(this);
    pEventList->sceneEvent.RegisterSink(this);
    pEventList->sceneActionEvent.RegisterSink(this);
    pEventList->scenePhaseEvent.RegisterSink(this);
    pEventList->topicInfoEvent.RegisterSink(this);
    pEventList->loadGameEvent.RegisterSink(this);
}

void QuestService::ClearSession() noexcept
{
    ++m_session;
    m_pendingUpdates.clear();
    m_heldStages.clear();
    m_appliedStages.clear();
    m_pendingStops.clear();
    m_orderedUpdates.clear();
    m_endingQuests.Entries.clear();
}

void QuestService::OnConnected(const ConnectedEvent&) noexcept
{
    ClearSession();
    // TODO: this should be followed with whatever the quest leader selected
    /*
    // deselect any active quests
    auto* pPlayer = PlayerCharacter::Get();
    for (auto& objective : pPlayer->objectives)
    {
        if (auto* pQuest = objective.instance->quest)
            pQuest->SetActive(false);
    }
    */
}

void QuestService::OnUpdate(const UpdateEvent& acEvent) noexcept
{
    if (m_pendingUpdates.empty() && m_heldStages.empty() && m_recoveries.empty() && m_crossFadeCheckIn < 0.0 && !m_world.GetPartyService().IsInParty())
        return;

    // Quest fragments can change controls, cameras and cells. Let local character creation/loading finish first.
    // The journal reads the player's objectives without the lock their writer takes (RequestObjectivesData, 53171, walks
    // PlayerCharacter+0x590 unlocked while 40510 locks +0x3E8 to grow it), counting on the game being paused while it's
    // open. Members' updates would set stages under it, and their fragments objectives.
    UI* pUI = UI::Get();
    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pUI || !pPlayer || !pPlayer->parentCell || pUI->GetMenuOpen(BSFixedString("RaceSex Menu")) || pUI->GetMenuOpen(BSFixedString("Loading Menu")) ||
        pUI->GetMenuOpen(BSFixedString("Journal Menu")))
        return;

    // A player can pull the first keep chain while Hadvar's activation package is running.
    // The chain sets 265, but the scene only checked that stage when entering phase 14.
    // Do not wait for a second NPC activation after the player achieved the action.
    // Phase 15 still waits for the gate's actual open state before proceeding.
    if (m_world.GetPartyService().IsInParty())
    {
        auto* pScene = Cast<BGSScene>(TESForm::GetById(0x20167));
        if (pScene && pScene->isPlaying && pScene->currentPhaseIndex == 14 && pScene->owningQuest && pScene->owningQuest->IsStageReached(265))
            for (auto* pAction : pScene->actions)
                if (pAction && pAction->GetIndex() == 17 && pAction->GetType() == BGSSceneAction::Type::kPackage && !pAction->IsComplete())
                {
                    pAction->status |= 2;
                    spdlog::info("Completed Helgen gate-opening action after a player pulled the chain");
                }
    }

    if (m_pendingUpdates.empty() && m_heldStages.empty() && m_appliedStages.empty() && m_pendingStops.empty() && m_recoveries.empty() &&
        m_orderedUpdates.empty() && m_crossFadeCheckIn < 0.0 && m_endingQuests.Entries.empty())
        return;

    // Give each applied stage's scripts a frame before starting the next remote update.
    if (!m_pendingUpdates.empty())
    {
        const auto update = m_pendingUpdates.front();
        m_pendingUpdates.pop_front();
        const uint32_t formId = m_world.GetModSystem().GetGameId(update.Id);
        const auto& helgen = m_world.ctx().at<HelgenService>();
        if (((formId == 0xD0593 && update.Stage >= 20) || (formId == 0x3372B && update.Stage >= 200)) && helgen.DeferTowerEntry())
            m_pendingUpdates.push_front(update); // A later inn wait must not overtake the tower-opening stages.
        else if (formId == 0xD0593 && update.Stage >= 120 && helgen.DeferExteriorBarrier(update.Stage))
            m_pendingUpdates.push_front(update); // Keep subsequent barriers behind their local crossing.
        else if ((formId == 0x3372B || formId == 0xD0593) && update.Stage >= 200 && helgen.DeferKeepEntry())
            m_pendingUpdates.push_front(update); // Preserve branch selection before its cascading stages.
        else if (update.Stage >= 500 && formId == 0x3372B && helgen.DeferBridgeCollapse())
            m_pendingUpdates.push_back(update);
        else if (WaitsForOwnSequence(update))
            m_orderedUpdates.push_back(update);
        else
            ApplyQuestUpdate(update);
    }

    UpdatePendingStops(acEvent.Delta);
    UpdateRecoveries(acEvent.Delta);
    ReleaseOrderedUpdates(acEvent.Delta);
    if (m_crossFadeCheckIn >= 0.0)
    {
        m_crossFadeCheckIn -= acEvent.Delta;
        if (m_crossFadeCheckIn < 0.0)
            RemoveLeftOverCrossFades();
    }

    // Checked every frame rather than on scene events: an action's done event fires before its fragment runs, and the
    // action only counts as complete afterwards, with no event of its own
    Vector<uint32_t> questFormIds;
    for (const auto& [questFormId, _] : m_heldStages)
        questFormIds.push_back(questFormId);

    for (uint32_t questFormId : questFormIds)
    {
        TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(questFormId));
        if (HeldStages* pHeld = GetHeldStages(questFormId, pQuest))
        {
            ReleaseStagesForWaitingPhases(pQuest, *pHeld);
            ReleaseUncheckedStages(pQuest, *pHeld);
            ReleaseStalledStages(pQuest, *pHeld, acEvent.Delta);
        }
    }
}

void QuestService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    ClearSession();
}

void QuestService::OnPartyLeft(const PartyLeftEvent&) noexcept
{
    ClearSession();
}

void QuestService::OnPartyJoined(const PartyJoinedEvent&) noexcept
{
    // Do not send events queued before this party existed to its new members.
    ClearSession();
}

BSTEventResult QuestService::OnEvent(const TESQuestStartStopEvent* apEvent, const EventDispatcher<TESQuestStartStopEvent>*)
{
    // Whatever stopped it, a member or its own scripts
    if (!apEvent->started)
    {
        for (const QuestRecovery::Rule& rule : QuestRecovery::kRules)
        {
            if (rule.Quest == apEvent->formId)
                m_world.GetRunner().Queue([this, &rule] { StartRecovery(rule); });
        }
        for (const QuestRecovery::CrossFade& crossFade : QuestRecovery::kCrossFades)
        {
            if (crossFade.Sequence == apEvent->formId)
                m_world.GetRunner().Queue([this] { m_crossFadeCheckIn = QuestRecovery::kCrossFadeDelay; });
        }
    }

    // The story manager sends {formId, true, true} when a story event failed to start the quest (32483): nothing
    // started. A stop's event doesn't say how the quest ended (25047 sends {formId, false, false}), its flags do.
    if (!ScopedQuestOverride::IsOverriden() && !(apEvent->started && apEvent->failed))
    {
        if (TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(apEvent->formId)))
        {
            const uint8_t cStatus = apEvent->started ? static_cast<uint8_t>(RequestQuestUpdate::Started) : static_cast<uint8_t>(QuestSharing::StopStatus(pQuest->flags));
            QueueQuestUpdate(apEvent->formId, pQuest->currentStage, cStatus);
        }
    }
    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESQuestStageEvent* apEvent, const EventDispatcher<TESQuestStageEvent>*)
{
    if (!ScopedQuestOverride::IsOverriden())
        QueueQuestUpdate(apEvent->formId, apEvent->stageId, RequestQuestUpdate::StageUpdate);
    return BSTEventResult::kOk;
}

void QuestService::QueueQuestUpdate(uint32_t aFormId, uint16_t aStage, uint8_t aStatus) noexcept
{
    // Script callbacks may run on workers. Resolve party, registry and mod state only on the game thread.
    m_world.GetRunner().Queue([this, session = m_session.load(), aFormId, aStage, aStatus]
    {
        if (session != m_session)
            return;
        // Checked first: the stop event that comes for a quest ended for a member must always end its entry
        if (m_endingQuests.Suppresses(aFormId, QuestSharing::IsStop(aStatus)))
        {
            spdlog::info("Not sending quest {:X} stage {} status {}: the game is ending it for a member's stop", aFormId, aStage, aStatus);
            return;
        }
        if (m_world.ctx().at<HelgenService>().LocalQuest(aFormId) || IsRecovering(aFormId))
            return;
        if (!m_world.GetPartyService().IsInParty())
        {
            if (!QuestSharing::IsStop(aStatus))
                NotifyOverlayOfQuestUpdate(aFormId);
            return;
        }

        TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(aFormId));
        if (!pQuest || IsNonSyncableQuest(pQuest) || !QuestSharing::IsShared(static_cast<uint8_t>(pQuest->type)))
            return;

        // A stage this copy sets while a member's stop waits here belongs to the run that member ended, the server would
        // take it for a new run and start the quest again everywhere
        if (aStatus == RequestQuestUpdate::StageUpdate && m_pendingStops.find(aFormId) != m_pendingStops.end())
            return;

        RequestQuestUpdate update;
        if (!m_world.GetModSystem().GetServerModId(aFormId, update.Id))
            return;
        update.Stage = aStage;
        update.Status = aStatus;
        update.ClientQuestType = static_cast<std::underlying_type_t<TESQuest::Type>>(pQuest->type);
        spdlog::info("Sending quest update: {:X}, stage: {}, status: {}", aFormId, aStage, aStatus);
        m_world.GetTransport().Send(update);
    });
}

BSTEventResult QuestService::OnEvent(const TESQuestStageItemDoneEvent* apEvent, const EventDispatcher<TESQuestStageItemDoneEvent>*)
{
    // Sent once the fragment of one of the stage's log entries has run, usually from a script thread
    m_world.GetRunner().Queue(
        [this, session = m_session.load(), questFormId = apEvent->formId, stage = apEvent->stageId]()
        {
            if (session != m_session)
                return;

            if (auto applied = m_appliedStages.find(questFormId); applied != m_appliedStages.end())
                applied.value().Done(stage);

            auto it = m_heldStages.find(questFormId);
            if (it == m_heldStages.end())
                return;

            Vector<uint16_t>& pendingItems = it.value().PendingItems;
            auto itemIt = std::find(pendingItems.begin(), pendingItems.end(), stage);
            if (itemIt == pendingItems.end())
                return;

            pendingItems.erase(itemIt);
            spdlog::debug("Fragment of applied member quest stage {} for {:X} done, {} left", stage, questFormId, pendingItems.size());
        });

    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESSceneEvent* apEvent, const EventDispatcher<TESSceneEvent>*)
{
    spdlog::debug("Scene {:X} {}", apEvent->sceneFormId, apEvent->type == 0 ? "begin" : "end");
    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESSceneActionEvent* apEvent, const EventDispatcher<TESSceneActionEvent>*)
{
    spdlog::debug("Scene {:X} action {} done, quest: {:X}, alias: {}", apEvent->sceneFormId, apEvent->actionIndex, apEvent->questFormId, apEvent->aliasId);
    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESScenePhaseEvent* apEvent, const EventDispatcher<TESScenePhaseEvent>*)
{
    // Both vanilla cuff-cutting scenes end this phase by setting MQ101 stage 240.
    // A member can receive 240 before entering the keep, then its local scene disables
    // controls again. SetStage(240) is then a no-op, so repeat only its control unlock.
    // Phase bindings: installed Skyrim.esm VMAD, Hadvar Fragment_0 / Ralof Fragment_10.
    if (apEvent->type != 0 && ((apEvent->sceneFormId == 0x20167 && apEvent->phaseIndex == 7) ||
                              (apEvent->sceneFormId == 0x4E1E2 && apEvent->phaseIndex == 10)))
        m_world.GetRunner().Queue([this, session = m_session.load()]
        {
            if (session != m_session || !m_world.GetPartyService().IsInParty())
                return;
            auto* pQuest = Cast<TESQuest>(TESForm::GetById(0x3372B));
            auto* pPlayer = PlayerCharacter::Get();
            auto* pInput = BSInputEnableManager::Get();
            if (pQuest && !pQuest->IsStopped() && pQuest->IsStageReached(240) && pPlayer && !pPlayer->actorState.IsBleedingOut() && pInput)
            {
                pInput->EnableOtherEvent(BSInputEnableManager::kPlayerControls, true, true);
                if (auto* controls = PlayerControls::GetInstance())
                    controls->SetActivateEnabled(true); // EnablePlayerControls also clears the separate autoload-door gate.
                spdlog::info("Restored controls after local Helgen cuff-cutting scene");
            }
        });

    if (!spdlog::should_log(spdlog::level::debug))
        return BSTEventResult::kOk;

    BGSScene* pScene = Cast<BGSScene>(TESForm::GetById(apEvent->sceneFormId));
    if (!pScene)
        return BSTEventResult::kOk;

    // A phase without completion conditions ends once every action running in it is complete
    String actions;
    for (BGSSceneAction* pAction : pScene->actions)
    {
        if (!pAction || !pAction->IsInPhase(apEvent->phaseIndex))
            continue;

        constexpr std::array cTypeNames{"dialogue", "package", "timer"};
        const uint32_t cType = static_cast<uint32_t>(pAction->GetType());
        actions += spdlog::fmt_lib::format(
            " [{} {} alias {} {}]", pAction->GetIndex(), cType < cTypeNames.size() ? cTypeNames[cType] : "unknown", pAction->actorId,
            pAction->IsComplete() ? "complete" : "running");
    }

    spdlog::debug("Scene {:X} phase {} {}, actions:{}", apEvent->sceneFormId, apEvent->phaseIndex, apEvent->type == 0 ? "begin" : "end", actions);
    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESLoadGameEvent*, const EventDispatcher<TESLoadGameEvent>*)
{
    // The loaded world replaces whatever was being ended, and a save can hold a quest that stopped mid-sequence
    m_world.GetRunner().Queue(
        [this]
        {
            m_recoveries.clear();
            for (const QuestRecovery::Rule& rule : QuestRecovery::kRules)
                StartRecovery(rule);
            // A save keeps the crossfade (upstream issue 835: black across restarts)
            m_crossFadeCheckIn = QuestRecovery::kCrossFadeDelay;
        });
    return BSTEventResult::kOk;
}

BSTEventResult QuestService::OnEvent(const TESTopicInfoEvent* apEvent, const EventDispatcher<TESTopicInfoEvent>*)
{
    spdlog::debug(
        "Dialogue line {:X} {}, speaker: {:X}", apEvent->uiTopicInfoFormID, apEvent->eType == 0 ? "begin" : "end", apEvent->pSpeakerRef ? apEvent->pSpeakerRef->formID : 0);
    return BSTEventResult::kOk;
}

static bool Compare(TESConditionItem::OpCode aOpCode, float aLhs, float aRhs) noexcept
{
    switch (aOpCode)
    {
    case TESConditionItem::OpCode::kEqualTo: return aLhs == aRhs;
    case TESConditionItem::OpCode::kNotEqualTo: return aLhs != aRhs;
    case TESConditionItem::OpCode::kGreaterThan: return aLhs > aRhs;
    case TESConditionItem::OpCode::kGreaterThanOrEqualTo: return aLhs >= aRhs;
    case TESConditionItem::OpCode::kLessThan: return aLhs < aRhs;
    case TESConditionItem::OpCode::kLessThanOrEqualTo: return aLhs <= aRhs;
    default: return false;
    }
}

// Whether setting the stage turns a GetStage/GetStageDone check on the quest from false to true. With aOrChecksLater,
// also whether it changes the check's result at all, or the check is on the same or a later stage.
static bool IsCheckedByStage(const TESCondition& acConditions, TESQuest* apQuest, uint16_t aStage, bool aOrChecksLater) noexcept
{
    for (const TESConditionItem* pItem = acConditions.pHead; pItem; pItem = pItem->pNext)
    {
        if (pItem->UsesGlobal() || pItem->params[0] != apQuest)
            continue;

        const TESConditionItem::OpCode cOpCode = pItem->GetOpCode();
        float checkedStage = 0.f;
        bool before = false;
        bool after = false;
        if (pItem->function == TESConditionItem::kGetStage)
        {
            // Stages only go up, GetStage returns the highest one set
            checkedStage = pItem->comparisonValue;
            before = Compare(cOpCode, apQuest->currentStage, pItem->comparisonValue);
            after = Compare(cOpCode, std::max(apQuest->currentStage, aStage), pItem->comparisonValue);
        }
        else if (pItem->function == TESConditionItem::kGetStageDone)
        {
            const uint16_t cStage = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(pItem->params[1]));
            checkedStage = cStage;
            if (cStage == aStage)
            {
                before = Compare(cOpCode, 0.f, pItem->comparisonValue);
                after = Compare(cOpCode, 1.f, pItem->comparisonValue);
            }
        }
        else
        {
            continue;
        }

        if (!before && after)
            return true;

        if (aOrChecksLater && (before != after || checkedStage >= aStage))
            return true;
    }

    return false;
}

// Whether the phase's own lines are done, so passing its completion conditions won't cut one off. Timers and packages
// don't hold such a phase back, and packages never complete on clients that don't own the NPC.
static bool IsPhaseWaitingOnConditions(BGSScene* apScene, uint32_t aPhaseIndex) noexcept
{
    for (BGSSceneAction* pAction : apScene->actions)
    {
        if (!pAction || !pAction->IsInPhase(aPhaseIndex))
            continue;

        if (pAction->GetType() != BGSSceneAction::Type::kDialogue || pAction->IsComplete() || pAction->Loops())
            continue;

        // The player's own lines wait for this player to answer, while a member may have answered instead
        if (apScene->owningQuest && apScene->owningQuest->GetAliasedRef(pAction->actorId) == PlayerCharacter::Get())
            continue;

        return false;
    }

    return true;
}

// Whether a phase the playing scenes have yet to finish checks the stage or a later one, or checks differently with it
// (a phase waiting on GetStage == N). Scenes that only end or repeat on it don't count (MQ105's sprint trial).
static bool IsCheckedByPlayingScene(TESQuest* apQuest, uint16_t aStage) noexcept
{
    for (BGSScene* pScene : apQuest->scenes)
    {
        if (!pScene)
            continue;

        for (uint32_t i = pScene->GetNextPhaseIndex(); i < pScene->phases.length; ++i)
        {
            const BGSScenePhase* pPhase = pScene->phases[i];
            if (!pPhase)
                continue;

            // The running phase is past its start conditions
            const bool cIsRunning = pScene->currentPhaseIndex == i;
            if ((!cIsRunning && IsCheckedByStage(pPhase->startConditions, apQuest, aStage, true)) ||
                IsCheckedByStage(pPhase->completionConditions, apQuest, aStage, true))
            {
                return true;
            }
        }
    }

    return false;
}

void QuestService::HoldStage(TESQuest* apQuest, uint16_t aStage) noexcept
{
    Vector<uint16_t>& stages = m_heldStages[apQuest->formID].Stages;
    auto it = std::lower_bound(stages.begin(), stages.end(), aStage);
    if (it == stages.end() || *it != aStage)
        stages.insert(it, aStage);

    spdlog::info("Holding member quest stage {} for {:X} until no playing scene of it checks it", aStage, apQuest->formID);
}

static void ApplyHeldStages(TESQuest* apQuest, HeldStages& aHeld, size_t aCount) noexcept
{
    ScopedQuestOverride _;

    // In order, a later stage's scripts can rely on the earlier ones
    for (size_t i = 0; i < aCount; ++i)
    {
        // A scene the fragments start only plays once they've run, and the next stage may be what it waits on. A log
        // entry reports when its fragment is done, but one whose conditions fail never runs (TESQuestStageItem 25260),
        // and a stage's variants usually exclude each other, so wait for the first.
        const uint16_t cStage = aHeld.Stages[i];
        if (apQuest->ScriptSetStage(cStage) && apQuest->GetStageItemCount(cStage))
            aHeld.PendingItems.push_back(cStage);
    }

    aHeld.Stages.erase(aHeld.Stages.begin(), aHeld.Stages.begin() + aCount);
}

static void ReleaseStagesForWaitingPhases(TESQuest* apQuest, HeldStages& aHeld) noexcept
{
    // A phase whose own actions are done and whose completion conditions want a held stage only moves on with it.
    // Release up to the first stage that does it, the leader's scene then carries on at its own pace.
    for (BGSScene* pScene : apQuest->scenes)
    {
        if (aHeld.Stages.empty())
            break;

        if (!pScene || !pScene->isPlaying || pScene->currentPhaseIndex >= pScene->phases.length)
            continue;

        BGSScenePhase* pPhase = pScene->phases[pScene->currentPhaseIndex];
        if (!pPhase)
            continue;

        // Conditions first, they rarely mention a held stage and are cheaper than walking the phase's actions
        const auto cStageIt = std::find_if(
            aHeld.Stages.begin(), aHeld.Stages.end(),
            [&](uint16_t aStage) { return IsCheckedByStage(pPhase->completionConditions, apQuest, aStage, false); });
        if (cStageIt == aHeld.Stages.end() || !IsPhaseWaitingOnConditions(pScene, pScene->currentPhaseIndex))
            continue;

        spdlog::info(
            "Releasing held member quest stages for {:X} up to {}, scene {:X} phase {} waits on it", apQuest->formID, *cStageIt, pScene->formID,
            pScene->currentPhaseIndex);
        ApplyHeldStages(apQuest, aHeld, cStageIt - aHeld.Stages.begin() + 1);
    }
}

// Applies held stages in order while no playing scene checks the next one, e.g. once the scenes that did have ended.
// Waits for the fragments of the stages it applied first.
static void ReleaseUncheckedStages(TESQuest* apQuest, HeldStages& aHeld) noexcept
{
    while (!aHeld.Stages.empty() && aHeld.PendingItems.empty() && !IsCheckedByPlayingScene(apQuest, aHeld.Stages.front()))
    {
        spdlog::info("Applying held member quest stage {} for {:X}, no playing scene checks it", aHeld.Stages.front(), apQuest->formID);
        ApplyHeldStages(apQuest, aHeld, 1);
    }
}

// The Helgen scenes are paced by HelgenService and legitimately wait a long time for both players.
static bool CanStall(const TESQuest* apQuest) noexcept
{
    return apQuest->formID != 0x3372B && apQuest->formID != 0xD0593;
}

static uint64_t GetSceneProgress(TESQuest* apQuest) noexcept
{
    uint64_t progress = 0;
    for (BGSScene* pScene : apQuest->scenes)
    {
        if (pScene && pScene->isPlaying)
            progress = progress * 1099511628211ull + ((static_cast<uint64_t>(pScene->formID) << 32) | pScene->currentPhaseIndex);
    }

    return progress;
}

// A scene can stop for good, and the held stage may be what would have stopped it. Then waiting only splits the party.
static void ReleaseStalledStages(TESQuest* apQuest, HeldStages& aHeld, double aDelta) noexcept
{
    if (aHeld.Stages.empty() || !CanStall(apQuest) || !aHeld.Stall.Update(GetSceneProgress(apQuest), aDelta))
        return;

    spdlog::info("Applying held member quest stage {} for {:X}, its scene has stopped moving", aHeld.Stages.front(), apQuest->formID);
    aHeld.PendingItems.clear();
    ApplyHeldStages(apQuest, aHeld, 1);
    aHeld.Stall.OnReleased();
}

HeldStages* QuestService::GetHeldStages(uint32_t aQuestFormId, TESQuest* apQuest) noexcept
{
    auto it = m_heldStages.find(aQuestFormId);
    if (it == m_heldStages.end())
        return nullptr;

    // Setting a stage on a stopped quest starts it again, and scenes also end because their quest stopped
    HeldStages& held = it.value();
    if (!apQuest || apQuest->IsStopped())
    {
        if (!held.Stages.empty())
            spdlog::info("Dropping held member quest stages for {:X}, quest stopped or missing", aQuestFormId);

        m_heldStages.erase(it);
        return nullptr;
    }

    // Reached by the leader's own scripts in the meantime
    std::erase_if(
        held.Stages,
        [apQuest](uint16_t aStage)
        {
            if (!apQuest->IsStageReached(aStage))
                return false;

            spdlog::info("Dropping held member quest stage {} for {:X}, already reached here", aStage, apQuest->formID);
            return true;
        });

    if (held.Stages.empty() && held.PendingItems.empty())
    {
        m_heldStages.erase(it);
        return nullptr;
    }

    return &held;
}

void QuestService::OnQuestUpdate(const NotifyQuestUpdate& aUpdate) noexcept
{
    if (m_world.ctx().at<HelgenService>().LocalQuest(m_world.GetModSystem().GetGameId(aUpdate.Id)))
        return;
    if (m_world.GetPartyService().IsInParty())
        m_pendingUpdates.push_back(aUpdate);
}

void QuestService::ApplyQuestUpdate(const NotifyQuestUpdate& aUpdate) noexcept
{
    if (!m_world.GetPartyService().IsInParty())
        return;

    ModSystem& modSystem = World::Get().GetModSystem();
    uint32_t formId = modSystem.GetGameId(aUpdate.Id);
    TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(formId));
    if (!pQuest)
    {
        spdlog::error("Failed to find quest, base id: {:X}, mod id: {:X}", aUpdate.Id.BaseId, aUpdate.Id.ModId);
        return;
    }

    // A later update means the member moved on after stopping the quest (a repeatable one starting over), so a stop still
    // waiting here can't wait any longer
    if (const auto stopIt = m_pendingStops.find(formId); stopIt != m_pendingStops.end())
    {
        const uint8_t cStatus = stopIt->second.Status;
        m_pendingStops.erase(stopIt);
        EndQuest(pQuest, cStatus);
    }

    // The game stops it a frame or more later: a restart or stage applied before then would mix into the run being
    // stopped. Waits in front of the queue until the stop event, at most kEndingWait.
    if (m_endingQuests.Contains(formId))
    {
        m_pendingUpdates.push_front(aUpdate);
        return;
    }

    if (aUpdate.Status == NotifyQuestUpdate::StageUpdate && pQuest->IsStageReached(aUpdate.Stage))
    {
        spdlog::info("Remote quest {:X} already reached stage {}, current stage: {}", formId, aUpdate.Stage, pQuest->currentStage);
        return;
    }

    // A member stage the leader's playing scene checks can push it past its own scripts (a phase ending before its lines
    // and scripts ran), so hold it, and the stages after it, until the scene gets there or reaches it itself.
    const bool cIsStop = QuestSharing::IsStop(aUpdate.Status);
    const bool cIsLeaderStage = aUpdate.Status == NotifyQuestUpdate::StageUpdate && m_world.GetPartyService().IsLeader();
    HeldStages* pHeld = cIsLeaderStage || cIsStop ? GetHeldStages(formId, pQuest) : nullptr;
    if (cIsLeaderStage && (pHeld || IsCheckedByPlayingScene(pQuest, aUpdate.Stage)))
    {
        HoldStage(pQuest, aUpdate.Stage);
        return;
    }

    // The member reached the held stages before stopping the quest, and once it's stopped here they're dropped. The stop
    // waits for their fragments, see PendingItems.
    if (pHeld && !pHeld->Stages.empty())
    {
        spdlog::info("Applying {} held member quest stages for {:X} before it stops", pHeld->Stages.size(), formId);
        ApplyHeldStages(pQuest, *pHeld, pHeld->Stages.size());
    }

    if (cIsStop)
    {
        // Nothing of the member's run to end in a quest that doesn't run here (it never started, or stopped itself)
        if (pQuest->IsStopped())
        {
            spdlog::info("Remote quest {:X} ended, status: {}, it doesn't run here", formId, aUpdate.Status);
            return;
        }

        m_pendingStops[formId] = QuestSharing::PendingStop{aUpdate.Status};
        spdlog::info("Remote quest {:X} ended, status: {}, ending it here once its member stages' fragments have run", formId, aUpdate.Status);
        return;
    }

    // The quest events fire synchronously while the stage is set, don't send them back
    ScopedQuestOverride _;

    bool bResult = false;
    switch (aUpdate.Status)
    {
    case NotifyQuestUpdate::Started:
    {
        // A start carries the stage the quest was at, usually 0, which a quest that never ran here counts as reached
        // already, so it never started. Start it the way the game does, which runs its startup stage and fills its
        // aliases from this world, then catch up on the stage.
        bResult = !pQuest->IsStopped();
        if (!bResult)
            pQuest->EnsureQuestStarted(bResult, true);
        if (bResult && !pQuest->IsStageReached(aUpdate.Stage))
        {
            bResult = pQuest->ScriptSetStage(aUpdate.Stage);
            if (bResult)
                TrackFragments(pQuest, aUpdate.Stage);
        }
        pQuest->SetActive(true);
        spdlog::info("Remote quest started: {:X}, stage: {}", formId, aUpdate.Stage);
        break;
    }
    case NotifyQuestUpdate::StageUpdate:
        bResult = pQuest->ScriptSetStage(aUpdate.Stage);
        if (bResult)
            TrackFragments(pQuest, aUpdate.Stage);
        spdlog::info("Remote quest updated: {:X}, stage: {}", formId, aUpdate.Stage);
        break;
    default: break;
    }

    if (!bResult)
        spdlog::error("Failed to update the client quest state, quest: {:X}, stage: {}, status: {}", formId, aUpdate.Stage, aUpdate.Status);
}

void QuestService::TrackFragments(TESQuest* apQuest, uint16_t aStage) noexcept
{
    // A stage without log entries has no fragments
    if (apQuest->GetStageItemCount(aStage))
        m_appliedStages[apQuest->formID].Add(aStage);
}

bool QuestService::AreFragmentsRunning(uint32_t aQuestFormId) const noexcept
{
    const auto applied = m_appliedStages.find(aQuestFormId);
    if (applied != m_appliedStages.end() && applied->second.Running())
        return true;

    const auto held = m_heldStages.find(aQuestFormId);
    return held != m_heldStages.end() && !held->second.PendingItems.empty();
}

void QuestService::UpdatePendingStops(double aDelta) noexcept
{
    m_endingQuests.Update(aDelta);

    for (auto it = m_appliedStages.begin(); it != m_appliedStages.end();)
    {
        it.value().Age += aDelta;
        if (it->second.Running())
            ++it;
        else
            it = m_appliedStages.erase(it);
    }

    // Ending a quest runs game code, so collect first
    Vector<uint32_t> ready;
    for (auto it = m_pendingStops.begin(); it != m_pendingStops.end(); ++it)
    {
        if (it.value().Update(AreFragmentsRunning(it->first), aDelta))
            ready.push_back(it->first);
    }

    for (uint32_t questFormId : ready)
    {
        const auto it = m_pendingStops.find(questFormId);
        if (it == m_pendingStops.end())
            continue;

        const uint8_t cStatus = it->second.Status;
        m_pendingStops.erase(it);
        if (TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(questFormId)))
            EndQuest(pQuest, cStatus);
    }
}

// Ends the quest the way the member's ended, unless its own fragments did already: completed or failed as a log entry
// does, then stopped as Papyrus Quest.Stop does. The game then stops its scenes and clears its aliases.
void QuestService::EndQuest(TESQuest* apQuest, uint8_t aStatus) noexcept
{
    if (QuestSharing::NeedsOutcome(aStatus, apQuest->flags))
        apQuest->SetEnded(aStatus == QuestSharing::kFailed);

    // Enabled or already stopping, the game's stop event and shutdown stages are still to come, and are the member's
    if (!apQuest->IsStopped())
        m_endingQuests.Add(apQuest->formID);

    const bool cWasRunning = apQuest->IsEnabled();
    apQuest->Stop();
    spdlog::info("Ended remote quest {:X}, status: {}, stage: {}{}", apQuest->formID, aStatus, apQuest->currentStage, cWasRunning ? "" : ", its own scripts had stopped it");
}

void QuestService::StartRecovery(const QuestRecovery::Rule& acRule) noexcept
{
    TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(acRule.Quest));
    if (!pQuest || !QuestRecovery::Applies(pQuest->IsStopped(), pQuest->IsStageDone(acRule.SinceStage), pQuest->IsStageReached(acRule.UntilStage)))
        return;

    const TESObjectREFR* pDoor = Cast<TESObjectREFR>(TESForm::GetById(acRule.Door));
    const auto* pLock = pDoor ? pDoor->GetLock() : nullptr;
    const bool cUnlockDoor = pLock && pLock->IsLocked();

    // A stopped controller's loop has ended (only it stops the controller), and setting a stage would start it again
    TESQuest* pController = Cast<TESQuest>(TESForm::GetById(acRule.Controller));
    const bool cEndSequence = pController && !pController->IsStopped() && pController->IsStageDone(acRule.ControllerStartStage) &&
                              !pController->IsStageReached(acRule.ControllerStages.back());

    if (!cUnlockDoor && !cEndSequence)
        return;

    m_recoveries[acRule.Quest] = Recovery{&acRule, cUnlockDoor, cEndSequence};
    spdlog::info(
        "Quest {:X} stopped at stage {} before stage {}, ending what it left running: door {:X} {}, controller {:X} {}", acRule.Quest, pQuest->currentStage,
        acRule.UntilStage, acRule.Door, cUnlockDoor ? "to unlock" : "unlocked", acRule.Controller, cEndSequence ? "to end" : "not running");
}

void QuestService::UpdateRecoveries(double aDelta) noexcept
{
    Vector<uint32_t> finished;
    for (auto it = m_recoveries.begin(); it != m_recoveries.end(); ++it)
    {
        Recovery& recovery = it.value();
        const QuestRecovery::Rule& rule = *recovery.pRule;

        if (recovery.UnlockDoor)
        {
            recovery.UnlockDoor = false;
            if (TESObjectREFR* pDoor = Cast<TESObjectREFR>(TESForm::GetById(rule.Door)))
            {
                // What the stage's fragment does, Lock(false, false): Papyrus ObjectReference.Lock (native 56198) unlocks
                // through GetLock 20223, Lock::SetLock 12401 and LockChange 19512, as ObjectService does for members
                using ObjectReference = TESObjectREFR;
                PAPYRUS_FUNCTION(void, ObjectReference, Lock, bool, bool);
                s_pLock(pDoor, false, false);
                spdlog::info("Unlocked {:X}, quest {:X} stopped before unlocking it", rule.Door, rule.Quest);
            }
        }

        TESQuest* pController = recovery.EndSequence ? Cast<TESQuest>(TESForm::GetById(rule.Controller)) : nullptr;
        if (!pController || recovery.Steps.Finished())
        {
            finished.push_back(it->first);
            continue;
        }

        const auto cStage = recovery.Steps.Update(rule, aDelta, pController->IsStopped(), [pController](uint16_t aStage) { return pController->IsStageReached(aStage); });
        if (cStage)
        {
            pController->ScriptSetStage(*cStage);
            spdlog::info("Set stage {} of {:X}, quest {:X} stopped before it would", *cStage, rule.Controller, rule.Quest);
        }
    }

    for (uint32_t questFormId : finished)
        m_recoveries.erase(questFormId);
}

bool QuestService::IsRecovering(uint32_t aControllerFormId) const noexcept
{
    for (const auto& [_, recovery] : m_recoveries)
    {
        if (recovery.EndSequence && recovery.pRule->Controller == aControllerFormId)
            return true;
    }

    return false;
}

bool QuestService::MustWaitForOwnSequence(const NotifyQuestUpdate& acUpdate) noexcept
{
    const uint32_t cFormId = m_world.GetModSystem().GetGameId(acUpdate.Id);
    for (const QuestRecovery::StageOrder& order : QuestRecovery::kStageOrders)
    {
        if (order.Quest != cFormId)
            continue;

        const TESQuest* pSetBy = Cast<TESQuest>(TESForm::GetById(order.SetBy));
        const bool cSetByRunning = pSetBy && !pSetBy->IsStopped();
        // The next run of the sequence gets its own wait
        if (!cSetByRunning)
            m_orderWaited = 0.0;

        if (QuestRecovery::MustWait(order, cFormId, acUpdate.Stage, cSetByRunning, m_orderWaited))
            return true;
    }

    return false;
}

// Updates of a quest keep their order: once one waits, the later ones wait behind it
bool QuestService::WaitsForOwnSequence(const NotifyQuestUpdate& acUpdate) noexcept
{
    const bool cQuestWaits = std::any_of(m_orderedUpdates.begin(), m_orderedUpdates.end(), [&acUpdate](const NotifyQuestUpdate& acWaiting) { return acWaiting.Id == acUpdate.Id; });
    if (!cQuestWaits && !MustWaitForOwnSequence(acUpdate))
        return false;

    spdlog::info("Member quest update {:X} stage {} waits for this client's own sequence to set it", acUpdate.Id.LogFormat(), acUpdate.Stage);
    return true;
}

void QuestService::ReleaseOrderedUpdates(double aDelta) noexcept
{
    if (m_orderedUpdates.empty())
        return;

    m_orderWaited += aDelta;
    const double cWaited = m_orderWaited;
    if (MustWaitForOwnSequence(m_orderedUpdates.front()))
        return;

    // Back in front of the queue, in order. A stage the sequence set itself is already reached by then.
    spdlog::info("Releasing {} member quest updates that waited {:.0f} s for this client's own sequence", m_orderedUpdates.size(), cWaited);
    m_pendingUpdates.insert(m_pendingUpdates.begin(), m_orderedUpdates.begin(), m_orderedUpdates.end());
    m_orderedUpdates.clear();
}

void QuestService::RemoveLeftOverCrossFades() noexcept
{
    // ApplyCrossFade (18575, Papyrus native 55937) keeps its instance at 400901 until RemoveCrossFade (18577, native
    // 55949) fades it out over the given seconds and clears it. The instance's modifier is at +0x28 (built by 18565).
    POINTER_SKYRIMSE(ImageSpaceModifierInstanceForm*, s_crossFade, 400901);
    using TRemoveCrossFade = void(float);
    POINTER_SKYRIMSE(TRemoveCrossFade, s_removeCrossFade, 18577);

    for (const QuestRecovery::CrossFade& crossFade : QuestRecovery::kCrossFades)
    {
        TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(crossFade.Quest));
        const TESQuest* pSequence = Cast<TESQuest>(TESForm::GetById(crossFade.Sequence));
        const ImageSpaceModifierInstanceForm* pCurrent = *s_crossFade.Get();
        const bool cApplied = pCurrent && pCurrent->pMod && pCurrent->pMod->formID == crossFade.Modifier;
        if (!pQuest || !QuestRecovery::IsLeftOver(pQuest->IsStageDone(crossFade.Stage), pSequence && !pSequence->IsStopped(), cApplied))
            continue;

        s_removeCrossFade.Get()(1.f);
        spdlog::info("Removed crossfade {:X}: quest {:X} reached stage {} that replaces it before {:X} applied it", crossFade.Modifier, crossFade.Quest, crossFade.Stage, crossFade.Sequence);
    }
}

void QuestService::NotifyOverlayOfQuestUpdate(uint32_t aFormId) noexcept
{
    if (!m_world.GetTransport().IsOnline())
        return;

    // Nobody around to party with, or the invite toast already prompts the player to join one.
    const auto remotePlayerView = m_world.view<PlayerComponent, RemoteComponent>();
    if (m_world.GetPartyService().HasPendingInvitations() || remotePlayerView.begin() == remotePlayerView.end())
        return;

    TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(aFormId));
    if (!pQuest || IsNonSyncableQuest(pQuest))
        return;

    // Hidden background quests and minor misc tasks are too noisy to prompt about
    if (pQuest->type == TESQuest::Type::None || pQuest->type == TESQuest::Type::Miscellaneous)
        return;

    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("questUpdated");
}

static constexpr std::array kNonSyncableQuestIds = std::to_array<uint32_t>({
    0x2BA16,   // Werewolf transformation quest
    0x20071D0, // Vampire transformation quest
    0x3AC44,   // MS13BleakFallsBarrowLeverScene
    // 0xFE014801,  // Unknown dynamic ID, kept as note, maybe lookup correct ID this game?
    0xF2593 // Skill experience quest
});

bool QuestService::IsNonSyncableQuest(TESQuest* apQuest)
{
    // Quests with no quest stages are never synced. Most TESQues::Type:: quests should
    // be synced, including Type::None and Type::Miscellaneous, but there are a few
    // known exceptions that should be excluded that are in the table.
    return    apQuest->stages.Empty() 
           || std::find(kNonSyncableQuestIds.begin(), kNonSyncableQuestIds.end(), apQuest->formID) != kNonSyncableQuestIds.end();
}

void QuestService::DebugDumpQuests()
{
    auto& quests = ModManager::Get()->quests;
    for (TESQuest* pQuest : quests)
        spdlog::info("{:X}|{}|{}|{}", pQuest->formID, (uint8_t)pQuest->type, pQuest->priority, pQuest->idName.AsAscii());
}
