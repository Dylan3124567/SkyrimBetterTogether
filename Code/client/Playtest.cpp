#include <TiltedOnlinePCH.h>
#include <Playtest.h>
#include "../playtest/Bridge.h"
#include <DInputHook.hpp>
#include <World.h>
#include <PlayerCharacter.h>
#include <TimeManager.h>
#include <Camera/PlayerCamera.h>
#include <Games/ActorExtension.h>
#include <AI/Movement/PlayerControls.h>
#include <Forms/ActorValueInfo.h>
#include <Interface/UI.h>
#include <Forms/TESQuest.h>
#include <Forms/TESPackage.h>
#include <Services/PapyrusService.h>
#include <Services/HelgenService.h>
#include <Services/OverlayService.h>
#include <Services/InputService.h>
#include <Services/TransportService.h>
#include <OverlayApp.hpp>
#include <Services/PartyService.h>
#include <Events/SetWaypointEvent.h>
#include <Forms/TESWorldSpace.h>
#include <Interface/Menus/CompassMarkers.h>
#include <sstream>

extern std::atomic_uint32_t g_unloadedActivations;

namespace Playtest
{
bool Baseline()
{
    static const bool enabled = [] {
        wchar_t control[2]{}, baseline[2]{};
        return GetEnvironmentVariableW(L"STR_PLAYTEST", control, 2) == 1 && control[0] == L'1' &&
               GetEnvironmentVariableW(L"STR_PLAYTEST_BASELINE", baseline, 2) == 1 && baseline[0] == L'1';
    }();
    return enabled;
}
static const PlaytestApi* s_api{};
static float s_delta{};
static bool s_mousePending{};
void BeginInput(float delta) { s_delta = delta; s_mousePending = true; }
bool Active() { return s_api && s_api->Active(); }
void Tick() { if (s_api) s_api->Tick(); }
void Capture(IDXGISwapChain* swap) { if (s_api) s_api->Capture(swap); }
static bool ReadData(bool keyboard, unsigned long size, void* data, unsigned long* count, unsigned long flags)
{
    // Device polling clears the event queue before processing the keyboard.
    if (keyboard && s_mousePending)
    {
        s_mousePending = false;
        PollMouse(s_delta);
    }
    return s_api->Data(keyboard, size, data, count, flags);
}

using TSetCursorPos = BOOL (WINAPI*)(int, int);
using TClipCursor = BOOL (WINAPI*)(const RECT*);
static TSetCursorPos RealSetCursorPos = nullptr;
static TClipCursor RealClipCursor = nullptr;
static BOOL WINAPI HookSetCursorPos(int x, int y) { return Active() ? TRUE : RealSetCursorPos(x, y); }
static BOOL WINAPI HookClipCursor(const RECT* rect) { return Active() ? TRUE : RealClipCursor(rect); }

static void Host(const char* request, char* output, unsigned capacity)
{
    std::istringstream in(request);
    std::string command, extra;
    in >> command;
    auto* world = Baseline() ? nullptr : &World::Get();
    auto* overlay = world ? &world->GetOverlayService() : nullptr;
    auto* ui = UI::Get();
    auto* player = PlayerCharacter::Get();
    std::string response = "{\"error\":\"unknown command\"}";
    if ((command == "scene" || command == "alias") &&
        (!ui || ui->GetMenuOpen(BSFixedString("Loading Menu")) ||
         (world && !world->ctx().at<PapyrusService>().Get("Actor", "IsDead"))))
        response = "{\"error\":\"actor diagnostics unavailable while the game is loading\"}";
    else if (command == "status")
    {
        auto* quest = Cast<TESQuest>(TESForm::GetById(0x3372B));
        std::string menus = "[";
        for (const char* name : {"Main Menu", "Loading Menu", "RaceSex Menu", "Console", "Dialogue Menu", "InventoryMenu", "Journal Menu", "MessageBoxMenu", "ContainerMenu", "BarterMenu", "TweenMenu", "MapMenu", "Tutorial Menu", "Lockpicking Menu", "Sleep/Wait Menu"})
            if (ui && ui->GetMenuOpen(BSFixedString(name)))
                menus += (menus.size() > 1 ? ",\"" : "\"") + std::string(name) + "\"";
        menus += "]";
        const bool playerReady = player && player->baseForm && ui && !ui->GetMenuOpen(BSFixedString("Main Menu")) && !ui->GetMenuOpen(BSFixedString("Loading Menu"));
        auto* controls = PlayerControls::GetInstance();
        float x{}, y{};
        Cursor(x, y);
        response = fmt::format("{{\"pid\":{},\"baseline\":{},\"menus\":{},\"mq101\":{},\"pause_refs\":{},\"control_flags\":{},\"movement_enabled\":{},\"input_blocked\":{},\"look_enabled\":{},\"bleeding_out\":{},\"health\":{},\"online\":{},\"overlay\":{},\"position\":[{},{},{}],\"rotation\":[{},{},{}],\"cursor\":[{},{}]}}",
            GetCurrentProcessId(), Baseline(), menus, quest ? quest->currentStage : 0, ui ? ui->numPausesGame : 0,
            BSInputEnableManager::Get() ? BSInputEnableManager::Get()->GetEnabledControls() : 0u,
            BSInputEnableManager::Get() && BSInputEnableManager::IsMovementControlsEnabled(), controls && controls->bBlockPlayerInput,
            controls && controls->pLookHandler && controls->pLookHandler->isEnabled, player && player->actorState.IsBleedingOut(),
            playerReady ? player->GetActorValue(ActorValueInfo::kHealth) : 0.f,
            world && world->GetTransport().IsOnline(), overlay && overlay->GetActive(), player ? player->position.x : 0, player ? player->position.y : 0, player ? player->position.z : 0,
            player ? player->rotation.x : 0, player ? player->rotation.y : 0, player ? player->rotation.z : 0, x, y);
        response.pop_back();
        response += fmt::format(",\"first_person\":{}}}", playerReady && PlayerCamera::Get() && PlayerCamera::Get()->IsFirstPerson());
    }
    else if (command == "clock" && TimeData::Get())
    {
        auto* time = TimeData::Get();
        response = fmt::format("{{\"hour\":{},\"days_passed\":{},\"day\":{},\"month\":{},\"year\":{},\"scale\":{}}}",
            time->GameHour->f, time->GameDaysPassed->f, time->GameDay->f, time->GameMonth->f, time->GameYear->f, time->TimeScale->f);
    }
    else if (command == "teleport" && world && player && ui &&
             !ui->GetMenuOpen("Loading Menu") && !ui->GetMenuOpen("Main Menu"))
    {
        uint32_t id{};
        if ((in >> std::hex >> id) && !(in >> extra))
            if (auto* target = Cast<TESObjectREFR>(TESForm::GetById(id)))
                if (!target->IsDeleted() && !target->IsDisabled() && target->GetParentCellEx())
                {
                    player->MoveTo(target->GetParentCellEx(), target->position);
                    response = "{\"ok\":true}";
                }
    }
    else if (command == "actor_value" && world && ui &&
             !ui->GetMenuOpen("Loading Menu") && !ui->GetMenuOpen("Main Menu"))
    {
        uint32_t id{}, value{};
        if ((in >> std::hex >> id >> std::dec >> value) && !(in >> extra) && value < ActorValueInfo::kActorValueCount)
            if (auto* actor = Cast<Actor>(TESForm::GetById(id)))
                if (actor->baseForm && !actor->IsDeleted())
                    response = fmt::format("{{\"form\":{},\"value\":{},\"current\":{},\"permanent\":{}}}",
                        id, value, actor->GetActorValue(value), actor->GetActorPermanentValue(value));
    }
    else if (command == "activation")
        response = fmt::format("{{\"unloaded_after_activation\":{}}}", g_unloadedActivations.load());
    else if (command == "helgen" && world)
        response = fmt::format("{{\"host_form\":{},\"creation_held\":{}}}", world->ctx().at<HelgenService>().GetSceneHostId(), HelgenService::HoldScene(0xCD68E));
    else if (command == "ref")
    {
        uint32_t id{};
        if ((in >> std::hex >> id) && !(in >> extra))
        {
            if (auto* ref = Cast<TESObjectREFR>(TESForm::GetById(id)))
                response = fmt::format("{{\"form\":{},\"disabled\":{},\"deleted\":{},\"loaded\":{},\"position\":[{},{},{}]}}",
                    id, ref->IsDisabled(), ref->IsDeleted(), ref->GetNiNode() != nullptr, ref->position.x, ref->position.y, ref->position.z);
            else if (auto* quest = Cast<TESQuest>(TESForm::GetById(id)))
                response = fmt::format("{{\"form\":{},\"stage\":{},\"stage20\":{},\"stage22\":{},\"stage25\":{},\"stage50\":{}}}",
                    id, quest->currentStage, quest->IsStageReached(20), quest->IsStageReached(22), quest->IsStageReached(25), quest->IsStageReached(50));
        }
    }
    else if (command == "players" && world)
    {
        response = "{\"players\":[";
        auto view = world->view<FormIdComponent, PlayerComponent>();
        bool first = true;
        for (auto entity : view)
            if (auto* actor = Cast<Actor>(TESForm::GetById(view.get<FormIdComponent>(entity).Id)))
            {
                response += fmt::format("{}{{\"player\":{},\"form\":{},\"loaded\":{},\"disabled\":{},\"position\":[{},{},{}]}}",
                    first ? "" : ",", view.get<PlayerComponent>(entity).Id, actor->formID, actor->GetNiNode() != nullptr, actor->IsDisabled(),
                    actor->position.x, actor->position.y, actor->position.z);
                first = false;
            }
        response += "]}";
    }
    // Synced actors with their owner and state, to compare what both clients show
    else if (command == "actors" && world)
    {
        response = "{\"actors\":[";
        auto view = world->view<FormIdComponent>();
        bool first = true;
        for (auto entity : view)
        {
            auto* actor = Cast<Actor>(TESForm::GetById(view.get<FormIdComponent>(entity).Id));
            if (!actor)
                continue;
            uint32_t serverId = 0;
            const char* owner = "none";
            if (const auto* local = world->try_get<LocalComponent>(entity))
                serverId = local->Id, owner = "local";
            else if (const auto* remote = world->try_get<RemoteComponent>(entity))
                serverId = remote->Id, owner = "remote";
            response += fmt::format("{}{{\"form\":{},\"base\":{},\"server\":{},\"owner\":\"{}\",\"dead\":{},\"state\":{},\"loaded\":{},\"position\":[{:.0f},{:.0f},{:.0f}]}}",
                first ? "" : ",", actor->formID, actor->baseForm ? actor->baseForm->formID : 0, serverId, owner, actor->IsDead(), actor->actorState.flags1,
                actor->GetNiNode() != nullptr, actor->position.x, actor->position.y, actor->position.z);
            first = false;
        }
        response += "]}";
    }
    // Test sessions connect and form parties without the overlay's forms
    else if (command == "connect" && world)
    {
        std::string endpoint, password;
        if ((in >> endpoint) && endpoint.find_first_of("\\/") == std::string::npos)
        {
            std::getline(in >> std::ws, password);
            world->GetRunner().Queue([endpoint, password] {
                World::Get().GetTransport().SetServerPassword(password);
                if (!World::Get().GetTransport().Connect(endpoint))
                    World::Get().GetTransport().OnDisconnected(Client::kLocalProblem);
            });
            response = "{\"queued\":true}";
        }
    }
    else if (command == "party" && world)
    {
        const auto& party = world->GetPartyService();
        std::string members;
        for (const uint32_t member : party.GetPartyMembers())
            members += (members.empty() ? "" : ",") + std::to_string(member);
        response = fmt::format("{{\"local\":{},\"in_party\":{},\"leader\":{},\"members\":[{}]}}", world->GetTransport().GetLocalPlayerId(), party.IsInParty(),
                               party.IsInParty() ? party.GetLeaderPlayerId() : 0, members);
    }
    else if ((command == "party_create" || command == "party_invite" || command == "party_accept") && world)
    {
        uint32_t id{};
        if (command == "party_create" || (in >> id))
        {
            world->GetRunner().Queue([command, id] {
                auto& party = World::Get().GetPartyService();
                if (command == "party_create")
                    party.CreateParty();
                else if (command == "party_invite")
                    party.CreateInvite(id);
                else
                    party.AcceptInvite(id);
            });
            response = "{\"queued\":true}";
        }
    }
    // Places the local custom map marker the way the map does, so it reaches the party
    else if (command == "waypoint" && world && player && ui && !ui->GetMenuOpen("Loading Menu") && !ui->GetMenuOpen("Main Menu"))
    {
        NiPoint3 position{};
        if ((in >> position.x >> position.y >> position.z) && player->GetWorldSpace())
        {
            player->SetWaypoint(&position, player->GetWorldSpace());
            Vector3_NetQuantize netPosition{};
            netPosition.x = position.x;
            netPosition.y = position.y;
            netPosition.z = position.z;
            world->GetRunner().Trigger(SetWaypointEvent(netPosition, player->GetWorldSpace()->formID));
            response = "{\"ok\":true}";
        }
    }
    else if (command == "compass")
    {
        const auto status = CompassMarkers::GetStatus();
        response = fmt::format("{{\"targets\":{},\"decorated\":{},\"helper_loads\":{},\"local_colour\":{}}}", status.Targets, status.Decorated, status.HelperLoads, status.LocalColour);
    }
    else if (command == "alias")
    {
        uint32_t questId{}, aliasId{};
        if ((in >> std::hex >> questId >> std::dec >> aliasId) && !(in >> extra) && aliasId <= 65535)
            if (auto* quest = Cast<TESQuest>(TESForm::GetById(questId)))
                if (auto* actor = Cast<Actor>(quest->GetAliasedRef(aliasId)))
                    response = fmt::format("{{\"form\":{},\"remote\":{},\"deleted\":{},\"dead\":{},\"loaded\":{},\"health\":{},\"position\":[{},{},{}]}}",
                        actor->formID, world && actor->GetExtension()->IsRemote(), actor->IsDeleted(), world ? (actor->IsDead() ? "true" : "false") : "null", actor->GetNiNode() != nullptr,
                        actor->baseForm && actor->GetNiNode() ? actor->GetActorValue(ActorValueInfo::kHealth) : 0.f,
                        actor->position.x, actor->position.y, actor->position.z);
    }
    else if (command == "scene")
    {
        uint32_t id{};
        if ((in >> std::hex >> id) && !(in >> extra))
        {
            if (auto* scene = Cast<BGSScene>(TESForm::GetById(id)))
            {
                std::string actions = "[";
                for (auto* action : scene->actions)
                    if (action && action->IsInPhase(scene->currentPhaseIndex))
                    {
                        auto* actor = scene->owningQuest ? Cast<Actor>(scene->owningQuest->GetAliasedRef(action->actorId)) : nullptr;
                        const void* getPackage = world ? world->ctx().at<PapyrusService>().Get("Actor", "GetCurrentPackage") : nullptr;
                        auto* package = actor && getPackage ? PapyrusFunction<TESPackage*, Actor>(getPackage)(actor) : nullptr;
                        actions += (actions.size() > 1 ? "," : "") + fmt::format("{{\"index\":{},\"actor\":{},\"type\":{},\"running\":{},\"complete\":{},\"form\":{},\"remote\":{},\"deleted\":{},\"dead\":{},\"health\":{},\"quest_alias\":{},\"loaded\":{},\"package\":{},\"position\":[{},{},{}]}}",
                            action->GetIndex(), action->actorId, static_cast<uint32_t>(action->GetType()), action->IsRunning(), action->IsComplete(), actor ? actor->formID : 0,
                            actor && world && actor->GetExtension()->IsRemote(), actor && actor->IsDeleted(), world ? (actor && actor->IsDead() ? "true" : "false") : "null",
                            actor && actor->baseForm && actor->GetNiNode() ? actor->GetActorValue(ActorValueInfo::kHealth) : 0.f, actor && actor->extraData.Contains(ExtraDataType::AliasInstanceArray),
                            actor && actor->GetNiNode(), package ? package->formID : 0, actor ? actor->position.x : 0, actor ? actor->position.y : 0, actor ? actor->position.z : 0);
                    }
                response = fmt::format("{{\"playing\":{},\"phase\":{},\"actions\":{}]}}", scene->isPlaying, scene->currentPhaseIndex, actions);
            }
        }
    }
    else if (command == "cursor")
    {
        int xi, yi;
        if ((in >> xi >> yi) && !(in >> extra) && xi >= 0 && xi < 16384 && yi >= 0 && yi < 16384)
        {
            float x = static_cast<float>(xi), y = static_cast<float>(yi);
            if (Cursor(x, y, true)) response = "{\"ok\":true}";
        }
    }
    else if (command == "overlay" && overlay)
    {
        int enabled;
        if ((in >> enabled) && !(in >> extra) && (enabled == 0 || enabled == 1) && overlay->GetOverlayApp())
        {
            overlay->SetActive(enabled != 0);
            response = "{\"ok\":true}";
        }
    }
    else if (command == "ui_mouse" && overlay)
    {
        int x, y, button, down;
        if ((in >> x >> y >> button >> down) && !(in >> extra) && x >= 0 && x < 16384 && y >= 0 && y < 16384 && button >= -1 && button <= 2 && (down == 0 || down == 1))
        {
            if (!overlay->GetActive() && button == 0 && down && InputService::OpenMenuShortcutAt(x, y))
            {
                response = "{\"ok\":true}";
            }
            else if (overlay->GetActive())
            {
                overlay->GetOverlayApp()->InjectMouseMove(x, y, 0);
                if (button >= 0)
                    overlay->GetOverlayApp()->InjectMouseButton(x, y, static_cast<cef_mouse_button_type_t>(button), !down, 0);
                response = "{\"ok\":true}";
            }
        }
    }
    else if (command == "ui_key" && overlay && overlay->GetActive())
    {
        int type, key, modifiers;
        if ((in >> type >> key >> modifiers) && !(in >> extra) && type >= 0 && type <= 3 && key >= 0 && key <= 65535 && modifiers >= 0 && modifiers <= 2047)
        {
            overlay->GetOverlayApp()->InjectKey(static_cast<cef_key_event_type_t>(type), modifiers, key, 0);
            response = "{\"ok\":true}";
        }
    }
    if (response.size() >= capacity)
        response = "{\"error\":\"diagnostic response exceeds bridge capacity\"}";
    strncpy_s(output, capacity, response.c_str(), _TRUNCATE);
}

void Initialize()
{
    wchar_t enabled[2]{};
    if (GetEnvironmentVariableW(L"STR_PLAYTEST", enabled, 2) != 1 || enabled[0] != L'1')
        return;
    const auto root = TiltedPhoques::GetPath();
    HMODULE module = LoadLibraryExW((root / L"PlaytestBridge.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module) { spdlog::error("Playtest DLL load failed: {}", GetLastError()); return; }
    auto start = reinterpret_cast<StartPlaytest>(GetProcAddress(module, "StartBridge"));
    s_api = start ? start(Host, root.c_str()) : nullptr;
    if (!s_api || s_api->Version != 1) { s_api = nullptr; spdlog::error("Playtest bridge initialization failed"); return; }
    auto& input = TiltedPhoques::DInputHook::Get();
    input.VirtualState = s_api->State;
    input.VirtualData = ReadData;
    TP_HOOK_IAT(SetCursorPos, "user32.dll");
    TP_HOOK_IAT(ClipCursor, "user32.dll");
    spdlog::info("Local playtest bridge ready: SkyrimPlaytest-{}", GetCurrentProcessId());
}
}
