#include <Components.h>
#include <Messages/RequestActorValueChanges.h>
#include <Messages/RequestActorMaxValueChanges.h>
#include <Messages/RequestHealthChangeBroadcast.h>
#include <Messages/RequestDeathStateChange.h>
#include <Services/ActorValueService.h>
#include <World.h>
#include <GameServer.h>
#include <Messages/NotifyActorValueChanges.h>
#include <Messages/NotifyActorMaxValueChanges.h>
#include <Messages/NotifyHealthChangeBroadcast.h>
#include <Messages/NotifyDeathStateChange.h>
#include <Structs/DeathMemory.h>

ActorValueService::ActorValueService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
{
    m_updateHealthConnection = aDispatcher.sink<PacketEvent<RequestActorValueChanges>>().connect<&ActorValueService::OnActorValueChanges>(this);
    m_updateMaxValueConnection = aDispatcher.sink<PacketEvent<RequestActorMaxValueChanges>>().connect<&ActorValueService::OnActorMaxValueChanges>(this);
    m_updateDeltaHealthConnection = aDispatcher.sink<PacketEvent<RequestHealthChangeBroadcast>>().connect<&ActorValueService::OnHealthChangeBroadcast>(this);
    m_deathStateConnection = aDispatcher.sink<PacketEvent<RequestDeathStateChange>>().connect<&ActorValueService::OnDeathStateChange>(this);
    m_characterDestroyedConnection = m_world.on_destroy<CharacterComponent>().connect<&ActorValueService::OnCharacterDestroyed>(this);
}

// Hitter and killer ids carry entity integers (World::ToInteger), "no actor" has to be no entity
static_assert(entt::to_integral(static_cast<entt::entity>(entt::null)) == DeathMemory::cNoActor);

bool ActorValueService::TakeWitness(const GameId& acActorId, const Player& acPlayer) noexcept
{
    const auto it = m_deathWitnesses.find(acActorId);
    if (it == m_deathWitnesses.end())
        return false;

    const auto& players = it->second.Players;
    const bool cSawDeath = std::find(players.begin(), players.end(), DeathMemory::WitnessId(acPlayer.GetDiscordId(), acPlayer.GetId())) != players.end();
    m_deathWitnesses.erase(it);
    return cSawDeath;
}

void ActorValueService::OnCharacterDestroyed(entt::registry& aRegistry, const entt::entity aEntity) noexcept
{
    // Entity ids are reused, a new actor must not inherit the last one's hitter
    m_lastHitters.erase(aEntity);
}

void ActorValueService::OnActorValueChanges(const PacketEvent<RequestActorValueChanges>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    auto actorValuesView = m_world.view<ActorValuesComponent, OwnerComponent>();

    auto it = actorValuesView.find(static_cast<entt::entity>(message.Id));

    if (it == actorValuesView.end() || !actorValuesView.get<OwnerComponent>(*it).IsCurrentOwner(acMessage.pPlayer, message.OwnershipEpoch))
        return;

    auto& actorValuesComponent = actorValuesView.get<ActorValuesComponent>(*it);
    for (auto& [id, value] : message.Values)
    {
        actorValuesComponent.CurrentActorValues.ActorValuesList[id] = value;
    }

    NotifyActorValueChanges notify;
    notify.OwnershipEpoch = message.OwnershipEpoch;
    notify.Id = acMessage.Packet.Id;
    notify.Values = acMessage.Packet.Values;

    const entt::entity cEntity = static_cast<entt::entity>(message.Id);
    if (!GameServer::Get()->SendToPlayersInRange(notify, cEntity, acMessage.pPlayer))
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
}

void ActorValueService::OnActorMaxValueChanges(const PacketEvent<RequestActorMaxValueChanges>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    auto actorValuesView = m_world.view<ActorValuesComponent, OwnerComponent>();

    auto it = actorValuesView.find(static_cast<entt::entity>(message.Id));

    if (it == actorValuesView.end() || !actorValuesView.get<OwnerComponent>(*it).IsCurrentOwner(acMessage.pPlayer, message.OwnershipEpoch))
        return;

    auto& actorValuesComponent = actorValuesView.get<ActorValuesComponent>(*it);
    for (auto& [id, value] : message.Values)
    {
        actorValuesComponent.CurrentActorValues.ActorMaxValuesList[id] = value;
    }

    NotifyActorMaxValueChanges notify;
    notify.OwnershipEpoch = message.OwnershipEpoch;
    notify.Id = message.Id;
    notify.Values = message.Values;

    const entt::entity cEntity = static_cast<entt::entity>(message.Id);
    if (!GameServer::Get()->SendToPlayersInRange(notify, cEntity, acMessage.pPlayer))
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
}

void ActorValueService::OnHealthChangeBroadcast(const PacketEvent<RequestHealthChangeBroadcast>& acMessage) noexcept
{
    auto& message = acMessage.Packet;

    if (!std::isfinite(message.DeltaHealth))
        return;
    auto actorValuesView = m_world.view<ActorValuesComponent, OwnerComponent>();

    auto it = actorValuesView.find(static_cast<entt::entity>(message.Id));

    const auto cHitter = acMessage.pPlayer->GetCharacter();
    if (it != actorValuesView.end())
    {
        auto& actorValuesComponent = actorValuesView.get<ActorValuesComponent>(*it);
        auto currentHealth = actorValuesComponent.CurrentActorValues.ActorValuesList[24];
        actorValuesComponent.CurrentActorValues.ActorValuesList[24] = currentHealth + message.DeltaHealth;

        // Damage from a client other than the owner's is that client's player hitting the actor
        if (message.DeltaHealth < 0.f && cHitter && actorValuesView.get<OwnerComponent>(*it).GetOwner() != acMessage.pPlayer)
            m_lastHitters[*it] = World::ToInteger(*cHitter);
    }

    NotifyHealthChangeBroadcast notify;
    notify.Id = message.Id;
    notify.DeltaHealth = message.DeltaHealth;
    notify.NonLethal = message.NonLethal;
    notify.HitterId = cHitter ? World::ToInteger(*cHitter) : DeathMemory::cNoActor;

    const entt::entity cEntity = static_cast<entt::entity>(message.Id);
    if (!GameServer::Get()->SendToPlayersInRange(notify, cEntity, acMessage.pPlayer))
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
}

