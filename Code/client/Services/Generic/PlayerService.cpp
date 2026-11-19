#include <Services/PlayerService.h>

#include <World.h>

#include <Events/UpdateEvent.h>
#include <Events/ConnectedEvent.h>
#include <Events/DisconnectedEvent.h>
#include <Events/GridCellChangeEvent.h>
#include <Events/CellChangeEvent.h>
#include <Events/PlayerDialogueEvent.h>
#include <Events/PlayerLevelEvent.h>
#include <Events/PartyJoinedEvent.h>
#include <Events/PartyLeftEvent.h>
#include <Events/BeastFormChangeEvent.h>
#include <Events/AddExperienceEvent.h>

#include <Messages/PlayerRespawnRequest.h>
#include <Messages/NotifyPlayerRespawn.h>
#include <Messages/ShiftGridCellRequest.h>
#include <Messages/EnterExteriorCellRequest.h>
#include <Messages/EnterInteriorCellRequest.h>
#include <Messages/PlayerDialogueRequest.h>
#include <Messages/PlayerLevelRequest.h>

#include <Structs/ServerSettings.h>

#include <PlayerCharacter.h>
#include <Forms/TESObjectCELL.h>
#include <Forms/TESGlobal.h>
#include <Games/Overrides.h>
#include <Games/References.h>
#include <AI/AIProcess.h>
#include <AI/Movement/PlayerControls.h>
#include <EquipManager.h>
#include <Forms/TESRace.h>
#include <Forms/TESQuest.h>
#include <Games/Skyrim/Interface/UI.h>
#include <Services/PapyrusService.h>

PlayerService::PlayerService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_dispatcher(aDispatcher)
    , m_transport(aTransport)
{
    m_updateConnection = m_dispatcher.sink<UpdateEvent>().connect<&PlayerService::OnUpdate>(this);
    m_connectedConnection = m_dispatcher.sink<ConnectedEvent>().connect<&PlayerService::OnConnected>(this);
    m_disconnectedConnection = m_dispatcher.sink<DisconnectedEvent>().connect<&PlayerService::OnDisconnected>(this);
    m_settingsConnection = m_dispatcher.sink<ServerSettings>().connect<&PlayerService::OnServerSettingsReceived>(this);
    m_notifyRespawnConnection = m_dispatcher.sink<NotifyPlayerRespawn>().connect<&PlayerService::OnNotifyPlayerRespawn>(this);
    m_gridCellChangeConnection = m_dispatcher.sink<GridCellChangeEvent>().connect<&PlayerService::OnGridCellChangeEvent>(this);
    m_cellChangeConnection = m_dispatcher.sink<CellChangeEvent>().connect<&PlayerService::OnCellChangeEvent>(this);
    m_playerDialogueConnection = m_dispatcher.sink<PlayerDialogueEvent>().connect<&PlayerService::OnPlayerDialogueEvent>(this);
    m_playerLevelConnection = m_dispatcher.sink<PlayerLevelEvent>().connect<&PlayerService::OnPlayerLevelEvent>(this);
    m_partyJoinedConnection = aDispatcher.sink<PartyJoinedEvent>().connect<&PlayerService::OnPartyJoinedEvent>(this);
    m_partyLeftConnection = aDispatcher.sink<PartyLeftEvent>().connect<&PlayerService::OnPartyLeftEvent>(this);

    EventDispatcherManager::Get()->deathEvent.RegisterSink(this);
}

BSTEventResult PlayerService::OnEvent(const TESDeathEvent* apEvent, const EventDispatcher<TESDeathEvent>*)
{
    // The killer is only the player for the player's own killing blow, not for a companion's or a party member's.
    if (!apEvent || !apEvent->pActorKiller || apEvent->pActorKiller != PlayerCharacter::Get())
        return BSTEventResult::kOk;

    const float cExperience = PlayerCharacter::TakeKillExperience();
    if (cExperience > 0.f && m_world.GetPartyService().IsInParty())
        m_world.GetRunner().Trigger(AddExperienceEvent(cExperience));

    return BSTEventResult::kOk;
}

void PlayerService::OnUpdate(const UpdateEvent&) noexcept
{
    RunRespawnUpdates();
    RunDifficultyUpdates();
    RunLevelUpdates();
    RunBeastFormDetection();
}

