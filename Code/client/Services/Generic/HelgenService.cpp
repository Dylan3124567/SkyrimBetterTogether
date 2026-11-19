#include <TiltedOnlinePCH.h>
#include <Services/HelgenService.h>
#include <Services/TransportService.h>
#include <Services/CharacterService.h>
#include <Services/OverlayService.h>
#include <Events/UpdateEvent.h>
#include <Events/DisconnectedEvent.h>
#include <Events/PartyLeftEvent.h>
#include <Forms/TESQuest.h>
#include <Interface/UI.h>
#include <Interface/IMenu.h>
#include <Messages/RequestHelgenReady.h>
#include <Messages/NotifyHelgenState.h>
#include <OverlayApp.hpp>
#include <PlayerCharacter.h>
#include <Forms/TESNPC.h>
#include <Forms/TESObjectCELL.h>

namespace
{
TESQuest* IntroQuest() { return Cast<TESQuest>(TESForm::GetById(0x3372B)); }
bool CharacterMenu() { return UI::Get() && UI::Get()->GetMenuOpen(BSFixedString("RaceSex Menu")); }
}

HelgenService::HelgenService(World& aWorld, entt::dispatcher& aDispatcher) : m_world(aWorld)
{
    m_updateConnection = aDispatcher.sink<UpdateEvent>().connect<&HelgenService::OnUpdate>(this);
    m_disconnectConnection = aDispatcher.sink<DisconnectedEvent>().connect<&HelgenService::OnDisconnected>(this);
    m_partyLeftConnection = aDispatcher.sink<PartyLeftEvent>().connect<&HelgenService::OnPartyLeft>(this);
    m_stateConnection = aDispatcher.sink<NotifyHelgenState>().connect<&HelgenService::OnState>(this);
}

bool HelgenService::PrepareConnection() noexcept
{
    if (m_active)
        return true;
    TESQuest* pQuest = IntroQuest();
    if (!pQuest || pQuest->IsStopped() || pQuest->currentStage >= 160)
        return true;
    if (!CharacterMenu() || pQuest->currentStage != 75)
    {
        Status("To join shared Helgen, start New Game and wait at character creation. Otherwise finish the intro first.");
        return false;
    }
    if (!HoldCreationScene())
    {
        Status("Shared Helgen could not hold the character-creation scene. Restart the intro before connecting.");
        return false;
    }
    m_active = true;
    Status("Shared Helgen: connect, then finish and name your character. The intro waits for everyone.");
    return true;
}

bool HelgenService::HoldCreationScene() noexcept
{
    auto* scene = Cast<BGSScene>(TESForm::GetById(0xCD68E));
    auto* ui = UI::Get();
    auto* menu = ui ? ui->FindMenuByName(BSFixedString("RaceSex Menu")) : nullptr;
    if (!scene || !scene->isPlaying || scene->currentPhaseIndex >= scene->phases.length || !menu)
        return false;
    s_creationSceneHeld = true;
    // Hold the scene, not actor loading/rendering: the other player's preview must remain live.
    constexpr uint32_t mask = IMenu::kPausesGame | IMenu::kFreezeFrameBackground | IMenu::kFreezeFramePause;
    m_creationMenuFlags = menu->uiMenuFlags & mask;
    const bool wasPaused = ui->numPausesGame != 0;
    if (menu->PausesGame() && ui->numPausesGame)
        --ui->numPausesGame;
    menu->ClearFlag(mask);
    if (wasPaused != (ui->numPausesGame != 0))
        ui->NotifyPauseChange(ui->numPausesGame != 0);
    return true;
}

bool HelgenService::PrepareSceneUpdate(BGSScene* apScene) const noexcept
{
    if (!m_sceneHostId || s_creationSceneHeld || !apScene->isPlaying ||
        (apScene->formID != 0xCD68E && apScene->formID != 0xAA7DF))
        return true;
    const uint32_t phase = (m_leaderScenePhases.load() >> (apScene->formID == 0xAA7DF ? 16 : 0)) & 0xFFFF;
    const uint32_t current = apScene->GetNextPhaseIndex();
    if (phase == 0xFFFE || (phase != 0xFFFF && current > phase))
        return false;
    // The remote host replays its owner's movement; it never completes a local AI package.
    for (auto* action : apScene->actions)
        if (action && action->actorId == 119 && action->GetType() == BGSSceneAction::Type::kPackage &&
            action->IsInPhase(current) && (phase == 0xFFFF || phase > action->endPhase))
            action->status |= 2;
    return true;
}