void ActorValueService::OnDeathStateChange(const PacketEvent<RequestDeathStateChange>& acMessage) noexcept
{
    auto& message = acMessage.Packet;
    Player* const pSender = acMessage.pPlayer;

    auto characterView = m_world.view<CharacterComponent, OwnerComponent, CellIdComponent>();

    const auto it = characterView.find(static_cast<entt::entity>(message.Id));
    if (it == characterView.end())
        return;

    const entt::entity cEntity = *it;
    auto& characterComponent = characterView.get<CharacterComponent>(cEntity);
    const auto& ownerComponent = characterView.get<OwnerComponent>(cEntity);
    const auto& cellIdComponent = characterView.get<CellIdComponent>(cEntity);
    const Player* pOwner = ownerComponent.GetOwner();

    const bool cFromOwner = ownerComponent.IsCurrentOwner(pSender, message.OwnershipEpoch);
    const bool cEpochMatches = message.OwnershipEpoch != 0 && message.OwnershipEpoch == ownerComponent.OwnershipEpoch;
    const bool cInRange = pSender->GetCellComponent().IsInRange(cellIdComponent, characterComponent.IsDragon());
    const auto& senderParty = pSender->GetParty().JoinedPartyId;
    const bool cSharesParty = !pOwner || (senderParty && senderParty == pOwner->GetParty().JoinedPartyId);

    if (!DeathMemory::AcceptsChange(cFromOwner, cEpochMatches, message.IsDead, characterComponent.IsDead(), characterComponent.IsPlayer(), cInRange, cSharesParty))
        return;

    const bool cDies = message.IsDead && !characterComponent.IsDead();
    const bool cComesBack = !message.IsDead && characterComponent.IsDead();
    characterComponent.SetDead(message.IsDead);
    spdlog::debug("Updating death state {:x}:{}, down: {}, from owner: {}", message.Id, message.IsDead, message.IsDown, cFromOwner);

    uint32_t killerId = DeathMemory::cNoActor;
    if (cDies && message.KillerId != DeathMemory::cNoActor && m_world.valid(static_cast<entt::entity>(message.KillerId)))
    {
        const auto cKiller = static_cast<entt::entity>(message.KillerId);
        const auto* pKillerCell = m_world.try_get<CellIdComponent>(cKiller);
        const auto* pKillerOwner = m_world.try_get<OwnerComponent>(cKiller);
        const auto hitterIt = m_lastHitters.find(cEntity);

        const bool cIsSendersPlayer = pSender->GetCharacter() == cKiller;
        const bool cOwnedBySender = pKillerOwner && pKillerOwner->GetOwner() == pSender;
        const bool cIsLastHitter = hitterIt != m_lastHitters.end() && hitterIt->second == message.KillerId;
        const bool cKillerInRange = m_world.all_of<CharacterComponent>(cKiller) && pKillerCell && pKillerCell->IsInRange(cellIdComponent, characterComponent.IsDragon());

        if (DeathMemory::AcceptsKiller(cIsSendersPlayer, cOwnedBySender, cIsLastHitter, cKillerInRange))
            killerId = message.KillerId;
        else
            spdlog::warn("Player {:X} named {:X} as the killer of {:X}, which it may not", pSender->GetId(), message.KillerId, message.Id);
    }

    if (cDies || cComesBack)
        m_lastHitters.erase(cEntity);

    // Who had the death in their game, for when the actor comes back with one of them, see DeathMemory::KeepsDeath
    if (const auto* pFormIdComponent = m_world.try_get<FormIdComponent>(cEntity))
    {
        if (cDies)
        {
            const auto [day, month, year] = m_world.GetCalendarService().GetDate();
            const int64_t cNow = DeathMemory::GameHour(year, month, day, m_world.GetCalendarService().GetTime().first);
            m_deathWitnesses.erase(pFormIdComponent->Id);
            DeathMemory::PruneWitnesses(m_deathWitnesses, cNow);

            DeathWitnesses witnesses{cNow, {}};
            for (auto* pPlayer : m_world.GetPlayerManager())
            {
                if (pPlayer == pSender || cellIdComponent.IsInRange(pPlayer->GetCellComponent(), characterComponent.IsDragon()))
                    witnesses.Players.push_back(DeathMemory::WitnessId(pPlayer->GetDiscordId(), pPlayer->GetId()));
            }
            m_deathWitnesses[pFormIdComponent->Id] = std::move(witnesses);
        }
        else if (cComesBack)
            m_deathWitnesses.erase(pFormIdComponent->Id);
    }

    NotifyDeathStateChange notify;
    notify.OwnershipEpoch = ownerComponent.OwnershipEpoch;
    notify.Id = message.Id;
    notify.IsDead = message.IsDead;
    notify.IsDown = message.IsDown && !message.IsDead;
    notify.Health = std::isfinite(message.Health) ? message.Health : 0.f;
    notify.KillerId = killerId;

    // A revival from a party member goes to the owner as well, whose actor is still dead
    if (!GameServer::Get()->SendToPlayersInRange(notify, cEntity, pSender))
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
}
