#pragma once

#include <Events/PacketEvent.h>
#include <Structs/GameId.h>

struct World;
struct Player;
struct UpdateEvent;
struct TransportService;
struct RequestActorValueChanges;
struct RequestActorMaxValueChanges;
struct RequestHealthChangeBroadcast;
struct RequestDeathStateChange;

/**
 * @brief Broadcasts changes in (max) actor values and updates them server side.
 */
struct ActorValueService
{
    ActorValueService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;
    ~ActorValueService() noexcept = default;

    TP_NOCOPYMOVE(ActorValueService);

    /**
     * @brief Whether the player was around when the actor last died, see DeathMemory::KeepsDeath. The record goes with the
     * death record it is asked for.
     */
    [[nodiscard]] bool TakeWitness(const GameId& acActorId, const Player& acPlayer) noexcept;

private:
    World& m_world;

    void OnActorValueChanges(const PacketEvent<RequestActorValueChanges>& acMessage) const noexcept;
    void OnActorMaxValueChanges(const PacketEvent<RequestActorMaxValueChanges>& acMessage) const noexcept;
    void OnHealthChangeBroadcast(const PacketEvent<RequestHealthChangeBroadcast>& acMessage) noexcept;
    void OnDeathStateChange(const PacketEvent<RequestDeathStateChange>& acMessage) noexcept;
    void OnCharacterDestroyed(entt::registry& aRegistry, entt::entity aEntity) noexcept;

    entt::scoped_connection m_updateHealthConnection;
    entt::scoped_connection m_updateMaxValueConnection;
    entt::scoped_connection m_updateDeltaHealthConnection;
    entt::scoped_connection m_deathStateConnection;
    entt::scoped_connection m_characterDestroyedConnection;

    struct DeathWitnesses
    {
        int64_t Hour{};
        // DeathMemory::WitnessId of the players around
        Vector<uint64_t> Players;
    };

    // The players around each actor when it last died, see DeathMemory::PruneWitnesses
    TiltedPhoques::Map<GameId, DeathWitnesses> m_deathWitnesses;
    // Server id of the player character that last hurt each actor from a client other than its owner's
    TiltedPhoques::Map<entt::entity, uint32_t> m_lastHitters;
};
