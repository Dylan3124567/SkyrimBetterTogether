#include <TiltedOnlinePCH.h>

#include <Services/ActorValueService.h>
#include <Services/CharacterService.h>
#include <World.h>
#include <Forms/ActorValueInfo.h>
#include <Games/References.h>
#include <Games/Misc/SubtitleManager.h>
#include <Components.h>

#include <Events/UpdateEvent.h>
#include <Events/ActorRemovedEvent.h>
#include <Events/ConnectedEvent.h>
#include <Events/DisconnectedEvent.h>
#include <Events/HealthChangeEvent.h>

#include <Messages/NotifyActorValueChanges.h>
#include <Messages/RequestActorValueChanges.h>
#include <Messages/NotifyActorMaxValueChanges.h>
#include <Messages/RequestActorMaxValueChanges.h>
#include <Messages/NotifyHealthChangeBroadcast.h>
#include <Messages/RequestHealthChangeBroadcast.h>
#include <Messages/NotifyDeathStateChange.h>
#include <Messages/RequestDeathStateChange.h>

#include <misc/ActorValueOwner.h>
#include <Forms/TESQuest.h>
#include <PlayerCharacter.h>
#include <Systems/CombatRules.h>

namespace
{
// The game of the client sending a blow would only knock the actor down if its player dealt it
bool IsKnockOnly(Actor* apActor) noexcept
{
    return apActor && !apActor->CanBeKilledBy(PlayerCharacter::Get());
}

// Server ids are entity integers and 0 is one, see DeathMemory::cNoActor
uint32_t GetServerIdOf(World& aWorld, const TESObjectREFR* apReference) noexcept
{
    if (!apReference)
        return DeathMemory::cNoActor;

    auto view = aWorld.view<FormIdComponent>();
    for (auto entity : view)
    {
        if (view.get<FormIdComponent>(entity).Id == apReference->formID)
            return Utils::GetServerId(entity).value_or(DeathMemory::cNoActor);
    }
    return DeathMemory::cNoActor;
}

Actor* GetActorByServerId(const uint32_t aServerId) noexcept
{
    return aServerId != DeathMemory::cNoActor ? Utils::GetByServerId<Actor>(aServerId) : nullptr;
}
} // namespace

ActorValueService::ActorValueService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_dispatcher(aDispatcher)
    , m_transport(aTransport)
{
    m_world.on_construct<LocalComponent>().connect<&ActorValueService::OnLocalComponentAdded>(this);
    m_world.on_destroy<LocalComponent>().connect<&ActorValueService::OnLocalComponentRemoved>(this);
    m_dispatcher.sink<DisconnectedEvent>().connect<&ActorValueService::OnDisconnected>(this);
    m_dispatcher.sink<ActorRemovedEvent>().connect<&ActorValueService::OnActorRemoved>(this);
    m_dispatcher.sink<UpdateEvent>().connect<&ActorValueService::OnUpdate>(this);
    m_dispatcher.sink<NotifyActorValueChanges>().connect<&ActorValueService::OnActorValueChanges>(this);
    m_dispatcher.sink<NotifyActorMaxValueChanges>().connect<&ActorValueService::OnActorMaxValueChanges>(this);
    m_dispatcher.sink<HealthChangeEvent>().connect<&ActorValueService::OnHealthChange>(this);
    m_dispatcher.sink<NotifyHealthChangeBroadcast>().connect<&ActorValueService::OnHealthChangeBroadcast>(this);
    m_dispatcher.sink<NotifyDeathStateChange>().connect<&ActorValueService::OnDeathStateChange>(this);
}