void HelgenService::ReleaseCreationScene() noexcept
{
    s_creationSceneHeld = false;
    if (m_creationMenuFlags)
        if (auto* ui = UI::Get())
            if (auto* menu = ui->FindMenuByName(BSFixedString("RaceSex Menu")))
            {
                const bool wasPaused = ui->numPausesGame != 0;
                if ((m_creationMenuFlags & IMenu::kPausesGame) && !menu->PausesGame() && CharacterMenu())
                    ++ui->numPausesGame;
                menu->SetFlag(m_creationMenuFlags);
                if (wasPaused != (ui->numPausesGame != 0))
                    ui->NotifyPauseChange(ui->numPausesGame != 0);
            }
    m_creationMenuFlags = 0;
}

bool HelgenService::DeferBridgeCollapse() const noexcept
{
    auto* quest = IntroQuest();
    if (!quest || quest->IsStopped() || quest->IsStageReached(500))
        return false;
    const auto* player = PlayerCharacter::Get();
    const auto* bridge = Cast<TESObjectREFR>(TESForm::GetById(0x945B8));
    const auto* trigger = Cast<TESObjectREFR>(TESForm::GetById(0xBA02E));
    if (!player || !bridge || !trigger)
        return true;
    // ponytail: vanilla Helgen geometry only; overhaul support needs its trigger metadata.
    // The midpoint towards the vanilla collapse trigger is safely beyond the far bank.
    // Later stages can also invoke 500, so defer the whole remote continuation until here.
    return player->parentCell != bridge->parentCell ||
           glm::distance(glm::vec3(player->position), glm::vec3(trigger->position)) >=
           glm::distance(glm::vec3(player->position), glm::vec3(bridge->position));
}

bool HelgenService::StagePlayer(Actor* apActor, uint32_t aPlayerId) const noexcept
{
    if (!CreatingCharacters() || !apActor)
        return false;
    auto* player = PlayerCharacter::Get();
    if (!player || !player->parentCell)
        return false;
    const auto& members = m_world.GetPartyService().GetPartyMembers();
    const auto member = std::find(members.begin(), members.end(), aPlayerId);
    const float side = 85.f + 65.f * static_cast<float>(std::distance(members.begin(), member));
    const float yaw = player->rotation.z;
    auto position = player->position;
    // Separate standing marks keep the preview out of the creation camera and local player capsule.
    position.x += side * std::cos(yaw) - 90.f * std::sin(yaw);
    position.y -= side * std::sin(yaw) + 90.f * std::cos(yaw);
    apActor->ForcePosition(position);
    apActor->SetRotation(0.f, 0.f, yaw);
    return true;
}

bool HelgenService::DeferExteriorBarrier(uint16_t aStage) const noexcept
{
    auto* intro = IntroQuest();
    auto* attack = Cast<TESQuest>(TESForm::GetById(0xD0593));
    // DragonAttack120 closes the inn; 164 closes the village passage; 190 closes the house.
    // Each has an independent local trigger: remote progress must not erect these barriers early.
    const uint16_t boundary = aStage >= 190 ? 190 : aStage >= 164 ? 164 : 120;
    return intro && !intro->IsStopped() && !intro->IsStageReached(200) && attack && !attack->IsStageReached(boundary);
}

bool HelgenService::DeferKeepEntry() const noexcept
{
    auto* intro = IntroQuest();
    if (!intro || intro->IsStopped() || intro->IsStageReached(200))
        return false;
    const auto* player = PlayerCharacter::Get();
    // MQ200 invokes DragonAttack200, disabling both exterior autoload doors.
    // ponytail: vanilla HelgenKeep01; overhauls need their destination cell IDs.
    return !player || !player->parentCell || player->parentCell->formID != 0x5DE24;
}

