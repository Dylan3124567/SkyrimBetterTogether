#pragma once

#include <World.h>
#include <Events/EventDispatcher.h>
#include <Games/Events.h>
#include <atomic>
#include <deque>
#include <Messages/NotifyQuestUpdate.h>
#include <Structs/QuestRecovery.h>
#include <Structs/QuestSharing.h>
#include <Systems/QuestStall.h>

struct DisconnectedEvent;
struct PartyLeftEvent;
struct PartyJoinedEvent;
struct UpdateEvent;

struct TESQuest;

/**
 * @brief Syncs quest starts, stops and stages across the party
 *
 * Every client runs the party's quests itself (scripts, scenes, dialogue), and only starts, stops and stages are
 * shared. Anything else a quest holds (aliases, script properties) only matches when the stage fragments produce it.
 *
 * Sending: TESQuestStartStopEvent and TESQuestStageEvent go out as RequestQuestUpdate, only in a party (outside one, an
 * update may prompt the overlay to suggest one). Quests without stages or in kNonSyncableQuestIds are never sent, and
 * the server drops None/Miscellaneous ones unless bEnableMiscQuestSync is set.
 *
 * Relaying: the server sends updates to the rest of the party as NotifyQuestUpdate. Members' scripts often reach the
 * same stage on their own, so it relays each once, from whoever reaches it first, until the quest next starts or stops.
 *
 * Applying: stages go through ScriptSetStage so their fragments run (and a stopped quest restarts), skipping those
 * already reached. ScopedQuestOverride keeps the events this fires from being sent back.
 *
 * Stops: a stop is sent with how the quest ended (its Completed and Failed flags), and only for a quest running here.
 * It waits for this client's fragments of the member stages applied before it, as the member's quest only stopped
 * after them: stopping first clears the aliases they use and makes the game ignore the objectives they set. A fail
 * stage's fragment usually stops the quest itself. Otherwise it's completed or failed as a log entry would, and
 * stopped as Papyrus Quest.Stop would. Stages this copy sets meanwhile belong to the ended run and aren't sent. A start
 * the story manager gave up on isn't sent either. See QuestSharing.h rule 6.
 *
 * Scenes: setting a stage a playing scene waits on can end its phase early, skipping its lines and scripts and leaving
 * the quest stuck. So the leader, whose world the party follows, holds member stages that an upcoming phase checks and
 * queues later ones behind them. Members apply every stage right away. A scene that stops moving doesn't keep them
 * held, see QuestStall.
 *
 * Nothing is applied while the journal is open: it reads the player's objectives without the lock their writers take,
 * relying on the game being paused, and remote updates don't wait for that.
 *
 * Recovery: a quest that stops in the middle of a sequence only its later stage ends gets that sequence ended here,
 * whether a member's stop or its own scripts stopped it, or a save holds it stopped, see QuestRecovery.h. A member's
 * stage that a local, unshared sequence sets when it ends waits for that sequence here (StageOrder), and a crossfade
 * such a sequence left applied after its replacing stage ran is removed (CrossFade).
 */
