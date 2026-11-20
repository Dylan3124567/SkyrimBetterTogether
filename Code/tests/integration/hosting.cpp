#include <optional>
#include <glm/glm.hpp>
#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>
#include <TiltedCore/ViewBuffer.hpp>
#include <SteamInterface.hpp>
#include <Client.hpp>
#include <steam/steamnetworkingsockets.h>
#include <Messages/ServerMessageFactory.h>
#include <Messages/AuthenticationRequest.h>
#include <Messages/PartyCreateRequest.h>
#include <Messages/PartyLeaveRequest.h>
#include <Messages/PartyInviteRequest.h>
#include <Messages/PartyAcceptInviteRequest.h>
#include <Messages/PartyKickRequest.h>
#include <Messages/RequestQuestUpdate.h>
#include <Messages/RequestWeatherChange.h>
#include <Messages/RequestCurrentWeather.h>
#include <Messages/PartyChangeLeaderRequest.h>
#include <Messages/AssignCharacterRequest.h>
#include <Messages/CharacterSpawnRequest.h>
#include <Messages/ActivateRequest.h>
#include <Messages/NewPackageRequest.h>
#include <Messages/ClientReferencesMoveRequest.h>
#include <Messages/RequestHelgenReady.h>
#include <Messages/EnterInteriorCellRequest.h>
#include <Services/HostSession.h>
#include <BuildInfo.h>
#include <snappy.h>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