bool HelgenService::DeferTowerEntry() const noexcept
{
    auto* intro = IntroQuest();
    auto* attack = Cast<TESQuest>(TESForm::GetById(0xD0593));
    if (!intro || intro->IsStopped() || intro->IsStageReached(200) || !attack || attack->IsStageReached(50))
        return false;
    const auto* player = PlayerCharacter::Get();
    const auto* door = Cast<TESObjectREFR>(TESForm::GetById(0x10757B));
    const auto* inside = Cast<TESObjectREFR>(TESForm::GetById(0xDEECA));
    if (!player || !door || !inside)
        return true;
    // ponytail: vanilla tower geometry; use an overhaul's entrance marker if supporting it.
    // Stage 20 starts the tower scene; 22/25/50 can close the door and enable its solid blocker.
    // Do not accept those remote triggers until this player has crossed the entrance.
    return player->parentCell != door->parentCell ||
           glm::distance(glm::vec3(player->position), glm::vec3(inside->position)) >=
           glm::distance(glm::vec3(player->position), glm::vec3(door->position));
}

void HelgenService::Pause(bool aPause) noexcept
{
    UI* pUI = UI::Get();
    if (!pUI || m_paused == aPause)
        return;
    const bool wasPaused = pUI->numPausesGame != 0;
    // Own exactly one pause reference; closing RaceSex Menu releases only its own.
    if (aPause)
        ++pUI->numPausesGame;
    else if (pUI->numPausesGame)
        --pUI->numPausesGame;
    m_paused = aPause;
    if (wasPaused != (pUI->numPausesGame != 0))
        pUI->NotifyPauseChange(pUI->numPausesGame != 0);
}

void HelgenService::Cancel() noexcept
{
    if (m_sceneHostId)
        if (auto* quest = IntroQuest(); quest && quest->GetAliasedRef(119) != PlayerCharacter::Get())
            quest->ForceRefIntoAlias(119, PlayerCharacter::Get());
    ReleaseCreationScene();
    Pause(false);
    const bool wasActive = m_active;
    m_active = m_registered = m_ready = m_finished = m_leaderFinished = false;
    m_leaderAtBlock = m_sentAtBlock = false;
    m_sceneHostId = 0;
    m_leaderScenePhases = m_sentScenePhases = 0xFFFEFFFE;
    m_phase = m_reported = 0;
    m_session = m_readyCount = m_totalCount = 0;
    if (wasActive)
        Status("Shared Helgen ended. Continuing locally; reconnect after the intro or restart at character creation.");
}

void HelgenService::OnDisconnected(const DisconnectedEvent&) noexcept { Cancel(); }
void HelgenService::OnPartyLeft(const PartyLeftEvent&) noexcept
{
    if (m_active)
    {
        Cancel();
        m_world.GetTransport().Close();
    }
}

void HelgenService::Status(const String& acText) noexcept
{
    if (m_status == acText)
        return;
    m_status = acText;
    spdlog::info("Helgen: {}", acText);
    auto args = CefListValue::Create();
    args->SetString(0, acText.c_str());
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("helgenStatus", args);
}