void PlayerService::OnConnected(const ConnectedEvent& acEvent) noexcept
{
    // TODO: SkyrimTogether.esm
    TESGlobal* pKillMove = Cast<TESGlobal>(TESForm::GetById(0x100F19));
    pKillMove->f = 0.f;

    TESGlobal* pWorldEncountersEnabled = Cast<TESGlobal>(TESForm::GetById(0xB8EC1));
    pWorldEncountersEnabled->f = 0.f;
}

void PlayerService::OnDisconnected(const DisconnectedEvent& acEvent) noexcept
{
    PlayerCharacter::Get()->SetDifficulty(m_previousDifficulty);
    m_serverDifficulty = m_previousDifficulty = 6;

    ToggleDeathSystem(false);

    TESGlobal* pKillMove = Cast<TESGlobal>(TESForm::GetById(0x100F19));
    pKillMove->f = 1.f;

    // Restore to the default value (150 in skyrim, 175 in fallout 4)
    float* greetDistance = Settings::GetGreetDistance();
    *greetDistance = 150.f;

    TESGlobal* pWorldEncountersEnabled = Cast<TESGlobal>(TESForm::GetById(0xB8EC1));
    pWorldEncountersEnabled->f = 1.f;
}

void PlayerService::OnServerSettingsReceived(const ServerSettings& acSettings) noexcept
{
    m_previousDifficulty = *Settings::GetDifficulty();
    PlayerCharacter::Get()->SetDifficulty(acSettings.Difficulty);
    m_serverDifficulty = acSettings.Difficulty;

    if (!acSettings.GreetingsEnabled)
    {
        float* greetDistance = Settings::GetGreetDistance();
        *greetDistance = 0.f;
    }

    ToggleDeathSystem(acSettings.DeathSystemEnabled);
}

void PlayerService::OnNotifyPlayerRespawn(const NotifyPlayerRespawn& acMessage) const noexcept
{
    PlayerCharacter::Get()->PayGold(acMessage.GoldLost);

    std::string message = fmt::format("You died and lost {} gold.", acMessage.GoldLost);
    Utils::ShowHudMessage(String(message));
}

void PlayerService::OnGridCellChangeEvent(const GridCellChangeEvent& acEvent) const noexcept
{
    uint32_t baseId = 0;
    uint32_t modId = 0;

    if (m_world.GetModSystem().GetServerModId(acEvent.WorldSpaceId, modId, baseId))
    {
        ShiftGridCellRequest request;
        request.WorldSpaceId = GameId(modId, baseId);
        request.PlayerCell = acEvent.PlayerCell;
        request.CenterCoords = acEvent.CenterCoords;
        request.Cells = acEvent.Cells;

        m_transport.Send(request);
    }
}

void PlayerService::OnCellChangeEvent(const CellChangeEvent& acEvent) const noexcept
{
    if (acEvent.WorldSpaceId)
    {
        EnterExteriorCellRequest message;
        message.CellId = acEvent.CellId;
        message.WorldSpaceId = acEvent.WorldSpaceId;
        message.CurrentCoords = acEvent.CurrentCoords;

        m_transport.Send(message);
    }
    else
    {
        EnterInteriorCellRequest message;
        message.CellId = acEvent.CellId;

        m_transport.Send(message);
    }
}

void PlayerService::OnPlayerDialogueEvent(const PlayerDialogueEvent& acEvent) const noexcept
{
    if (!m_transport.IsConnected())
        return;

    const auto& partyService = m_world.GetPartyService();
    if (!partyService.IsInParty())
        return;

    PlayerDialogueRequest request{};
    request.Text = acEvent.Text;

    m_transport.Send(request);
}

void PlayerService::OnPlayerLevelEvent(const PlayerLevelEvent& acEvent) const noexcept
{
    if (!m_transport.IsConnected())
        return;

    PlayerLevelRequest request{};
    request.NewLevel = PlayerCharacter::Get()->GetLevel();

    m_transport.Send(request);
}

void PlayerService::OnPartyJoinedEvent(const PartyJoinedEvent& acEvent) noexcept
{
    // TODO: this can be done a bit prettier
    if (acEvent.IsLeader)
    {
        TESGlobal* pWorldEncountersEnabled = Cast<TESGlobal>(TESForm::GetById(0xB8EC1));
        pWorldEncountersEnabled->f = 1.f;
    }
}

void PlayerService::OnPartyLeftEvent(const PartyLeftEvent& acEvent) noexcept
{
    // TODO: this can be done a bit prettier
    if (World::Get().GetTransport().IsConnected())
    {
        TESGlobal* pWorldEncountersEnabled = Cast<TESGlobal>(TESForm::GetById(0xB8EC1));
        pWorldEncountersEnabled->f = 0.f;
    }
}

