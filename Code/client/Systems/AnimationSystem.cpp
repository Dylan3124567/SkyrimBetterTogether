#include <TiltedOnlinePCH.h>

#include <Systems/AnimationSystem.h>

#include <Games/Animation/TESActionData.h>
#include <Games/Animation/ActorMediator.h>

#include <Games/References.h>

#include <Forms/BGSAction.h>
#include <AI/AIProcess.h>
#include <Misc/MiddleProcess.h>

#include <Messages/RequestActionEvents.h>
#include <Structs/Movement.h>

#include <Components.h>
#include <World.h>

#include <Forms/TESObjectCELL.h>
#include <Forms/TESWorldSpace.h>

#include <Systems/NpcSyncRules.h>
#include <Systems/SyncRules.h>

static_assert(NpcSyncRules::cLifeStateMask == ActorState::kLifeStateMask);
static_assert(ActionEvent::kFurnitureStateMask == ActorState::kSitSleepStateMask);

void AnimationSystem::Update(World& aWorld, Actor* apActor, RemoteAnimationComponent& aAnimationComponent, InterpolationComponent& aInterpolationComponent, const uint64_t aTick) noexcept
{
    auto& actions = aAnimationComponent.TimePoints;

    const auto it = std::begin(actions);
    const bool cHasDueAction = it != std::end(actions) && it->Tick <= aTick;
    if (cHasDueAction || aAnimationComponent.ResetAnimationGraphForReplay)
    {
        // Check if animation graph is ready before attempting to play animations
        if (!apActor->animationGraphHolder.IsReady())
        {
            // Animation graph not ready, keep the action in queue and try again later
            return;
        }
        // Also when the replay has nothing left to play, as when getting up was the last thing its owner did
        if (aAnimationComponent.ResetAnimationGraphForReplay)
        {
            apActor->animationGraphHolder.RevertAnimationGraphManager();
            aAnimationComponent.ResetAnimationGraphForReplay = false;
        }

        if (!cHasDueAction)
            return;

        const auto& first = *it;

        // Played now it would sit the copy down where its owner no longer is, or where the furniture is missing
        const bool cIsSeating = first.IsSeatingAction();
        if (cIsSeating && !SyncRules::ShouldPlaySeatingAction(aInterpolationComponent.HasOwnerState, aInterpolationComponent.IsOwnerInFurnitureHere()))
        {
            spdlog::debug("Skipped seating action {} on {:X}, its owner is out of furniture this client has", first.EventName.c_str(), apActor->formID);

            if (aAnimationComponent.ReplayCount > 0)
                aAnimationComponent.ReplayCount--;

            actions.pop_front();
            return;
        }

        if (cIsSeating)
            aInterpolationComponent.MayBeInFurniture = true;

        const auto actionId = first.ActionId;

        // Target ids are the owner's own form ids. A temporary one means nothing here, or something else entirely, and
        // a plugin loaded in another place moves the rest. The furniture comes from the owner's snapshots instead, which
        // name it the way every client understands, and the furniture is what a furniture action targets.
        uint32_t targetId = first.TargetId < 0xFF000000 ? first.TargetId : 0;
        if (cIsSeating && aInterpolationComponent.OwnerFurniture)
            targetId = aInterpolationComponent.OwnerFurniture;

        const auto pAction = Cast<BGSAction>(TESForm::GetById(actionId));
        const auto pTarget = Cast<TESObjectREFR>(TESForm::GetById(targetId));

        apActor->actorState.flags1 = NpcSyncRules::MergeReplayedState(apActor->actorState.flags1, first.State1, apActor->IsDead());
        apActor->actorState.flags2 = first.State2;

        apActor->LoadAnimationVariables(first.Variables);

        aAnimationComponent.LastRanAction = first;

        // Play the animation
        TESActionData actionData(first.Type & 0x3, apActor, pAction, pTarget);
        actionData.eventName = BSFixedString(first.EventName.c_str());
        actionData.idleForm = Cast<TESIdleForm>(TESForm::GetById(first.IdleId));
        actionData.someFlag = ((first.Type & 0x4) != 0) ? 1 : 0;

        const auto result = ActorMediator::Get()->ForceAction(&actionData);

        if (aAnimationComponent.ReplayCount > 0)
            aAnimationComponent.ReplayCount--;

        actions.pop_front();
    }
}