void HelgenService::OnUpdate(const UpdateEvent&) noexcept
{
    UI* pUI = UI::Get();
    TESQuest* pQuest = IntroQuest();
    const bool loaded = pUI && !pUI->GetMenuOpen(BSFixedString("Main Menu")) && !pUI->GetMenuOpen(BSFixedString("Loading Menu"));
    if (!loaded || !m_world.GetTransport().IsOnline())
        m_towerBreachSynced = false;
    else if (!m_towerBreachSynced && m_world.GetPartyService().IsInParty() && !m_world.GetPartyService().IsLeader() &&
             pQuest && !pQuest->IsStopped() && pQuest->currentStage >= 160 && !pQuest->IsStageReached(200))
        if (auto* attack = Cast<TESQuest>(TESForm::GetById(0xD0593)); attack && attack->IsStageReached(103))
            if (auto* wall = Cast<TESObjectREFR>(TESForm::GetById(0x6CF54)); wall && wall->GetNiNode() && !wall->IsDisabled())
            {
                // The shared smash stage stops the guest's Alduin retry before its local furniture graph opens.
                BSFixedString event("Open");
                m_towerBreachSynced = wall->SendAnimationEvent(&event);
                if (m_towerBreachSynced)
                    spdlog::info("Helgen: restored local tower breach after shared smash stage");
            }
    if (!m_active || m_phase == 2)
        return;
    if (!loaded || !pQuest)
    {
        Cancel();
        m_world.GetTransport().Close();
        return;
    }
    const auto& party = m_world.GetPartyService();
    uint32_t hostId = 0;
    auto players = m_world.view<FormIdComponent, PlayerComponent>();
    for (auto entity : players)
        if (party.IsInParty() && !party.IsLeader() && players.get<PlayerComponent>(entity).Id == party.GetLeaderPlayerId())
            if (auto* actor = Cast<Actor>(TESForm::GetById(players.get<FormIdComponent>(entity).Id)); actor && (actor->GetNiNode() || m_phase == 1))
                hostId = actor->formID;
    m_sceneHostId = hostId;
    // The remote host cannot finish the guest's local chopping-block AI package.
    // Relay its arrival immediately: stage 97 releases the executioner's wind-up.
    if (m_phase == 1 && hostId && m_leaderAtBlock && pQuest->IsStageReached(95) && !pQuest->IsStageReached(97))
        pQuest->ScriptSetStage(97);
    // Only the host completes the chopping-block package. Its scene end starts Alduin for guests.
    if (m_phase == 1 && hostId && pQuest->IsStageReached(95) && !pQuest->IsStageReached(100) &&
        (m_leaderScenePhases.load() & 0xFFFF) == 0xFFFF)
        pQuest->ScriptSetStage(pQuest->IsStageReached(97) ? 100 : 97);
    // Face the procession while following; leave the spectator camera alone at the block.
    if (m_phase == 1 && pQuest->currentStage >= 80 && pQuest->currentStage < 95)
        if (auto* host = Cast<Actor>(TESForm::GetById(hostId)))
        {
            auto* player = PlayerCharacter::Get();
            const auto offset = glm::vec3(host->position) - glm::vec3(player->position);
            if (offset.x * offset.x + offset.y * offset.y > 1.f)
                player->SetRotation(0.f, 0.f, std::atan2(offset.x, offset.y));
        }
    // The guest never receives the execution reaction's animation event. The leader releases it.
    if (m_phase == 1 && hostId && m_leaderFinished && pQuest->IsStageReached(140) && !pQuest->IsStageReached(160))
        for (uint16_t stage : {145, 150, 160})
            if (!pQuest->IsStageReached(stage))
            {
                pQuest->ScriptSetStage(stage);
                break;
            }
    if (m_phase == 0 && !m_ready && !CharacterMenu())
    {
        // The live creation preview can leave the player's cached world model unchanged.
        // Rebuild it after the menu closes, before reporting this character ready.
        PlayerCharacter::Get()->QueueUpdate();
        m_ready = true;
    }
    if (m_phase == 1 && pQuest->IsStageReached(160))
    {
        m_finished = true;
        Pause(true);
    }
    if (!m_world.GetTransport().IsOnline())
        return;
    if (!m_world.GetPartyService().IsInParty())
    {
        Status("Shared Helgen: open Players & Party. One player must launch a party, then everyone must join it before the intro can continue.");
        return;
    }
    if (!m_registered)
    {
        RequestHelgenReady request;
        m_registered = m_world.GetTransport().Send(request);
    }
    const uint8_t state = m_finished ? 2 : m_ready ? 1 : 0;
    uint32_t loadedPlayers = 0;
    for (auto entity : players)
        if (std::find(party.GetPartyMembers().begin(), party.GetPartyMembers().end(), players.get<PlayerComponent>(entity).Id) != party.GetPartyMembers().end())
            if (auto* actor = Cast<Actor>(TESForm::GetById(players.get<FormIdComponent>(entity).Id)); actor && actor->GetNiNode())
                ++loadedPlayers;
    if (m_session && state > m_reported && (state != 1 || loadedPlayers >= party.GetPartyMembers().size()))
    {
        RequestHelgenReady request;
        request.Session = m_session;
        request.State = state;
        if (state == 1)
            m_world.ctx().at<CharacterService>().RunAppearanceUpdates(true);
        if (state == 1)
            if (auto* pNpc = Cast<TESNPC>(PlayerCharacter::Get()->baseForm))
                request.Name = pNpc->fullName.value.AsAscii();
        if (m_world.GetTransport().Send(request))
            m_reported = state;
    }
    if (m_phase == 1 && party.IsLeader() && m_reported == 1)
    {
        uint32_t phases = 0;
        for (uint32_t index = 0; index < 2; ++index)
        {
            auto* scene = Cast<BGSScene>(TESForm::GetById(index ? 0xAA7DF : 0xCD68E));
            const uint32_t phase = scene && scene->isPlaying ? scene->GetNextPhaseIndex() : 0xFFFF;
            phases |= std::min(phase, 0xFFFFu) << (16 * index);
        }
        const bool atBlock = pQuest->IsStageReached(97);
        if (phases != m_sentScenePhases || atBlock != m_sentAtBlock)
        {
            RequestHelgenReady request;
            request.Session = m_session;
            request.State = 1;
            request.ScenePhases = phases;
            request.LeaderAtBlock = atBlock;
            if (m_world.GetTransport().Send(request))
            {
                m_sentScenePhases = phases;
                m_sentAtBlock = atBlock;
            }
        }
    }
    if (m_phase == 0)
        Status((m_ready ? fmt::format("Shared Helgen: waiting for characters ({}/{} ready; at least 2 players). Disconnect to continue alone.", m_readyCount, std::max(m_totalCount, 2u))
                        : "Shared Helgen: close the multiplayer overlay, finish your character and confirm its name. Everyone waits until all characters are ready.").c_str());
    else if (m_phase == 1 && m_finished)
        Status("Shared Helgen: waiting for the other players to finish the scene.");
}