void PlayerService::RunRespawnUpdates() noexcept
{
    if (!m_isDeathSystemEnabled)
        return;

    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    const bool cIsDown = pPlayer->actorState.IsBleedingOut();

    if (!cIsDown && m_respawnFlow.IsUp())
    {
        m_cachedMainSpellId = pPlayer->magicItems[0] ? pPlayer->magicItems[0]->formID : 0;
        m_cachedSecondarySpellId = pPlayer->magicItems[1] ? pPlayer->magicItems[1]->formID : 0;
        m_cachedPowerId = pPlayer->equippedShout ? pPlayer->equippedShout->formID : 0;

        if (pPlayer->GetActorValue(ActorValueInfo::kHealth) > 0.f)
            m_aliveControls = BSInputEnableManager::Get()->GetEnabledControls();
    }

    RespawnFlow::Frame frame{};
    frame.Now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    frame.IsDown = cIsDown;
    frame.IsStuck = cIsDown || pPlayer->actorState.IsKnockedDown();
    frame.HasBroken = RespawnProtection::ConsumeBreak();

    if (m_respawnFlow.GetPhase() != RespawnFlow::Phase::Alive)
    {
        const auto* pUI = UI::Get();
        frame.IsLoading = !pUI || pUI->GetMenuOpen(BSFixedString("Loading Menu")) || !pPlayer->parentCell || !pPlayer->GetNiNode();

        // Looking around doesn't count, it can't hurt anyone and a nudged mouse shouldn't cost the player their protection
        if (const auto* pControls = PlayerControls::GetInstance())
        {
            const auto& cMove = pControls->Data.MoveInputVec;
            frame.HasMovementInput = pControls->Data.bAutoMove || cMove.x * cMove.x + cMove.y * cMove.y > 0.01f;
        }
    }

    ApplyRespawnCommands(m_respawnFlow.Update(frame));

    // The respawn let the player recover from bleedout so they stand up, the next time they go down must be for good again
    if (m_canLeaveBleedout && !cIsDown && m_respawnFlow.IsUp())
    {
        pPlayer->SetNoBleedoutRecovery(true);
        m_canLeaveBleedout = false;
    }

    RespawnProtection::SetActive(m_respawnFlow.IsInvulnerable());
}