void ActorValueService::CreateActorValuesComponent(const entt::entity aEntity, Actor* apActor) noexcept
{
    auto& actorValuesComponent = m_world.emplace_or_replace<ActorValuesComponent>(aEntity);

    for (int i = 0; i < ActorValueInfo::kActorValueCount; i++)
    {
        float value = apActor->GetActorValue(i);
        actorValuesComponent.CurrentActorValues.ActorValuesList.insert({i, value});
        float maxValue = apActor->GetActorPermanentValue(i);
        actorValuesComponent.CurrentActorValues.ActorMaxValuesList.insert({i, maxValue});
    }
}

void ActorValueService::OnLocalComponentAdded(entt::registry& aRegistry, const entt::entity aEntity) noexcept
{
    const auto& formIdComponent = aRegistry.get<FormIdComponent>(aEntity);
    Actor* pActor = Cast<Actor>(TESForm::GetById(formIdComponent.Id));

    if (pActor != NULL)
    {
        auto& localComponent = aRegistry.get<LocalComponent>(aEntity);
        localComponent.IsDead = pActor->IsDead();
        localComponent.IsWeaponDrawn = pActor->actorState.IsWeaponDrawn();
        CreateActorValuesComponent(aEntity, pActor);
    }
}

void ActorValueService::OnLocalComponentRemoved(entt::registry& aRegistry, const entt::entity aEntity) noexcept
{
    m_downActors.erase(aEntity);
}

void ActorValueService::OnDisconnected(const DisconnectedEvent& acEvent) noexcept
{
    // Batched damage uses server IDs, which can refer to different actors after reconnecting.
    m_smallHealthChanges.clear();
    m_downActors.clear();
    m_knockedOut.clear();
    m_deadDebtors.clear();
    // TODO: investigate the previously reported intermittent component-clear crash.
    m_world.clear<ActorValuesComponent>();
}

void ActorValueService::OnActorRemoved(const ActorRemovedEvent& acEvent) noexcept
{
    m_knockedOut.erase(acEvent.FormId);
    // Unloaded, how long it has been dead here counts again from the next time it is seen
    m_deadDebtors.erase(acEvent.FormId);

    if (!m_transport.IsConnected())
        return;

    auto view = m_world.view<FormIdComponent>();
    const uint32_t formId = acEvent.FormId;

    const auto it = std::find_if(
        std::begin(view), std::end(view),
        [view, formId](auto entity)
        {
            const auto& formIdComponent = view.get<FormIdComponent>(entity);

            return formIdComponent.Id == formId;
        });

    if (it != std::end(view))
        m_world.remove<ActorValuesComponent>(*it);
}

void ActorValueService::OnUpdate(const UpdateEvent& acEvent) noexcept
{
    RunSmallHealthUpdates();
    RunDeathStateUpdates();
    RunActorValuesUpdates();
    RunBrawlRepairs();
}

void ActorValueService::BroadcastActorValues() noexcept
{
    if (!m_transport.IsConnected())
        return;

    auto view = m_world.view<FormIdComponent, LocalComponent, ActorValuesComponent>();

    for (auto entity : view)
    {
        auto& formIdComponent = view.get<FormIdComponent>(entity);
        auto* pForm = TESForm::GetById(formIdComponent.Id);
        auto* pActor = Cast<Actor>(pForm);

        if (!pActor)
            continue;

        auto& localComponent = view.get<LocalComponent>(entity);
        auto& actorValuesComponent = view.get<ActorValuesComponent>(entity);

        RequestActorValueChanges requestValueChanges;
        requestValueChanges.Id = localComponent.Id;
        requestValueChanges.OwnershipEpoch = localComponent.OwnershipEpoch;
        RequestActorMaxValueChanges requestMaxValueChanges;
        requestMaxValueChanges.Id = localComponent.Id;
        requestMaxValueChanges.OwnershipEpoch = localComponent.OwnershipEpoch;

        bool isPlayer = pActor->GetExtension() && pActor->GetExtension()->IsPlayer();

        for (int i = 0; i < ActorValueInfo::kActorValueCount; i++)
        {
            if (isPlayer && i == ActorValueInfo::kDragonSouls)
                continue;
            
            float newValue = pActor->GetActorValue(i);
            float oldValue = actorValuesComponent.CurrentActorValues.ActorValuesList[i];
            if (newValue != oldValue)
            {
                requestValueChanges.Values.insert({i, newValue});
                actorValuesComponent.CurrentActorValues.ActorValuesList[i] = newValue;
            }

            float newMaxValue = pActor->GetActorPermanentValue(i);
            float oldMaxValue = actorValuesComponent.CurrentActorValues.ActorMaxValuesList[i];
            if (newMaxValue != oldMaxValue)
            {
                requestMaxValueChanges.Values.insert({i, newMaxValue});
                actorValuesComponent.CurrentActorValues.ActorMaxValuesList[i] = newMaxValue;
            }
        }

        if (requestValueChanges.Values.size() > 0)
        {
            m_transport.Send(requestValueChanges);
        }

        if (requestMaxValueChanges.Values.size() > 0)
        {
            m_transport.Send(requestMaxValueChanges);
        }
    }
}

