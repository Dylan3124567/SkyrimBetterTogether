#pragma once
#include <World.h>
#include <atomic>

struct UpdateEvent;
struct DisconnectedEvent;
struct PartyLeftEvent;
struct NotifyHelgenState;
struct Actor;
struct BGSScene;

struct HelgenService
{
    HelgenService(World&, entt::dispatcher&);
    static bool HoldScene(uint32_t aFormId) noexcept { return aFormId == 0xCD68E && s_creationSceneHeld.load(); }
    uint32_t GetSceneHostId() const noexcept { return m_sceneHostId.load(); }
    bool PrepareConnection() noexcept;
    bool PrepareSceneUpdate(BGSScene*) const noexcept;
    void Cancel() noexcept;
    // NPCs stay local while the introductory scenes use their quest aliases.
    bool DeferActors() const noexcept { return m_active && m_phase < 2; }
    bool StagePlayer(Actor*, uint32_t aPlayerId) const noexcept;
    bool CreatingCharacters() const noexcept { return m_active && m_phase == 0; }
    bool LocalQuest(uint32_t aFormId) const noexcept { return m_active && m_phase < 2 && (aFormId == 0x3372B || aFormId == 0xD0593); }
    bool DeferBridgeCollapse() const noexcept;
    bool DeferTowerEntry() const noexcept;
    bool DeferExteriorBarrier(uint16_t aStage) const noexcept;
    bool DeferKeepEntry() const noexcept;

private:
    void OnUpdate(const UpdateEvent&) noexcept;
    void OnDisconnected(const DisconnectedEvent&) noexcept;
    void OnPartyLeft(const PartyLeftEvent&) noexcept;
    void OnState(const NotifyHelgenState&) noexcept;
    void Pause(bool aPause) noexcept;
    void Status(const String& acText) noexcept;
    void RefreshActors() noexcept;
    bool HoldCreationScene() noexcept;
    void ReleaseCreationScene() noexcept;
    inline static std::atomic<bool> s_creationSceneHeld{};
    uint32_t m_creationMenuFlags{};
    std::atomic<uint32_t> m_leaderScenePhases{0xFFFEFFFE};
    uint32_t m_sentScenePhases{0xFFFEFFFE};
    World& m_world;
    bool m_active{};
    bool m_paused{};
    bool m_registered{};
    bool m_ready{};
    bool m_finished{};
    bool m_leaderFinished{};
    bool m_leaderAtBlock{};
    bool m_sentAtBlock{};
    bool m_towerBreachSynced{};
    std::atomic<uint32_t> m_sceneHostId{};
    uint8_t m_phase{};
    uint8_t m_reported{};
    uint32_t m_session{};
    uint32_t m_readyCount{};
    uint32_t m_totalCount{};
    String m_status;
    entt::scoped_connection m_updateConnection, m_disconnectConnection, m_partyLeftConnection, m_stateConnection;
};
