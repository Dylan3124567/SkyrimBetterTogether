#include <TiltedOnlinePCH.h>

#include <OverlayRenderHandler.hpp>

#include <Services/OverlayClient.h>
#include <Services/TransportService.h>
#include <Services/HelgenService.h>

#include <Messages/SendChatMessageRequest.h>
#include <Messages/TeleportRequest.h>

#include <Events/SetTimeCommandEvent.h>

#include <World.h>
#include <TiltedCore/Filesystem.hpp>

OverlayClient::OverlayClient(TransportService& aTransport, TiltedPhoques::OverlayRenderHandler* apHandler)
    : TiltedPhoques::OverlayClient(apHandler)
    , m_transport(aTransport)
{
}

OverlayClient::~OverlayClient() noexcept
{
}

bool OverlayClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message)
{
    if (message->GetName() == "ui-event")
    {
        auto pArguments = message->GetArgumentList();

        auto eventName = pArguments->GetString(0).ToString();
        auto eventArgs = pArguments->GetList(1);

        if (!eventArgs)
            return false;

#ifndef PUBLIC_BUILD
        LOG(INFO) << "event=ui_event name=" << eventName;
#endif

        if (eventName == "connect")
            ProcessConnectMessage(eventArgs);
        else if (eventName == "deactivate")
            World::Get().GetRunner().Queue([] { World::Get().GetOverlayService().SetActive(false); });
        else if (eventName == "startHosting")
        {
            const int port = eventArgs->GetInt(0);
            const auto password = eventArgs->GetString(1).ToString();
            const bool internet = eventArgs->GetBool(2);
            if (port < 1 || port > 65535)
                return false;
            World::Get().GetRunner().Queue([port, password, internet] {
                auto& helgen = World::Get().ctx().at<HelgenService>();
                if (!helgen.PrepareConnection())
                    return;
                if (!World::Get().GetTransport().IsConnected())
                    if (!World::Get().GetOverlayService().GetHostSession().Start(TiltedPhoques::GetPath() / "SkyrimTogetherServer.exe",
                                                                              static_cast<uint16_t>(port), password, internet))
                        helgen.Cancel();
            });
        }
        else if (eventName == "stopHosting")
            World::Get().GetRunner().Queue([] {
                World::Get().GetOverlayService().GetHostSession().Stop();
                World::Get().ctx().at<HelgenService>().Cancel();
                World::Get().GetTransport().Close();
            });
        else if (eventName == "disconnect")
            ProcessDisconnectMessage();
        else if (eventName == "revealPlayers")
            ProcessRevealPlayersMessage();
        else if (eventName == "sendMessage")
            ProcessChatMessage(eventArgs);
        else if (eventName == "setTime")
            ProcessSetTimeCommand(eventArgs);
        else if (eventName == "launchParty")
            World::Get().GetRunner().Queue([] { World::Get().GetPartyService().CreateParty(); });
        else if (eventName == "leaveParty")
            World::Get().GetRunner().Queue([] { World::Get().GetPartyService().LeaveParty(); });
        else if (eventName == "createPartyInvite")
        {
            uint32_t aPlayerId = eventArgs->GetInt(0);
            World::Get().GetRunner().Queue([aPlayerId] { World::Get().GetPartyService().CreateInvite(aPlayerId); });
        }
        else if (eventName == "acceptPartyInvite")
        {
            uint32_t aInviterId = eventArgs->GetInt(0);
            // push to main thread because the party service has to check validity of invite thread safely
            World::Get().GetRunner().Queue([aInviterId]() { World::Get().GetPartyService().AcceptInvite(aInviterId); });
        }
        else if (eventName == "kickPartyMember")
        {
            uint32_t aPlayerId = eventArgs->GetInt(0);
            World::Get().GetRunner().Queue([aPlayerId] { World::Get().GetPartyService().KickPartyMember(aPlayerId); });
        }
        else if (eventName == "changePartyLeader")
        {
            uint32_t aPlayerId = eventArgs->GetInt(0);
            World::Get().GetRunner().Queue([aPlayerId] { World::Get().GetPartyService().ChangePartyLeader(aPlayerId); });
        }
        else if (eventName == "teleportToPlayer")
            ProcessTeleportMessage(eventArgs);
        else if (eventName == "toggleDebugUI")
            ProcessToggleDebugUI();
        else if (eventName == "openPlayGuide")
            ProcessOpenPlayGuide();

        return true;
    }

    return false;
}