void ActorValueService::OnHealthChange(const HealthChangeEvent& acEvent) noexcept
{
    if (!m_transport.IsConnected())
        return;

    auto view = m_world.view<FormIdComponent>();

    const auto hitteeIt = std::find_if(std::begin(view), std::end(view), [id = acEvent.HitteeId, view](entt::entity entity) { return view.get<FormIdComponent>(entity).Id == id; });

    if (hitteeIt == std::end(view))
    {
        spdlog::warn("Health change event form id component not found, form id: {:X}", acEvent.HitteeId);
        return;
    }

    std::optional<uint32_t> serverIdRes = Utils::GetServerId(*hitteeIt);
    if (!serverIdRes.has_value())
    {
        spdlog::error("{}: failed to find server id", __FUNCTION__);
        return;
    }

    uint32_t serverId = serverIdRes.value();

    if (acEvent.DeltaHealth > -1.0f && acEvent.DeltaHealth < 1.0f)
    {
        m_smallHealthChanges[serverId] += acEvent.DeltaHealth;
        return;
    }

    RequestHealthChangeBroadcast requestHealthChange;
    requestHealthChange.Id = serverId;
    requestHealthChange.DeltaHealth = acEvent.DeltaHealth;
    // A brawl's opponent is essential only on the brawling player's client, its owner has to hear it
    requestHealthChange.NonLethal = IsKnockOnly(Cast<Actor>(TESForm::GetById(acEvent.HitteeId)));

    m_transport.Send(requestHealthChange);

    spdlog::debug("Sent out delta health through collection: {:X}:{:f}", serverId, acEvent.DeltaHealth);
}

void ActorValueService::RunSmallHealthUpdates() noexcept
{
    static std::chrono::steady_clock::time_point lastSendTimePoint;
    constexpr auto cDelayBetweenUpdates = 250ms;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSendTimePoint < cDelayBetweenUpdates)
        return;

    lastSendTimePoint = now;

    if (!m_smallHealthChanges.empty())
    {
        for (auto& value : m_smallHealthChanges)
        {
            RequestHealthChangeBroadcast requestHealthChange;
            requestHealthChange.Id = value.first;
            requestHealthChange.DeltaHealth = value.second;
            requestHealthChange.NonLethal = IsKnockOnly(Utils::GetByServerId<Actor>(value.first));

            m_transport.Send(requestHealthChange);

            spdlog::debug("Sent out delta health through timer, {:X}:{:f}", value.first, value.second);
        }

        m_smallHealthChanges.clear();
    }
}