void PlayerService::ApplyRespawnCommands(uint32_t aCommands) noexcept
{
    if (aCommands == RespawnFlow::kNone)
        return;

    PlayerCharacter* pPlayer = PlayerCharacter::Get();

    if (aCommands & RespawnFlow::kEndProtection)
    {
        PlayerCharacter::SetGodMode(false);

        if (!(aCommands & RespawnFlow::kBeginDeath))
            Utils::ShowHudMessage("Spawn protection ended.");
    }

    if (aCommands & RespawnFlow::kBeginDeath)
    {
        // Death disables controls in several steps. Keep the last living state until recovery.
        m_respawnControls = m_aliveControls;
        FadeOutGame(true, true, 3.0f, true, 2.0f);

        // If a player dies not by its health reaching 0, getting it up from its bleedout state isn't possible
        // just by setting its health back to max. Therefore, put it to 0.
        if (pPlayer->GetActorValue(ActorValueInfo::kHealth) > 0.f)
            pPlayer->ForceActorValue(ActorValueOwner::ForceMode::DAMAGE, ActorValueInfo::kHealth, 0);

        pPlayer->PayCrimeGoldToAllFactions();
    }

    if (aCommands & RespawnFlow::kRespawn)
    {
        // Before anything else, whatever killed the player may still be there and the next hit must not land
        PlayerCharacter::SetGodMode(true);

        pPlayer->RespawnPlayer();
        m_canLeaveBleedout = true;

        m_transport.Send(PlayerRespawnRequest());

        auto* pEquipManager = EquipManager::Get();
        TESForm* pSpell = TESForm::GetById(m_cachedMainSpellId);
        if (pSpell)
            pEquipManager->EquipSpell(pPlayer, pSpell, 0);
        pSpell = TESForm::GetById(m_cachedSecondarySpellId);
        if (pSpell)
            pEquipManager->EquipSpell(pPlayer, pSpell, 1);
        pSpell = TESForm::GetById(m_cachedPowerId);
        if (pSpell)
            pEquipManager->EquipShout(pPlayer, pSpell);
    }

    if (aCommands & RespawnFlow::kForceState)
    {
        spdlog::warn("Player is still down after respawning, clearing actor state: {:X}", pPlayer->actorState.flags1);
        pPlayer->actorState.ClearDownedState();
    }

    if (aCommands & RespawnFlow::kForceGraph)
    {
        spdlog::warn("Player is still down after respawning, resetting behavior graph");
        BSFixedString defaultState("IdleForceDefaultState");
        pPlayer->SendAnimationEvent(&defaultState);
    }

    if (aCommands & RespawnFlow::kGetUp)
    {
        auto* pInput = BSInputEnableManager::Get();
        pInput->EnableOtherEvent(m_respawnControls & ~pInput->GetEnabledControls(), true, true);
    }

    // Only when getting up from bleedout and both resets above failed, the get up from a ragdoll always ends standing
    if (aCommands & RespawnFlow::kKnock)
    {
        spdlog::warn("Player is still down after respawning, ragdolling them so they get up");
        if (pPlayer->currentProcess)
            pPlayer->currentProcess->KnockExplosion(pPlayer, &pPlayer->position, 0.f);
    }

    if (aCommands & RespawnFlow::kFadeIn)
        FadeOutGame(false, true, 0.5f, true, 2.f);

    if (aCommands & RespawnFlow::kStartProtection)
    {
        // Controls that went missing before the last death aren't in m_respawnControls, and without these the player can
        // only look around. The respawn already pulled the player out of whatever quest scene disabled them, except during
        // the intro, which respawns in place.
        auto* pIntro = Cast<TESQuest>(TESForm::GetById(0x3372B));
        if (!pIntro || pIntro->IsStopped() || pIntro->IsStageReached(1000))
        {
            PAPYRUS_FUNCTION(void, Actor, SetDontMove, bool);
            s_pSetDontMove(pPlayer, false);

            auto* pInput = BSInputEnableManager::Get();
            pInput->EnableOtherEvent(BSInputEnableManager::kPlayerControls & ~pInput->GetEnabledControls(), true, true);
        }

        m_respawnControls = 0;

        Utils::ShowHudMessage("Spawn protection active until you move or act.");
    }

    if (aCommands & RespawnFlow::kFadeInNow)
        FadeOutGame(false, true, 0.5f, true, 0.f);
}

void PlayerService::RunDifficultyUpdates() const noexcept
{
    if (!m_transport.IsConnected())
        return;

    PlayerCharacter::Get()->SetDifficulty(m_serverDifficulty);
}

void PlayerService::RunLevelUpdates() const noexcept
{
    // The LevelUp hook is kinda weird, so ehh, just check periodically, doesn't really cost anything.

    static std::chrono::steady_clock::time_point lastSendTimePoint;
    constexpr auto cDelayBetweenUpdates = 1000ms;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSendTimePoint < cDelayBetweenUpdates)
        return;

    lastSendTimePoint = now;

    static uint16_t oldLevel = PlayerCharacter::Get()->GetLevel();

    uint16_t newLevel = PlayerCharacter::Get()->GetLevel();
    if (newLevel != oldLevel)
    {
        PlayerLevelRequest request{};
        request.NewLevel = newLevel;

        m_transport.Send(request);

        oldLevel = newLevel;
    }
}

void PlayerService::RunBeastFormDetection() const noexcept
{
    static uint32_t lastRaceFormID = 0;
    static std::chrono::steady_clock::time_point lastSendTimePoint;
    constexpr auto cDelayBetweenUpdates = 250ms;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSendTimePoint < cDelayBetweenUpdates)
        return;

    lastSendTimePoint = now;

    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pPlayer->race)
        return;

    if (pPlayer->race->formID == lastRaceFormID)
        return;

    if (pPlayer->race->formID == 0x200283A || pPlayer->race->formID == 0xCDD84)
        m_world.GetDispatcher().trigger(BeastFormChangeEvent());

    lastRaceFormID = pPlayer->race->formID;
}

void PlayerService::ToggleDeathSystem(bool aSet) noexcept
{
    if (!aSet)
    {
        if (m_respawnFlow.IsInvulnerable())
            PlayerCharacter::SetGodMode(false);
        if (m_respawnFlow.IsScreenFaded())
            FadeOutGame(false, true, 0.5f, true, 0.f);

        m_respawnFlow.Reset();
        RespawnProtection::SetActive(false);
        m_respawnControls = 0;
        m_canLeaveBleedout = false;
    }
    m_isDeathSystemEnabled = aSet;

    PlayerCharacter::Get()->SetPlayerRespawnMode(aSet);
}
