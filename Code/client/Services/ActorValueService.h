#pragma once

#include <Events/EventDispatcher.h>
#include <Games/Events.h>

struct World;

struct ConnectedEvent;
struct DisconnectedEvent;
struct ActorRemovedEvent;
struct UpdateEvent;
struct HealthChangeEvent;

struct TransportService;
struct NotifyActorValueChanges;
struct NotifyActorMaxValueChanges;
struct NotifyHealthChangeBroadcast;
struct NotifyDeathStateChange;

struct Actor;

/**
 * @brief Broadcast and apply actor values and death state.
 *
 * Responsible for periodically checking whether actor values of local actors have changed since
 * the previous frame, and if so, broadcast those values to other players within range.
 * This service is also responsible for applying incoming actor value changes.
 * Health is synced separately from other actor values, since health changes
 * need to be synced immediately.
 *
 * This service is also responsible for syncing the death states of individual actors.
 * The death state is perdiocally checked and compared to cached death states.
 */
struct ActorValueService
{
public:
    ActorValueService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~ActorValueService() noexcept = default;

    TP_NOCOPYMOVE(ActorValueService);

    /**
     * @brief Asks for an actor this client only has a copy of to come back for everyone, after something here revived it.
     */
    void OnLocalRevival(uint32_t aFormId) noexcept;

private:
    enum ValueType : uint8_t
    {
        kValue,
        kMaxValue
    };

    /**
     * @brief Creates actor values components for new local components.
     */
    void OnLocalComponentAdded(entt::registry& aRegistry, entt::entity aEntity) noexcept;
    void OnLocalComponentRemoved(entt::registry& aRegistry, entt::entity aEntity) noexcept;
    /**
     * @brief Clears actor values components on disconnect.
     */
    void OnDisconnected(const DisconnectedEvent&) noexcept;
    /**
     * @brief Removes actor values component on reference removed.
     */
    void OnActorRemoved(const ActorRemovedEvent&) noexcept;
    /**
     * @brief Checks whether health, death state and actor values should be broadcasted.
     */
    void OnUpdate(const UpdateEvent&) noexcept;
    /**
     * @brief Receives remote actor values changes and applies them locally.
     */
    void OnActorValueChanges(const NotifyActorValueChanges& acMessage) const noexcept;
    /**
     * @brief Receives remote max actor values changes and applies them locally.
     */
    void OnActorMaxValueChanges(const NotifyActorMaxValueChanges& acMessage) const noexcept;
    /**
     * @brief Processes health value changes.
     *
     * If the health changes are small, it will be cached and sent out on update.
     */
    void OnHealthChange(const HealthChangeEvent&) noexcept;
    /**
     * @brief Receives health value changes and applies them locally.
     */
    void OnHealthChangeBroadcast(const NotifyHealthChangeBroadcast& acMessage) noexcept;
    /**
     * @brief Receives death state changes and applies them locally.
     */
    void OnDeathStateChange(const NotifyDeathStateChange& acEvent) const noexcept;

    /**
     * @brief Checks and broadcasts new actor values.
     *
     * If the current (max) actor values have changed compared to the previously cached actor values,
     * the new actor values will be broadcasted to the other clients in range.
     */
    void RunActorValuesUpdates() noexcept;
    /**
     * @brief Checks and broadcasts small health changes.
     *
     * If the small health member variable has collected any health changes, this function
     * broadcasts those changes to the other clients.
     */
    void RunSmallHealthUpdates() noexcept;
    /**
     * @brief Checks and broadcasts death and bleedout state changes.
     */
    void RunDeathStateUpdates() noexcept;
    /**
     * @brief Undoes what the co-op brawl deaths left behind, see CombatRules: debtors of TG01 and brawls with a dead opponent.
     */
    void RunBrawlRepairs() noexcept;
    /**
     * @brief Brings back an actor this client runs after a party member revived it.
     */
    bool ReviveOwnActor(const NotifyDeathStateChange& acMessage) const noexcept;

    /**
     * @brief Checks and broadcasts actor values.
     * @see RunActorValuesUpdates()
     */
    void BroadcastActorValues() noexcept;
    /**
     * @brief Creates actor values component for actor and caches the initial actor values.
     */
    void CreateActorValuesComponent(entt::entity aEntity, Actor* apActor) noexcept;

    World& m_world;
    entt::dispatcher& m_dispatcher;
    TransportService& m_transport;

    //! @brief Server ids and collected health changes.
    Map<uint32_t, float> m_smallHealthChanges;
    //! @brief Local actors last reported as down (bleeding out).
    Set<entt::entity> m_downActors;
    //! @brief Form ids of local actors down from a relayed blow that only knocks down, see CombatRules::IgnoresRelayedBlow.
    Set<uint32_t> m_knockedOut;

    struct DeadDebtor
    {
        std::chrono::steady_clock::time_point Since;
        bool KilledByPlayer{};
    };
    //! @brief TG01 debtors seen dead, by form id, see CombatRules::RevivesDebtor.
    Map<uint32_t, DeadDebtor> m_deadDebtors;
    //! @brief TG01 debtors seen alive this session, a death after that has its killer recorded in this session.
    Set<uint32_t> m_debtorsSeenAlive;
};