void ActorValueService::RunDeathStateUpdates() noexcept
{
    static std::chrono::steady_clock::time_point lastSendTimePoint;
    constexpr auto cDelayBetweenUpdates = 250ms;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSendTimePoint < cDelayBetweenUpdates)
        return;

    lastSendTimePoint = now;

    auto localView = m_world.view<FormIdComponent, LocalComponent>();

    for (auto entity : localView)
    {
        const auto& formIdComponent = localView.get<FormIdComponent>(entity);
        Actor* const pActor = Cast<Actor>(TESForm::GetById(formIdComponent.Id));
        if (!pActor)
            continue;

        auto& localComponent = localView.get<LocalComponent>(entity);

        const bool isDead = pActor->IsDead();
        // Players go down and get up through the respawn flow
        const bool isDown = !isDead && !pActor->GetExtension()->IsPlayer() && pActor->actorState.IsBleedingOut();
        const bool wasDown = m_downActors.contains(entity);
        if (!isDown)
            m_knockedOut.erase(pActor->formID);
        if (isDead == localComponent.IsDead && isDown == wasDown)
            continue;

        const bool cDies = isDead && !localComponent.IsDead;
        localComponent.IsDead = isDead;
        if (isDown)
            m_downActors.insert(entity);
        else
            m_downActors.erase(entity);

        RequestDeathStateChange requestChange;
        requestChange.Id = localComponent.Id;
        requestChange.OwnershipEpoch = localComponent.OwnershipEpoch;
        requestChange.IsDead = isDead;
        requestChange.IsDown = isDown;
        requestChange.Health = pActor->GetActorValue(ActorValueInfo::kHealth);
        // Other clients' copies die with the same killer, OnDeath scripts like Erandur's check akKiller
        if (cDies)
            requestChange.KillerId = GetServerIdOf(m_world, TESObjectREFR::GetByHandle(pActor->killerHandle));

        spdlog::info(
            "Sending death state {}, down: {}, killer: {:X} for local actor {:X}, server id: {:X}", isDead, isDown, requestChange.KillerId, pActor->formID, localComponent.Id);

        m_transport.Send(requestChange);
    }
}

void ActorValueService::RunBrawlRepairs() noexcept
{
    static std::chrono::steady_clock::time_point lastRunTimePoint;
    constexpr auto cDelayBetweenRuns = 1000ms;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastRunTimePoint < cDelayBetweenRuns || !m_transport.IsConnected())
        return;

    lastRunTimePoint = now;

    if (auto* pBrawl = Cast<TESQuest>(TESForm::GetById(CombatRules::cBrawlQuest)); pBrawl && pBrawl->IsEnabled())
    {
        const Actor* pOpponent = Cast<Actor>(pBrawl->GetAliasedRef(CombatRules::cBrawlOpponentAlias));
        if (pOpponent && CombatRules::EndsBrawl(true, pBrawl->IsStageDone(CombatRules::cBrawlEndStage), pOpponent->IsDead()))
        {
            spdlog::info("Ending the brawl against {:X}, who died in it", pOpponent->formID);
            pBrawl->SetStage(CombatRules::cBrawlEndStage);
        }
    }

    auto* pDebtJob = Cast<TESQuest>(TESForm::GetById(CombatRules::cTG01));
    if (!pDebtJob || !pDebtJob->IsEnabled())
        return;

    for (const auto& debtor : CombatRules::cTG01Debtors)
    {
        Actor* pDebtor = Cast<Actor>(pDebtJob->GetAliasedRef(debtor.AliasId));
        if (!pDebtor)
            continue;

        const uint32_t cFormId = pDebtor->formID;
        if (!pDebtor->IsDead())
        {
            m_debtorsSeenAlive.insert(cFormId);
            m_deadDebtors.erase(cFormId);
            continue;
        }

        auto deadIt = m_deadDebtors.find(cFormId);
        if (deadIt == m_deadDebtors.end())
        {
            // Killed by a player while this client watched, its own or another client's copy of one. The debtor's
            // killed-by-player stage was set on that player's client, which the quest sync brings here.
            Actor* pKiller = Cast<Actor>(TESObjectREFR::GetByHandle(pDebtor->killerHandle));
            const bool cByPlayer = m_debtorsSeenAlive.contains(cFormId) && pKiller && pKiller->GetExtension()->IsPlayer();
            deadIt = m_deadDebtors.insert({cFormId, DeadDebtor{now, cByPlayer}}).first;
        }

        // Only the client running the actor brings it back for everyone, the others' copies follow
        if (!Utils::GetLocalOwnershipToken(cFormId))
            continue;

        const double cDeadSeconds = std::chrono::duration<double>(now - deadIt->second.Since).count();
        if (CombatRules::RevivesDebtor(
                true, true, pDebtJob->IsStageDone(CombatRules::cTG01JobStage), pDebtJob->IsStageDone(debtor.KilledByPlayerStage), cDeadSeconds, deadIt->second.KilledByPlayer))
        {
            spdlog::info("Bringing back TG01 debtor {:X} from alias {}, dead for {} s without a player's kill", cFormId, debtor.AliasId, cDeadSeconds);
            m_deadDebtors.erase(cFormId);
            pDebtor->Revive();
        }
    }
}