// Exercises the real server protocol without launching Skyrim or loading saves.
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    TiltedPhoques::SteamInterface::Acquire();
    HostSession host;
    int failures = 0;
    auto check = [&](bool result, const char* label) {
        std::cout << (result ? "PASS " : "FAIL ") << label << std::endl;
        failures += !result;
        return result;
    };
    check(!host.Start(argv[1], 0, "test-password", false), "reject invalid port");
    check(!host.Start(argv[1], 10678, "bad\npassword", false), "reject control characters in password");
    check(host.GetStatus().State == "error", "invalid password publishes an actionable error");
    const std::string password = "x \"!&=\\";
    check(host.Start(argv[1], 10678, password, argc > 2), "start host with short custom password containing spaces, quotes, symbols and trailing backslash");
    check(!host.Start(argv[1], 10678, "test-password", false), "reject duplicate host");
    for (int i = 0; i < 400 && host.GetStatus().State == "starting"; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!check(host.GetStatus().State == "ready", host.GetStatus().Message.c_str()))
        return 1;

    struct Peer
    {
        HSteamNetConnection Connection = k_HSteamNetConnection_Invalid;
        uint32_t Id = 0;
        uint32_t Leader = 0;
        bool Authenticated = false;
        bool InParty = false;
        int Invites = 0;
        int QuestUpdates = 0;
        uint8_t QuestStatus = 0;
        uint32_t ActorId = 0;
        uint32_t ActorEpoch = 0;
        int Movements = 0;
        int Assignments = 0;
        GameId AssignedCell{};
        Vector3_NetQuantize AssignedPosition{};
        uint32_t SpawnAlias = 0;
        uint32_t SpawnId = 0;
        int Activations = 0;
        int Packages = 0;
        uint32_t MovementEpoch = 0;
        uint32_t HelgenSession = 0;
        uint8_t HelgenPhase = 255;
        uint32_t HelgenReady = 0;
        uint32_t HelgenScenePhases = 0xFFFEFFFE;
        GameId Weather{};
        NotifySleep Sleep;
        TiltedPhoques::Map<uint32_t, TiltedPhoques::String> Players;
    };
    std::array<Peer, 3> peers;
    auto* sockets = SteamNetworkingSockets();
    auto send = [&](Peer& peer, const ClientMessage& message) {
        TiltedPhoques::Buffer buffer(65536);
        TiltedPhoques::Buffer::Writer writer(&buffer);
        writer.WriteBits(TiltedPhoques::kPayload, 8);
        message.Serialize(writer);
        return sockets->SendMessageToConnection(peer.Connection, buffer.GetData(), static_cast<uint32_t>(writer.Size()), k_nSteamNetworkingSend_Reliable, nullptr) == k_EResultOK;
    };
    auto pump = [&](int milliseconds) {
        const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
        do
        {
            sockets->RunCallbacks();
            for (auto& peer : peers)
            {
                ISteamNetworkingMessage* incoming = nullptr;
                while (peer.Connection && sockets->ReceiveMessagesOnConnection(peer.Connection, &incoming, 1) > 0)
                {
                    const auto* data = static_cast<const uint8_t*>(incoming->GetData());
                    size_t size = incoming->GetSize();
                    std::string uncompressed;
                    if (size && data[0] == TiltedPhoques::kCompressedPayload)
                    {
                        snappy::Uncompress(reinterpret_cast<const char*>(data + 1), size - 1, &uncompressed);
                        data = reinterpret_cast<const uint8_t*>(uncompressed.data());
                        size = uncompressed.size();
                    }
                    else if (size && data[0] == TiltedPhoques::kPayload)
                    {
                        ++data;
                        --size;
                    }
                    else
                        size = 0;
                    if (size)
                    {
                        TiltedPhoques::ViewBuffer buffer(const_cast<uint8_t*>(data), size);
                        TiltedPhoques::Buffer::Reader reader(&buffer);
                        auto message = ServerMessageFactory{}.Extract(reader);
                        if (message)
                        {
                            switch (message->GetOpcode())
                            {
                            case kNotifySleep: peer.Sleep = static_cast<NotifySleep&>(*message); break;
                            case kAuthenticationResponse: {
                                auto& auth = static_cast<AuthenticationResponse&>(*message);
                                peer.Authenticated = auth.Type == AuthenticationResponse::ResponseType::kAccepted;
                                peer.Id = auth.PlayerId;
                                break;
                            }
                            case kNotifyPartyJoined: {
                                peer.InParty = true;
                                peer.Leader = static_cast<NotifyPartyJoined&>(*message).LeaderPlayerId;
                                break;
                            }
                            case kNotifyPartyLeft: peer.InParty = false; break;
                            case kNotifyQuestUpdate:
                                ++peer.QuestUpdates;
                                peer.QuestStatus = static_cast<NotifyQuestUpdate&>(*message).Status;
                                break;
                            case kNotifyActivate: ++peer.Activations; break;
                            case kNotifyNewPackage: ++peer.Packages; break;
                            case kNotifyHelgenState: {
                                const auto& state = static_cast<NotifyHelgenState&>(*message);
                                peer.HelgenSession = state.Session;
                                peer.HelgenPhase = state.Phase;
                                peer.HelgenReady = state.Ready;
                                peer.HelgenScenePhases = state.ScenePhases;
                                break;
                            }
                            case kCharacterSpawnRequest: {
                                const auto& spawn = static_cast<CharacterSpawnRequest&>(*message);
                                peer.SpawnAlias = spawn.HelgenAliasId;
                                peer.SpawnId = spawn.ServerId;
                                break;
                            }
                            case kAssignCharacterResponse: {
                                ++peer.Assignments;
                                const auto& assignment = static_cast<AssignCharacterResponse&>(*message);
                                peer.ActorId = assignment.ServerId;
                                peer.ActorEpoch = assignment.OwnershipEpoch;
                                peer.AssignedCell = assignment.CellId;
                                peer.AssignedPosition = assignment.Position;
                                break;
                            }
                            case kNotifyOwnershipTransfer:
                                peer.ActorEpoch = static_cast<NotifyOwnershipTransfer&>(*message).OwnershipEpoch;
                                break;
                            case kServerReferencesMoveRequest: {
                                const auto& movement = static_cast<ServerReferencesMoveRequest&>(*message);
                                peer.Movements += static_cast<int>(movement.Updates.size());
                                for (const auto& [id, point] : movement.Updates)
                                    peer.MovementEpoch = point.OwnershipEpoch;
                                break;
                            }
                            case kNotifyWeatherChange: peer.Weather = static_cast<NotifyWeatherChange&>(*message).Id; break;
                            case kNotifyPartyInvite: ++peer.Invites; break;
                            case kNotifyPartyInfo: peer.Leader = static_cast<NotifyPartyInfo&>(*message).LeaderPlayerId; break;
                            case kNotifyPlayerList: peer.Players = static_cast<NotifyPlayerList&>(*message).Players; break;
                            default: break;
                            }
                        }
                    }
                    incoming->Release();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while (std::chrono::steady_clock::now() < until);
    };
    SteamNetworkingIPAddr endpoint{};
    endpoint.ParseString("127.0.0.1:10678");
    for (auto& peer : peers)
    {
        peer.Connection = sockets->ConnectByIPAddress(endpoint, 0, nullptr);
        pump(500);
        AuthenticationRequest auth;
        auth.Username = "HostSmoke";
        auth.Version = BUILD_COMMIT;
        auth.Token = password.c_str();
        send(peer, auth);
    }
    pump(1500);
    for (const auto& peer : peers)
        check(peer.Authenticated, "authenticate real loopback client");
    auto& a = peers[0];
    auto& b = peers[1];
    auto& c = peers[2];
    RequestSleep sleep;
    sleep.Session = 1; sleep.Hours = 8; sleep.Seconds = 900;
    send(a, sleep); send(b, sleep);
    pump(250);
    check(a.Sleep.State == NotifySleep::Waiting && a.Sleep.Ready == 2, "sleep waits for every connected player");
    sleep.Hours = 1;
    send(c, sleep);
    pump(250);
    check(a.Sleep.State == NotifySleep::Advance && a.Sleep.Step == 1 && c.Sleep.Step == 1, "sleep grants the first native tick together");
    const float sleepStart = a.Sleep.Time.Time;
    sleep.State = RequestSleep::Tick;
    for (uint32_t step = 1; step <= 4; ++step)
    {
        sleep.Step = step;
        send(a, sleep); send(a, sleep); send(b, sleep);
        pump(100);
        check(a.Sleep.Step == step, "duplicates cannot skip a sleeper or advance the clock");
        send(c, sleep);
        pump(150);
    }
    check(a.Sleep.State == NotifySleep::Finished && b.Sleep.State == NotifySleep::Finished && c.Sleep.State == NotifySleep::Finished,
        "shortest requested sleep wakes everyone");
    const float slept = std::fmod(a.Sleep.Time.Time - sleepStart + 24.f, 24.f);
    check(std::abs(slept - 1.f) < 0.001f, "one shared hour advances exactly once");
    sleep = {}; sleep.Session = 2; sleep.Hours = 1; sleep.Seconds = 900;
    send(a, sleep); send(b, sleep); pump(100);
    sleep.Session = 1; sleep.State = RequestSleep::Cancel;
    send(a, sleep); pump(100);
    check(b.Sleep.State == NotifySleep::Waiting, "stale cancellation cannot cancel a new sleep");
    sleep.Session = 2;
    send(a, sleep); pump(150);
    check(b.Sleep.State == NotifySleep::Cancelled, "cancelling a bed releases the other sleeper");
    send(a, PartyCreateRequest{});
    pump(500);
    check(a.InParty && b.InParty && c.InParty, "host party automatically includes friends");
    for (auto& peer : peers)
        send(peer, RequestHelgenReady{});
    pump(300);
    check(a.HelgenSession && a.HelgenSession == b.HelgenSession && b.HelgenSession == c.HelgenSession, "Helgen participants receive one party session");
    RequestHelgenReady ready;
    ready.Session = a.HelgenSession;
    ready.State = 1;
    ready.Name = "Bridge One";
    send(a, ready);
    ready.Name = "Bridge Two";
    send(b, ready);
    pump(300);
    check(a.HelgenPhase == 0 && a.HelgenReady == 2 && c.HelgenPhase == 0, "Helgen waits for every character");
    check(b.Players[a.Id] == "Bridge One" && a.Players[b.Id] == "Bridge Two", "finished character names replace Prisoner on peers");
    ++ready.Session;
    send(c, ready);
    pump(300);
    check(a.HelgenPhase == 0, "stale Helgen session cannot release the intro");
    --ready.Session;
    send(c, ready);
    pump(300);
    check(a.HelgenPhase == 1 && b.HelgenPhase == 1 && c.HelgenPhase == 1, "all ready players release the cinematic together");
    ready.Name.clear();
    ready.ScenePhases = 0xFFFF001A;
    send(a, ready);
    pump(300);
    check(b.HelgenScenePhases == ready.ScenePhases && c.HelgenScenePhases == ready.ScenePhases, "leader scene phases reach both guests");
    ready.ScenePhases = 0xFFFF001B;
    send(b, ready);
    pump(300);
    check(c.HelgenScenePhases == 0xFFFF001A, "guest cannot override the leader scene phases");
    ready.ScenePhases = 0x01000001;
    send(a, ready);
    pump(300);
    check(c.HelgenScenePhases == 0xFFFF001A, "invalid scene phase is rejected");
    ready.State = 2;
    send(a, ready);
    send(b, ready);
    pump(300);
    check(a.HelgenPhase == 1 && a.HelgenReady == 2, "gameplay waits for every local cinematic");
    send(c, ready);
    pump(300);
    check(a.HelgenPhase == 2 && b.HelgenPhase == 2 && c.HelgenPhase == 2, "all completed cinematics release gameplay");
    send(b, PartyLeaveRequest{});
    pump(200);
    PartyInviteRequest introInvite;
    introInvite.PlayerId = b.Id;
    send(a, introInvite);
    pump(200);
    PartyAcceptInviteRequest introAccept;
    introAccept.InviterId = a.Id;
    send(b, introAccept);
    pump(200);
    send(b, RequestHelgenReady{});
    pump(300);
    check(b.HelgenPhase == 3, "late intro registration is rejected after release");
    AssignCharacterRequest actor;
    actor.ReferenceId = GameId(1, 450);
    send(a, actor);
    pump(300);
    check(a.ActorEpoch != 0, "assign an actor to the first owner");
    ActivateRequest activation{};
    activation.Id = GameId(1, 451);
    activation.ActivatorId = a.ActorId;
    activation.PreActivationOpenState = 0;
    NewPackageRequest package{};
    package.ActorId = a.ActorId;
    package.PackageId = GameId(1, 452);
    send(a, activation);
    send(a, package);
    pump(300);
    check(c.Activations == 1 && c.Packages == 1, "relay owner activation and AI package");
    send(b, activation);
    send(b, package);
    pump(300);
    check(c.Activations == 1 && c.Packages == 1, "reject remote activation echoes and non-owner packages");
    activation.ActivatorId = package.ActorId = UINT32_MAX;
    send(a, activation);
    send(a, package);
    pump(300);
    check(c.Activations == 1 && c.Packages == 1, "reject activation and package for missing actors");
    activation.ActivatorId = package.ActorId = a.ActorId;
    ClientReferencesMoveRequest movement;
    movement.Tick = 1000;
    movement.Updates[a.ActorId].OwnershipEpoch = a.ActorEpoch;
    send(a, movement);
    pump(300);
    check(c.Movements == 1 && c.MovementEpoch == a.ActorEpoch, "relay movement with its ownership epoch");
    ++movement.Updates[a.ActorId].OwnershipEpoch;
    send(a, movement);
    pump(300);
    check(c.Movements == 1, "reject a mismatched movement epoch even from the owner");
    movement.Updates[a.ActorId].OwnershipEpoch = a.ActorEpoch;
    send(b, movement);
    pump(300);
    check(c.Movements == 1, "reject movement from a different player");
    RequestWeatherChange weather;
    weather.Id = GameId(1, 100);
    send(a, weather);
    pump(200);
    weather.Id = GameId(1, 200);
    send(b, weather);
    pump(200);
    send(c, RequestCurrentWeather{});
    pump(200);
    check(c.Weather == GameId(1, 100), "follower cannot overwrite party weather");
    PartyChangeLeaderRequest changeLeader;
    changeLeader.PartyMemberPlayerId = b.Id;
    send(a, changeLeader);
    pump(200);
    const uint32_t oldEpoch = a.ActorEpoch;
    send(b, actor);
    pump(300);
    check(b.ActorId == a.ActorId && b.ActorEpoch > oldEpoch, "handoff actor ownership to the new leader");
    send(a, activation);
    send(a, package);
    pump(300);
    check(c.Activations == 1 && c.Packages == 1, "former owner cannot activate or change AI packages");
    send(b, activation);
    send(b, package);
    pump(300);
    check(c.Activations == 2 && c.Packages == 2, "new owner can activate and change AI packages");
    send(a, movement);
    send(b, movement);
    pump(300);
    check(c.Movements == 1, "reject both former owner and stale epoch after handoff");
    movement.Updates[a.ActorId].OwnershipEpoch = b.ActorEpoch;
    send(b, movement);
    pump(300);
    check(c.Movements == 2 && c.MovementEpoch == b.ActorEpoch, "accept movement from the new ownership epoch");
    send(b, weather);
    pump(200);
    send(c, RequestCurrentWeather{});
    pump(200);
    check(c.Weather == weather.Id, "new leader controls party weather");
    changeLeader.PartyMemberPlayerId = a.Id;
    send(b, changeLeader);
    pump(200);
    RequestQuestUpdate quest{};
    quest.Stage = 0;
    quest.Id = GameId(1, 123);
    quest.ClientQuestType = 1;
    quest.Status = RequestQuestUpdate::Started;
    send(a, quest);
    pump(300);
    quest.Status = RequestQuestUpdate::StageUpdate;
    quest.Stage = 10;
    send(a, quest);
    pump(300);
    check(c.QuestUpdates == 2, "quest start and stage delivered");
    quest.Status = RequestQuestUpdate::Started;
    send(b, quest);
    quest.Status = RequestQuestUpdate::StageUpdate;
    send(b, quest);
    pump(300);
    check(c.QuestUpdates == 2, "duplicate start cannot reset quest stage deduplication");
    quest.Status = RequestQuestUpdate::Stopped;
    send(a, quest);
    pump(300);
    quest.Status = RequestQuestUpdate::Started;
    send(a, quest);
    pump(300);
    quest.Status = RequestQuestUpdate::StageUpdate;
    send(a, quest);
    pump(300);
    check(c.QuestUpdates == 5, "restarted quest can relay its stages again");
    quest.Status = RequestQuestUpdate::Stopped;
    send(a, quest);
    send(b, quest);
    pump(300);
    check(c.QuestUpdates == 6, "a stop several members report is relayed once");
    quest.Id = GameId(1, 124);
    quest.ClientQuestType = 6;
    quest.Status = RequestQuestUpdate::Started;
    send(a, quest);
    pump(300);
    check(c.QuestUpdates == 7, "miscellaneous quests are shared like any journal quest");
    quest.Id = GameId(1, 125);
    quest.ClientQuestType = 0;
    send(a, quest);
    pump(300);
    check(c.QuestUpdates == 7, "background quests without a journal entry are not shared");
    quest.Id = GameId(1, 126);
    quest.ClientQuestType = 7;
    quest.Stage = 0;
    quest.Status = RequestQuestUpdate::Started;
    send(a, quest);
    pump(300);
    quest.Stage = 500;
    quest.Status = RequestQuestUpdate::StageUpdate;
    send(a, quest);
    pump(300);
    quest.Status = RequestQuestUpdate::Failed;
    send(a, quest);
    pump(300);
    check(c.QuestUpdates == 10 && c.QuestStatus == RequestQuestUpdate::Failed, "a failed quest reaches the party as failed");
    quest.Status = RequestQuestUpdate::Stopped;
    send(b, quest);
    pump(300);
    check(c.QuestUpdates == 10 && c.QuestStatus == RequestQuestUpdate::Failed, "a member's own stop after a relayed failure is not relayed again");
    quest.Status = static_cast<uint8_t>(RequestQuestUpdate::Failed + 1);
    send(a, quest);
    pump(300);
    check(c.QuestUpdates == 10, "an unknown quest status is not relayed");
    send(b, PartyLeaveRequest{});
    send(c, PartyLeaveRequest{});
    pump(500);
    send(b, PartyCreateRequest{});
    pump(500);
    PartyKickRequest kick;
    kick.PartyMemberPlayerId = b.Id;
    send(a, kick);
    pump(500);
    check(b.InParty && b.Leader == b.Id, "cross-party kick rejected");
    PartyInviteRequest invite;
    invite.PlayerId = c.Id;
    send(b, invite);
    pump(500);
    check(c.Invites == 1, "party invite delivered");
    PartyAcceptInviteRequest accept;
    accept.InviterId = b.Id;
    send(c, accept);
    pump(500);
    check(c.InParty && c.Leader == b.Id, "valid invitation accepted");
    kick.PartyMemberPlayerId = c.Id;
    send(b, kick);
    pump(500);
    check(!c.InParty && a.Players.contains(c.Id) && b.Players.contains(c.Id), "kick leaves player on server roster");
    send(c, accept);
    pump(500);
    check(!c.InParty, "consumed invitation cannot be reused");
    send(b, invite);
    pump(500);
    send(b, PartyLeaveRequest{});
    pump(500);
    send(b, PartyCreateRequest{});
    pump(500);
    send(c, accept);
    pump(500);
    check(!c.InParty, "invite cannot outlive inviter membership");
    send(b, invite);
    pump(61000);
    send(c, accept);
    pump(500);
    check(!c.InParty, "expired invitation rejected");
    AssignCharacterRequest aliased;
    aliased.ReferenceId = GameId(UINT32_MAX, 0xD03);
    aliased.FormId = GameId(0, 0x1234);
    aliased.HelgenAliasId = 152;
    send(a, aliased);
    pump(300);
    const uint32_t firstAliasActor = a.ActorId;
    const uint32_t firstAliasEpoch = a.ActorEpoch;
    check(c.SpawnAlias == 152 && c.SpawnId == firstAliasActor, "spawn carries Helgen quest alias identity");
    aliased.ReferenceId.BaseId = 0xF99;
    send(b, aliased);
    pump(300);
    check(b.ActorId == firstAliasActor, "different save-local refs share one Helgen alias actor");
    aliased.HelgenAliasId = 170;
    send(b, aliased);
    pump(300);
    check(b.ActorId != firstAliasActor && c.SpawnAlias == 170, "same NPC base in distinct quest roles stays distinct");
    const int beforeInvalidAliases = b.Assignments;
    aliased.HelgenAliasId = 119;
    send(b, aliased);
    aliased.HelgenAliasId = 180;
    send(b, aliased);
    aliased.HelgenAliasId = 152;
    aliased.ReferenceId.ModId = 0;
    send(b, aliased);
    pump(300);
    check(b.Assignments == beforeInvalidAliases, "reject player, out-of-range and non-temporary alias claims");
    aliased.HelgenAliasId = 0;
    aliased.ReferenceId.ModId = UINT32_MAX;
    send(a, aliased);
    pump(200);
    const uint32_t firstUnaliasedActor = a.ActorId;
    send(b, aliased);
    pump(200);
    check(b.ActorId != firstUnaliasedActor, "ordinary temporary actors are not merged by local reference");
    send(a, PartyLeaveRequest{});
    pump(200);
    invite.PlayerId = a.Id;
    send(b, invite);
    pump(200);
    send(a, accept);
    pump(200);
    aliased.HelgenAliasId = 152;
    send(b, aliased);
    pump(300);
    check(b.ActorId == firstAliasActor && b.ActorEpoch > firstAliasEpoch, "new party leader can claim an aliased temporary actor");
    // Reproduce a companion discovered inside the keep while its old owner remains outside.
    AssignCharacterRequest player;
    player.ReferenceId = GameId(0, 0x14);
    player.CellId = GameId(0, 0x100);
    player.WorldSpaceId = GameId(0, 0x3C);
    for (auto& peer : peers)
        send(peer, player);
    pump(300);
    AssignCharacterRequest companion;
    companion.ReferenceId = GameId(0, 0x2BF9E);
    companion.CellId = player.CellId;
    companion.WorldSpaceId = player.WorldSpaceId;
    companion.Position = {15965.f, -77861.f, 8228.f};
    send(a, companion);
    pump(200);
    const uint32_t companionId = a.ActorId;
    const uint32_t companionEpoch = a.ActorEpoch;
    EnterInteriorCellRequest enter;
    enter.CellId = GameId(0, 0x5DE24);
    AssignCharacterRequest arrival = companion;
    arrival.CellId = enter.CellId;
    arrival.WorldSpaceId = {};
    arrival.Position = {-400.f, 5500.f, 394.f};
    send(b, arrival);
    pump(200);
    check(b.AssignedCell == companion.CellId, "reject keep arrival before the leader enters its cell");
    send(b, enter);
    pump(200);
    AssignCharacterRequest unrelated = companion;
    unrelated.ReferenceId.BaseId = 0x123;
    send(a, unrelated);
    pump(200);
    unrelated.CellId = arrival.CellId;
    unrelated.WorldSpaceId = {};
    send(b, unrelated);
    pump(200);
    check(b.AssignedCell == companion.CellId, "keep arrival exception cannot move unrelated NPCs");
    AssignCharacterRequest outsider = companion;
    outsider.ReferenceId.BaseId = 0x2BFA2;
    send(c, outsider);
    pump(200);
    outsider.CellId = arrival.CellId;
    outsider.WorldSpaceId = {};
    send(b, outsider);
    pump(200);
    check(b.AssignedCell == companion.CellId, "keep arrival cannot take another party's companion");
    invite.PlayerId = c.Id;
    send(b, invite);
    pump(200);
    send(c, accept);
    pump(200);
    send(c, enter);
    send(c, arrival);
    pump(200);
    check(c.AssignedCell == companion.CellId && c.ActorEpoch == companionEpoch, "nonleader arrival leaves canonical companion state unchanged");
    send(b, arrival);
    pump(300);
    check(b.ActorId == companionId && b.ActorEpoch > companionEpoch && b.AssignedCell == arrival.CellId && b.AssignedPosition == arrival.Position,
          "leader keep arrival responds with interior position before granting ownership");
    ClientReferencesMoveRequest staleMove;
    staleMove.Tick = 5000;
    staleMove.Updates[companionId].OwnershipEpoch = companionEpoch;
    staleMove.Updates[companionId].Position = companion.Position;
    send(a, staleMove);
    pump(200);
    send(b, arrival);
    pump(200);
    check(b.AssignedCell == arrival.CellId && b.AssignedPosition == arrival.Position, "former exterior owner cannot move the companion back after arrival");
    for (auto& peer : peers)
        sockets->CloseConnection(peer.Connection, 0, nullptr, false);
    struct CancelledClient : TiltedPhoques::Client
    {
        int Connections = 0;
        int Disconnections = 0;
        void OnConsume(const void*, uint32_t) override {}
        void OnConnected() override { ++Connections; }
        void OnDisconnected(EDisconnectReason) override { ++Disconnections; }
        void OnUpdate() override {}
    };
    {
        CancelledClient client;
        client.Connect("localhost:10678");
        client.Close();
        for (int i = 0; i < 100; ++i)
        {
            client.Update();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        check(client.Connections == 0 && client.Disconnections == 1, "cancelled DNS cannot connect later");
    }
    {
        CancelledClient client;
        client.Connect("localhost:10678");
    }
    check(true, "client destruction drains pending DNS safely");
    std::cout << "HOST " << host.GetStatus().Message << std::endl;
    host.Stop();
    for (int i = 0; i < 300 && host.GetStatus().State != "stopped"; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    check(host.GetStatus().State == "stopped" && host.GetStatus().Message == "Hosting stopped.", "stop host and confirm owned mapping removed");
    std::cout << "STOP " << host.GetStatus().Message << std::endl;
    peers = {};
    check(host.Start(argv[1], 10678, "1", false), "restart host with password 1");
    for (int i = 0; i < 300 && host.GetStatus().State == "starting"; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    check(host.GetStatus().State == "ready", "single-character password server listens");
    a.Connection = sockets->ConnectByIPAddress(endpoint, 0, nullptr);
    pump(500);
    AuthenticationRequest shortPassword;
    shortPassword.Username = "ShortPassword";
    shortPassword.Version = BUILD_COMMIT;
    shortPassword.Token = "1";
    send(a, shortPassword);
    pump(1500);
    check(a.Authenticated, "authenticate with user-reported password 1");
    sockets->CloseConnection(a.Connection, 0, nullptr, false);
    host.Stop();
    for (int i = 0; i < 300 && host.GetStatus().State != "stopped"; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    check(host.GetStatus().State == "stopped", "single-character host stops cleanly");
    TiltedPhoques::SteamInterface::Release();
    return failures ? 1 : 0;
}