class QuestService final : public BSTEventSink<TESQuestStartStopEvent>,
                           BSTEventSink<TESQuestStageEvent>,
                           BSTEventSink<TESQuestStageItemDoneEvent>,
                           BSTEventSink<TESSceneEvent>,
                           BSTEventSink<TESSceneActionEvent>,
                           BSTEventSink<TESScenePhaseEvent>,
                           BSTEventSink<TESTopicInfoEvent>,
                           BSTEventSink<TESLoadGameEvent>
{
public:
    QuestService(World&, entt::dispatcher&);
    ~QuestService() = default;

    static bool IsNonSyncableQuest(TESQuest* apQuest);
    static void DebugDumpQuests();

    struct HeldStages
    {
        // Member stages not applied yet, sorted
        Vector<uint16_t> Stages{};
        // Applied stages none of whose log entries' fragments has run yet
        Vector<uint16_t> PendingItems{};
        QuestStall Stall{};
    };

    struct Recovery
    {
        const QuestRecovery::Rule* pRule{nullptr};
        bool UnlockDoor{false};
        bool EndSequence{false};
        QuestRecovery::Steps Steps{};
    };

private:
    void OnConnected(const ConnectedEvent&) noexcept;
    void OnUpdate(const UpdateEvent& acEvent) noexcept;
    void OnDisconnected(const DisconnectedEvent&) noexcept;
    void OnPartyLeft(const PartyLeftEvent&) noexcept;
    void OnPartyJoined(const PartyJoinedEvent&) noexcept;

    BSTEventResult OnEvent(const TESQuestStartStopEvent*, const EventDispatcher<TESQuestStartStopEvent>*) override;
    BSTEventResult OnEvent(const TESQuestStageEvent*, const EventDispatcher<TESQuestStageEvent>*) override;
    BSTEventResult OnEvent(const TESQuestStageItemDoneEvent*, const EventDispatcher<TESQuestStageItemDoneEvent>*) override;
    BSTEventResult OnEvent(const TESSceneEvent*, const EventDispatcher<TESSceneEvent>*) override;
    BSTEventResult OnEvent(const TESSceneActionEvent*, const EventDispatcher<TESSceneActionEvent>*) override;
    BSTEventResult OnEvent(const TESScenePhaseEvent*, const EventDispatcher<TESScenePhaseEvent>*) override;
    BSTEventResult OnEvent(const TESTopicInfoEvent*, const EventDispatcher<TESTopicInfoEvent>*) override;
    BSTEventResult OnEvent(const TESLoadGameEvent*, const EventDispatcher<TESLoadGameEvent>*) override;

    void OnQuestUpdate(const NotifyQuestUpdate&) noexcept;
    void QueueQuestUpdate(uint32_t aFormId, uint16_t aStage, uint8_t aStatus) noexcept;
    void ApplyQuestUpdate(const NotifyQuestUpdate&) noexcept;
    void HoldStage(TESQuest* apQuest, uint16_t aStage) noexcept;
    // The quest's held stages, while it has stages to apply or fragments to wait for. Drops the stages that can't be
    // applied anymore.
    HeldStages* GetHeldStages(uint32_t aQuestFormId, TESQuest* apQuest) noexcept;
    void NotifyOverlayOfQuestUpdate(uint32_t aFormId) noexcept;
    void TrackFragments(TESQuest* apQuest, uint16_t aStage) noexcept;
    [[nodiscard]] bool AreFragmentsRunning(uint32_t aQuestFormId) const noexcept;
    void UpdatePendingStops(double aDelta) noexcept;
    void EndQuest(TESQuest* apQuest, uint8_t aStatus) noexcept;
    void ClearSession() noexcept;
    void StartRecovery(const QuestRecovery::Rule& acRule) noexcept;
    void UpdateRecoveries(double aDelta) noexcept;
    [[nodiscard]] bool IsRecovering(uint32_t aControllerFormId) const noexcept;
    [[nodiscard]] bool MustWaitForOwnSequence(const NotifyQuestUpdate& acUpdate) noexcept;
    [[nodiscard]] bool WaitsForOwnSequence(const NotifyQuestUpdate& acUpdate) noexcept;
    void ReleaseOrderedUpdates(double aDelta) noexcept;
    void RemoveLeftOverCrossFades() noexcept;

    World& m_world;
    std::atomic_uint64_t m_session{0};
    std::deque<NotifyQuestUpdate> m_pendingUpdates;
    // Keyed by quest form ID
    Map<uint32_t, HeldStages> m_heldStages{};
    // Member stages applied here whose fragments haven't run, keyed by quest form ID
    Map<uint32_t, QuestSharing::AppliedStages> m_appliedStages{};
    // Members' stops waiting for those fragments, keyed by quest form ID
    Map<uint32_t, QuestSharing::PendingStop> m_pendingStops{};
    // Quests ended here for a member, whose stop the game hasn't finished yet
    QuestSharing::EndingQuests m_endingQuests{};
    // Sequences being ended here, keyed by the form ID of the quest that stopped. Local repairs, kept across sessions.
    Map<uint32_t, Recovery> m_recoveries{};
    // Members' updates waiting for a local sequence to set their stage first, in order, see QuestRecovery::StageOrder
    std::deque<NotifyQuestUpdate> m_orderedUpdates;
    // How long they waited for the sequence's current run, shared by the rules
    double m_orderWaited{0.0};
    // Seconds until left over crossfades are checked, negative when no check is due
    double m_crossFadeCheckIn{-1.0};

    entt::scoped_connection m_joinedConnection;
    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_disconnectedConnection;
    entt::scoped_connection m_leftConnection;
    entt::scoped_connection m_partyJoinedConnection;
    entt::scoped_connection m_questUpdateConnection;
};