void ActorValueService::RunActorValuesUpdates() noexcept
{
    static std::chrono::steady_clock::time_point lastSendTimePoint;
    constexpr auto cDelayBetweenUpdates = 1000ms;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSendTimePoint < cDelayBetweenUpdates)
        return;

    lastSendTimePoint = now;

    BroadcastActorValues();
}

void ActorValueService::OnHealthChangeBroadcast(const NotifyHealthChangeBroadcast& acMessage) noexcept
{
    Actor* pActor = Utils::GetByServerId<Actor>(acMessage.Id);
    if (!pActor)
    {
        spdlog::error("{}: could not find actor server id {:X}", __FUNCTION__, acMessage.Id);
        return;
    }

    // Only the owner's actor dies or goes down of damage, its state then reaches everyone (CombatRules)
    const ActorExtension* pExtension = pActor->GetExtension();
    const bool cIsDown = pActor->actorState.IsBleedingOut();
    if (!cIsDown)
        m_knockedOut.erase(pActor->formID);

    if (CombatRules::IgnoresRelayedBlow(acMessage.DeltaHealth, m_knockedOut.contains(pActor->formID), cIsDown))
    {
        spdlog::debug("Left a blow of {} out of knocked out actor {:X}", acMessage.DeltaHealth, pActor->formID);
        return;
    }

    const float cNewHealth = pActor->GetActorValue(ActorValueInfo::kHealth) + acMessage.DeltaHealth;
    const auto cBlow = CombatRules::ResolveRelayedBlow(
        acMessage.DeltaHealth, cNewHealth, pExtension->IsLocal(), pExtension->IsPlayer(), pActor->IsDead() || cIsDown, acMessage.NonLethal, IsKnockOnly(pActor));

    // Down before the health drops: at zero health the game queues a death the knock down couldn't take back
    if (cBlow == CombatRules::Blow::KnockDown)
    {
        spdlog::info("Knocking down actor {:X} instead of killing it, essential in the hitter's game: {}", pActor->formID, acMessage.NonLethal);
        pActor->KnockDown();
        if (pActor->actorState.IsBleedingOut())
            m_knockedOut.insert(pActor->formID);
    }

    pActor->ForceActorValue(ActorValueOwner::ForceMode::DAMAGE, ActorValueInfo::kHealth, cNewHealth);

    if (cBlow == CombatRules::Blow::Kill && !pActor->IsDead())
        pActor->KillByPlayer(GetActorByServerId(acMessage.HitterId));

    // TODO(cosideci): find fix for player health sync so this can be used again
    /*
    if (pActor->GetExtension()->IsRemotePlayer())
        World::Get().GetOverlayService().SetPlayerHealthPercentage(pActor->formID);
    */
}