void OverlayClient::ProcessConnectMessage(CefRefPtr<CefListValue> aEventArgs)
{
    std::string baseIp = aEventArgs->GetString(0);
    if (baseIp == "localhost")
    {
        baseIp = "127.0.0.1";
    }

    const int port = aEventArgs->GetInt(1);
    if (port < 1 || port > 65535 || baseIp.empty() || baseIp.size() > 253 || baseIp.find_first_of(" \t\r\n/\\") != std::string::npos)
        return;
    const auto password = aEventArgs->GetString(2).ToString();

    std::string endpoint = baseIp + ":" + std::to_string(port);

    World::Get().GetRunner().Queue([endpoint, password] {
        World::Get().GetTransport().SetServerPassword(password);
        if (!World::Get().GetTransport().Connect(endpoint))
            World::Get().GetTransport().OnDisconnected(Client::kLocalProblem);
    });
}

void OverlayClient::ProcessDisconnectMessage()
{
    World::Get().GetRunner().Queue([]() {
        World::Get().ctx().at<HelgenService>().Cancel();
        World::Get().GetTransport().Close();
    });
}

void OverlayClient::ProcessRevealPlayersMessage()
{
    SetUIVisible(false);
    World::Get().GetRunner().Queue([] { World::Get().GetMagicService().StartRevealingOtherPlayers(); });
}

void OverlayClient::ProcessChatMessage(CefRefPtr<CefListValue> aEventArgs)
{
    std::string contents = aEventArgs->GetString(1).ToString();
    if (!contents.empty())
    {
        SendChatMessageRequest messageRequest;
        messageRequest.MessageType = static_cast<ChatMessageType>(aEventArgs->GetInt(0));
        messageRequest.ChatMessage = contents;

        World::Get().GetRunner().Queue([messageRequest] { World::Get().GetTransport().Send(messageRequest); });
    }
}

void OverlayClient::ProcessSetTimeCommand(CefRefPtr<CefListValue> aEventArgs)
{
    const int hours = aEventArgs->GetInt(0);
    const int minutes = aEventArgs->GetInt(1);
    if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59)
        return;
    World::Get().GetRunner().Queue([hours, minutes] {
        World::Get().GetDispatcher().trigger(SetTimeCommandEvent(hours, minutes, World::Get().GetTransport().GetLocalPlayerId()));
    });
}

void OverlayClient::ProcessTeleportMessage(CefRefPtr<CefListValue> aEventArgs)
{
    TeleportRequest request{};
    request.PlayerId = aEventArgs->GetInt(0);

    World::Get().GetRunner().Queue([request] { World::Get().GetTransport().Send(request); });
}

void OverlayClient::ProcessToggleDebugUI()
{
    World::Get().GetRunner().Queue([] { World::Get().GetDebugService().m_showDebugStuff = !World::Get().GetDebugService().m_showDebugStuff; });
}

void OverlayClient::ProcessOpenPlayGuide()
{
    ShellExecuteW(nullptr, L"open", LR"(https://wiki.tiltedphoques.com/tilted-online/general-information/playguide)", nullptr, nullptr, SW_SHOWNORMAL);
}

void OverlayClient::SetUIVisible(bool aVisible) noexcept
{
    World::Get().GetRunner().Queue([aVisible] { World::Get().GetOverlayService().SetActive(aVisible); });
}