void AnimationSystem::Setup(World& aWorld, const entt::entity aEntity) noexcept
{
    aWorld.emplace_or_replace<RemoteAnimationComponent>(aEntity);
}

void AnimationSystem::Clean(World& aWorld, const entt::entity aEntity) noexcept
{
    if (aWorld.all_of<RemoteAnimationComponent>(aEntity))
        aWorld.remove<RemoteAnimationComponent>(aEntity);
}

void AnimationSystem::AddActionsForReplay(RemoteAnimationComponent& aAnimationComponent,
                                          const ActionReplayChain& acReplay) noexcept
{
    // A replay rebuilds past state, so it is due right away rather than when playback reaches the original ticks
    for (auto action : acReplay.Actions)
    {
        action.Tick = 0;
        aAnimationComponent.TimePoints.push_back(std::move(action));
    }

    aAnimationComponent.ReplayCount = acReplay.Actions.size();
    aAnimationComponent.ResetAnimationGraphForReplay = acReplay.ResetAnimationGraph;
}

void AnimationSystem::AddAction(RemoteAnimationComponent& aAnimationComponent, const std::string& acActionDiff) noexcept
{
    auto itor = std::begin(aAnimationComponent.TimePoints);
    const auto end = std::cend(aAnimationComponent.TimePoints);

    auto& lastProcessedAction = aAnimationComponent.LastProcessedAction;

    TiltedPhoques::ViewBuffer buffer((uint8_t*)acActionDiff.data(), acActionDiff.size());
    Buffer::Reader reader(&buffer);

    lastProcessedAction.ApplyDifferential(reader);

    aAnimationComponent.TimePoints.push_back(lastProcessedAction);
}

void AnimationSystem::SerializeMovement(World& aWorld, Actor* apActor, Movement& aMovement) noexcept
{
    if (const auto pCell = apActor->parentCell)
        aWorld.GetModSystem().GetServerModId(pCell->formID, aMovement.CellId.ModId, aMovement.CellId.BaseId);

    if (const auto pWorldSpace = apActor->GetWorldSpace())
        aWorld.GetModSystem().GetServerModId(pWorldSpace->formID, aMovement.WorldSpaceId.ModId, aMovement.WorldSpaceId.BaseId);

    aMovement.Position = apActor->position;

    aMovement.Rotation.x = apActor->rotation.x;
    aMovement.Rotation.y = apActor->rotation.z;

    apActor->SaveAnimationVariables(aMovement.Variables);
    aMovement.IsHeldInPlace = apActor->actorState.IsHeldInPlace();

    // Copies only sit when this furniture is there for them too. A temporary one means nothing on other clients.
    aMovement.FurnitureId = {};
    if (aMovement.IsHeldInPlace)
    {
        const TESObjectREFR* pFurniture = apActor->GetOccupiedFurniture();
        if (pFurniture && !pFurniture->IsTemporary())
            aWorld.GetModSystem().GetServerModId(pFurniture->formID, aMovement.FurnitureId);
    }

    if (apActor->currentProcess && apActor->currentProcess->middleProcess)
    {
        aMovement.Direction = apActor->currentProcess->middleProcess->direction;
    }
}

void AnimationSystem::SerializeActions(RequestActionEvents& aMessage, LocalComponent& localComponent, LocalAnimationComponent& animationComponent) noexcept
{
    if (animationComponent.Actions.empty())
        return;

    localComponent.CurrentAction = animationComponent.Actions.back();

    aMessage.Actions[localComponent.Id] = std::move(animationComponent.Actions);
    animationComponent.Actions.clear();
}

bool AnimationSystem::Serialize(World& aWorld, const ActionEvent& aActionEvent, const ActionEvent& aLastProcessedAction, std::string* apData)
{
    uint32_t actionBaseId = 0;
    uint32_t actionModId = 0;
    if (!aWorld.GetModSystem().GetServerModId(aActionEvent.ActionId, actionModId, actionBaseId))
        return false;

    uint32_t targetBaseId = 0;
    uint32_t targetModId = 0;
    if (!aWorld.GetModSystem().GetServerModId(aActionEvent.TargetId, targetModId, targetBaseId))
        return false;

    uint8_t scratch[1 << 14];
    TiltedPhoques::ViewBuffer buffer(scratch, std::size(scratch));
    Buffer::Writer writer(&buffer);
    aActionEvent.GenerateDifferential(aLastProcessedAction, writer);

    apData->assign(buffer.GetData(), buffer.GetData() + writer.Size());

    return true;
}