void ActorValueService::OnActorValueChanges(const NotifyActorValueChanges& acMessage) const noexcept
{
    auto view = m_world.view<FormIdComponent, RemoteComponent>();

    const auto itor = std::find_if(std::begin(view), std::end(view), [&acMessage, view](entt::entity entity)
    {
        const auto& remote = view.get<RemoteComponent>(entity);
        return remote.Id == acMessage.Id && acMessage.OwnershipEpoch != 0 && remote.OwnershipEpoch == acMessage.OwnershipEpoch;
    });

    if (itor == std::end(view))
        return;

    auto& formIdComponent = view.get<FormIdComponent>(*itor);
    Actor* const pActor = Cast<Actor>(TESForm::GetById(formIdComponent.Id));

    if (!pActor)
        return;

    for (const auto& [key, value] : acMessage.Values)
    {
        // Syncing dragon souls triggers "Dragon soul collected" event
        if (key == ActorValueInfo::kDragonSouls || key == ActorValueInfo::kHealth)
            continue;

        spdlog::debug("Actor value update, server ID: {:X}, key: {}, value: {}", acMessage.Id, key, value);

        if (key == ActorValueInfo::kStamina || key == ActorValueInfo::kMagicka || key == ActorValueInfo::kHealth)
        {
            pActor->ForceActorValue(ActorValueOwner::ForceMode::DAMAGE, key, value);
            continue;
        }
        pActor->SetActorValue(key, value);
    }
}

void ActorValueService::OnActorMaxValueChanges(const NotifyActorMaxValueChanges& acMessage) const noexcept
{
    auto view = m_world.view<FormIdComponent, RemoteComponent>();

    const auto it = std::find_if(std::begin(view), std::end(view), [&acMessage, view](entt::entity entity)
    {
        const auto& remote = view.get<RemoteComponent>(entity);
        return remote.Id == acMessage.Id && acMessage.OwnershipEpoch != 0 && remote.OwnershipEpoch == acMessage.OwnershipEpoch;
    });

    if (it == std::end(view))
        return;

    auto& formIdComponent = view.get<FormIdComponent>(*it);
    Actor* pActor = Cast<Actor>(TESForm::GetById(formIdComponent.Id));

    if (!pActor)
        return;

    for (const auto& [key, value] : acMessage.Values)
    {
        if (key == ActorValueInfo::kDragonSouls)
            continue;

        spdlog::debug("Actor max value update, server ID: {:X}, key: {}, value: {}", acMessage.Id, key, value);

        pActor->ForceActorValue(ActorValueOwner::ForceMode::PERMANENT, key, value);
    }
}

