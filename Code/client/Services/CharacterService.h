#pragma once
#include "Structs/Inventory.h"
#include "Structs/ActorData.h"
#include <Messages/PlayerAppearance.h>
#include <Systems/CaravanRules.h>
#include <Systems/SyncRules.h>

struct ActorAddedEvent;
struct ActorRemovedEvent;
struct UpdateEvent;
struct ConnectedEvent;
struct DisconnectedEvent;
struct EquipmentChangeEvent;
struct FormIdComponent;
struct ActionEvent;
struct AssignCharacterResponse;
struct CharacterSpawnRequest;
struct ServerReferencesMoveRequest;
struct NotifyActionEvents;
struct NotifyInventoryChanges;
struct NotifyFactionsChanges;
struct NotifyRemoveCharacter;
struct NotifyOwnershipTransfer;
struct SpellCastEvent;
struct NotifySpellCast;
struct InterruptCastEvent;
struct NotifyInterruptCast;
struct AddTargetEvent;
struct NotifyAddTarget;
struct ProjectileLaunchedEvent;
struct NotifyProjectileLaunch;
struct MountEvent;
struct NotifyMount;
struct InitPackageEvent;
struct NotifyNewPackage;
struct NotifyRespawn;
struct BeastFormChangeEvent;
struct AddExperienceEvent;
struct NotifySyncExperience;
struct DialogueEvent;
struct NotifyDialogue;
struct SubtitleEvent;
struct NotifySubtitle;
struct NotifyActorTeleport;
struct PartyJoinedEvent;

struct Actor;
struct TESNPC;
struct World;
struct TransportService;

/**
 * @brief Handles actors and players.
 */
struct CharacterService
{
    CharacterService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~CharacterService() noexcept = default;

    TP_NOCOPYMOVE(CharacterService);

    static void DeleteTempActor(const uint32_t aFormId) noexcept;

    // @param aForDialogue This client's player is talking to the actor, see RequestOwnershipClaim::ForDialogue
    bool RequestOwnership(uint32_t aFormId, uint32_t aServerId, entt::entity aEntity, bool aForDialogue = false) const noexcept;

    void OnActorAdded(const ActorAddedEvent& acEvent) noexcept;
    void OnActorRemoved(const ActorRemovedEvent& acEvent) noexcept;
    void OnUpdate(const UpdateEvent& acUpdateEvent) noexcept;
    void OnConnected(const ConnectedEvent& acConnectedEvent) const noexcept;
    void OnDisconnected(const DisconnectedEvent& acDisconnectedEvent) const noexcept;
    void OnAssignCharacter(const AssignCharacterResponse& acMessage) noexcept;
    void OnCharacterSpawn(const CharacterSpawnRequest& acMessage) const noexcept;
    void OnReferencesMoveRequest(const ServerReferencesMoveRequest& acMessage) const noexcept;
    void OnNotifyActionEvents(const NotifyActionEvents& acMessage) const noexcept;
    void OnActionEvent(const ActionEvent& acActionEvent) const noexcept;
    void OnFactionsChanges(const NotifyFactionsChanges& acEvent) const noexcept;
    void OnOwnershipTransfer(const NotifyOwnershipTransfer& acMessage) noexcept;
    void OnRemoveCharacter(const NotifyRemoveCharacter& acMessage) const noexcept;
    void OnMountEvent(const MountEvent& acEvent) const noexcept;
    void OnNotifyMount(const NotifyMount& acMessage) const noexcept;
    void OnInitPackageEvent(const InitPackageEvent& acEvent) const noexcept;
    void OnNotifyNewPackage(const NotifyNewPackage& acMessage) const noexcept;
    void OnNotifyRespawn(const NotifyRespawn& acMessage) const noexcept;
    void OnPlayerAppearance(const NotifyPlayerAppearance& acMessage) const noexcept;
    void OnBeastFormChange(const BeastFormChangeEvent& acEvent) const noexcept;
    void OnAddExperienceEvent(const AddExperienceEvent& acEvent) noexcept;
    void OnNotifySyncExperience(const NotifySyncExperience& acMessage) noexcept;
    void OnDialogueEvent(const DialogueEvent& acEvent) noexcept;
    void OnNotifyDialogue(const NotifyDialogue& acMessage) noexcept;
    void OnSubtitleEvent(const SubtitleEvent& acEvent) noexcept;
    void OnNotifySubtitle(const NotifySubtitle& acMessage) noexcept;
    void OnNotifyActorTeleport(const NotifyActorTeleport& acMessage) noexcept;
    void OnPartyJoinedEvent(const PartyJoinedEvent& acEvent) noexcept;

    void RunAppearanceUpdates(bool aForce = false) noexcept;
    void ProcessNewEntity(entt::entity aEntity) const noexcept;

    bool MoveActor(const Actor* apActor, const GameId& acWorldSpaceId, const GameId& acCellId, const Vector3_NetQuantize& acPosition) const noexcept;
    // Forces the death action, waiting until the actor can play it. The killer is who OnDeath scripts get as akKiller.
    void KillWhenReady(Actor* apActor, Actor* apKiller = nullptr) const noexcept;

private:

    void RequestServerAssignment(entt::entity aEntity) const noexcept;
    void CancelServerAssignment(entt::entity aEntity, uint32_t aFormId) const noexcept;
    void DeleteRemoteEntityComponents(entt::entity aEntity) const noexcept;
    // Makes the actor showing a remote one this client has no game form for, after deleting any earlier one for it
    Actor* CreateRemoteCopy(uint32_t aServerId, TESNPC* apNpc) const noexcept;
    // Stops showing a remote actor: the copy made for it is deleted, a game actor goes back to the local game
    void ReleaseRemoteActor(entt::entity aEntity) const noexcept;
    // Makes an already spawned actor fit a new spawn of it, see the definition
    void RefreshSpawnedActor(const CharacterSpawnRequest& acMessage) const noexcept;
    void DeclineOwnership(uint32_t aServerId, uint32_t aOwnershipEpoch) const noexcept;
    void ReconcileActorData(entt::entity aEntity, Actor* apActor, uint32_t aOwnershipEpoch, const ActorData& acActorData, bool aApplyInventory, bool aIsLocalOwner) noexcept;

    Actor* CreateCharacterForEntity(entt::entity aEntity) const noexcept;
    ActorData BuildActorData(Actor* apActor) const noexcept;
    void ApplyLeveledNpcPick(Actor* apActor, const GameId& acPickId) const noexcept;
    void ProcessLeveledConforms() noexcept;
    void ProcessPendingDeaths() noexcept;
    void ProcessAppearanceUpdates() noexcept;

    void RunLocalUpdates() const noexcept;
    void RunRemoteUpdates() noexcept;
    void RunThoraldUpdates() const noexcept;
    void RunSceneUpdates() noexcept;
    void RunFactionsUpdates() const noexcept;
    void RunSpawnUpdates() const noexcept;
    void RunCopyCleanup() noexcept;
    void RunCaravanUpdates() noexcept;
    void RunExperienceUpdates() noexcept;
    void ApplyCachedWeaponDraws(const UpdateEvent& acUpdateEvent) noexcept;

    World& m_world;
    entt::dispatcher& m_dispatcher;
    TransportService& m_transport;

    float m_cachedExperience = 0.f;
    mutable PlayerAppearance m_lastAppearance;
    double m_nextAppearance{};
    struct PendingAppearance
    {
        PlayerAppearance Latest;
        std::optional<PlayerAppearance> Applied;
        bool Disabling = false;
    };
    mutable Map<uint32_t, PendingAppearance> m_pendingAppearances;

    // The game actors this client made for other clients' actors, by server id
    mutable SyncRules::RemoteCopies m_remoteCopies{};
    // Copies this client was given to run. Still copies: let go, they go, or they came back as new actors of this client's.
    mutable Set<uint32_t> m_ownedCopies{};
    double m_nextCopyCleanup{};

    // Where each of CaravanRules::kCaravans has been seen camped by its leader copy
    std::array<CaravanRules::CampTracker, CaravanRules::kCaravans.size()> m_caravanCamps{};
    double m_nextCaravanUpdate{};

    // TODO: revamp this, read the local anim var like vampire lord?
    struct WeaponDrawData
    {
        WeaponDrawData() = default;
        WeaponDrawData(bool aDrawWeapon)
            : m_drawWeapon(aDrawWeapon)
        {
        }

        double m_timer = 0.0;
        bool m_drawWeapon = false;
        bool m_isFirstPass = true;
    };

    Map<uint32_t, WeaponDrawData> m_weaponDrawUpdates{};

    // Actor form ID -> pick form ID. The active stage lives in ActorExtension.
    // Written from const message handlers, drained by ProcessLeveledConforms.
    mutable Map<uint32_t, uint32_t> m_pendingLeveledConforms{};

    // Actors that died before this client got there, killed once their behavior graph can take the death action
    mutable Set<uint32_t> m_pendingDeaths{};

    struct VoiceLine
    {
        String File;
        double Time{};
    };
    mutable Map<uint32_t, VoiceLine> m_lastVoiceLines{};

    struct SceneHold
    {
        double Start{-1.0};
        uint64_t ReleasedPhase{};
    };
    mutable Map<uint32_t, SceneHold> m_sceneHolds{};

    entt::scoped_connection m_referenceAddedConnection;
    entt::scoped_connection m_referenceRemovedConnection;
    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_actionConnection;
    entt::scoped_connection m_factionsConnection;
    entt::scoped_connection m_ownershipTransferConnection;
    entt::scoped_connection m_removeCharacterConnection;
    entt::scoped_connection m_connectedConnection;
    entt::scoped_connection m_disconnectedConnection;
    entt::scoped_connection m_assignCharacterConnection;
    entt::scoped_connection m_characterSpawnConnection;
    entt::scoped_connection m_referenceMovementSnapshotConnection;
    entt::scoped_connection m_actionEventsConnection;
    entt::scoped_connection m_mountConnection;
    entt::scoped_connection m_notifyMountConnection;
    entt::scoped_connection m_initPackageConnection;
    entt::scoped_connection m_newPackageConnection;
    entt::scoped_connection m_notifyRespawnConnection;
    entt::scoped_connection m_playerAppearanceConnection;
    entt::scoped_connection m_beastFormChangeConnection;
    entt::scoped_connection m_addExperienceEventConnection;
    entt::scoped_connection m_syncExperienceConnection;
    entt::scoped_connection m_dialogueEventConnection;
    entt::scoped_connection m_dialogueSyncConnection;
    entt::scoped_connection m_subtitleEventConnection;
    entt::scoped_connection m_subtitleSyncConnection;
    entt::scoped_connection m_actorTeleportConnection;
    entt::scoped_connection m_partyJoinedConnection;
};