void HelgenService::RefreshActors() noexcept
{
    auto view = m_world.view<FormIdComponent>(entt::exclude<ObjectComponent>);
    Vector<entt::entity> entities(view.begin(), view.end());
    for (auto entity : entities)
        m_world.ctx().at<CharacterService>().ProcessNewEntity(entity);
}

void HelgenService::OnState(const NotifyHelgenState& acState) noexcept
{
    if (!m_active || (m_session && acState.Session != m_session) || !acState.Session)
        return;
    if (acState.Phase == 3)
    {
        Cancel();
        m_world.GetTransport().Close();
        Status("This party already started Helgen. Restart both players at character creation, or join after the intro.");
        return;
    }
    if (acState.Phase > 2 || acState.Ready > acState.Total || (acState.Phase >= 1 && !m_ready) || (acState.Phase == 2 && !m_finished))
        return;
    m_leaderFinished = acState.LeaderFinished;
    m_leaderAtBlock = acState.LeaderAtBlock;
    m_leaderScenePhases = acState.ScenePhases;
    m_session = acState.Session;
    m_readyCount = acState.Ready;
    m_totalCount = acState.Total;
    if (acState.Phase <= m_phase)
        return;
    m_phase = acState.Phase;
    if (m_phase == 1 && m_sceneHostId)
    {
        // Game.GetForm rejects signed FFxxxxxx runtime IDs; pass the actor through the quest alias.
        if (auto* host = Cast<Actor>(TESForm::GetById(m_sceneHostId)); host && IntroQuest())
            IntroQuest()->ForceRefIntoAlias(119, host);
        // Leave the creation preview mark and stand behind the host's actual network position.
        auto view = m_world.view<FormIdComponent, InterpolationComponent>();
        for (auto entity : view)
            if (view.get<FormIdComponent>(entity).Id == m_sceneHostId)
            {
                const auto& points = view.get<InterpolationComponent>(entity).TimePoints;
                if (!points.empty())
                {
                    const auto& point = points.back();
                    auto position = PlayerCharacter::Get()->position;
                    position.x = point.Position.x - 160.f * std::sin(point.Rotation.z);
                    position.y = point.Position.y - 160.f * std::cos(point.Rotation.z);
                    position.z = point.Position.z;
                    PlayerCharacter::Get()->ForcePosition(position);
                }
                break;
            }
    }
    ReleaseCreationScene();
    Pause(false);
    if (m_phase == 2)
    {
        m_sceneHostId = 0;
        RefreshActors();
    }
    Status("");
}