void ActorValueService::OnDeathStateChange(const NotifyDeathStateChange& acMessage) const noexcept
{
    auto view = m_world.view<RemoteComponent>();

    const auto it = std::find_if(std::begin(view), std::end(view), [&acMessage, view](entt::entity entity)
    {
        const auto& remote = view.get<RemoteComponent>(entity);
        return remote.Id == acMessage.Id && acMessage.OwnershipEpoch != 0 && remote.OwnershipEpoch == acMessage.OwnershipEpoch;
    });

    if (it == std::end(view))
    {
        if (!acMessage.IsDead && ReviveOwnActor(acMessage))
            return;

        const auto cEntity = Utils::FindEntityByServerId(acMessage.Id);
        spdlog::info(
            "Dropped death state {} for server id {:X} at epoch {}, entity found: {}, has form id: {}, waiting for 3D: {}", acMessage.IsDead, acMessage.Id, acMessage.OwnershipEpoch,
            cEntity.has_value(), cEntity && m_world.all_of<FormIdComponent>(*cEntity), cEntity && m_world.all_of<WaitingFor3D>(*cEntity));
        return;
    }

    // A copy that is still loading gets this state once its 3D is there. Missing it, the copy came back alive.
    if (auto* pWaitingFor3D = m_world.try_get<WaitingFor3D>(*it))
        pWaitingFor3D->SpawnRequest.IsDead = acMessage.IsDead;

    const auto* pFormIdComponent = m_world.try_get<FormIdComponent>(*it);
    Actor* pActor = pFormIdComponent ? Cast<Actor>(TESForm::GetById(pFormIdComponent->Id)) : nullptr;

    if (!pActor)
        return;

    ActorExtension* pExtension = pActor->GetExtension();
    // Players should never be killed
    if (pExtension->IsPlayer())
        return;

    const auto cFollow = CombatRules::FollowOwner(pActor->IsDead(), pActor->actorState.IsBleedingOut(), acMessage.IsDead, acMessage.IsDown);
    if (cFollow != CombatRules::Follow::None)
        spdlog::info("Following the owner of remote actor {:X}, server id: {:X}, dead: {}, down: {}, step: {}", pActor->formID, acMessage.Id, acMessage.IsDead, acMessage.IsDown, static_cast<int>(cFollow));

    switch (cFollow)
    {
    case CombatRules::Follow::Kill:
        // Forced: a remote actor refuses its own death action, which left the body standing. The killer the server passed on is
        // this client's player when it dealt the blow.
        m_world.GetCharacterService().KillWhenReady(pActor, GetActorByServerId(acMessage.KillerId));
        break;
    case CombatRules::Follow::Revive: pActor->Revive(); break;
    case CombatRules::Follow::KnockDown:
        // Bleedout is brief, a copy that can't play it right now just misses it
        if (pActor->GetNiNode() && pActor->animationGraphHolder.IsReady())
            pActor->KnockDown();
        break;
    case CombatRules::Follow::StandUp: pActor->StandUp(acMessage.Health); break;
    case CombatRules::Follow::None: break;
    }

    // A replayed voice can end before its subtitle packet arrives. Death must also
    // clear an already-visible line even when health replication killed the actor first.
    if (acMessage.IsDead)
        if (auto* pSubtitles = SubtitleManager::Get())
            pSubtitles->HideSubtitle(pActor);
}

bool ActorValueService::ReviveOwnActor(const NotifyDeathStateChange& acMessage) const noexcept
{
    // The server never sends an owner its own reports, a death state for one of its actors is a party member's revival
    auto view = m_world.view<FormIdComponent, LocalComponent>();
    for (auto entity : view)
    {
        const auto& localComponent = view.get<LocalComponent>(entity);
        if (localComponent.Id != acMessage.Id || localComponent.OwnershipEpoch != acMessage.OwnershipEpoch)
            continue;

        Actor* pActor = Cast<Actor>(TESForm::GetById(view.get<FormIdComponent>(entity).Id));
        if (!pActor || !pActor->IsDead() || pActor->GetExtension()->IsPlayer())
            return false;

        spdlog::info("Bringing back local actor {:X}, server id {:X}, a party member revived it", pActor->formID, acMessage.Id);
        // Where it lies and as it was, Respawn's Reset would also send it back to its editor location
        pActor->Revive();
        return true;
    }

    return false;
}

void ActorValueService::OnLocalRevival(const uint32_t aFormId) noexcept
{
    if (!m_transport.IsConnected())
        return;

    const auto cToken = Utils::GetRemoteOwnershipToken(aFormId);
    Actor* pActor = Cast<Actor>(TESForm::GetById(aFormId));
    if (!cToken || !pActor || pActor->IsDead())
        return;

    spdlog::info("Asking for actor {:X}, server id {:X}, to come back for everyone, it was revived here", aFormId, cToken->ServerId);

    // The server accepts it from a party member of the owner standing near the actor, see DeathMemory::AcceptsChange
    RequestDeathStateChange request;
    request.Id = cToken->ServerId;
    request.OwnershipEpoch = cToken->OwnershipEpoch;
    request.IsDead = false;
    request.Health = pActor->GetActorValue(ActorValueInfo::kHealth);

    m_transport.Send(request);
}
