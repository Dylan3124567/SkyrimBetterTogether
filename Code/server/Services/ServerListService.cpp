#include <Events/PlayerJoinEvent.h>
#include <Events/PlayerLeaveEvent.h>
#include <Events/UpdateEvent.h>
#include <GameServer.h>
#include <Services/ServerListService.h>

#include <console/Setting.h>
#include <future>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

extern Console::Setting<uint32_t> uMaxPlayerCount;

static constexpr char kMasterServerEndpoint[] =
    //"http://127.0.0.1:8000";
    "https://skyrim-reborn-list.skyrim-together.com";

extern Console::Setting<bool> bAnnounceServer;

ServerListService::ServerListService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
    , m_updateConnection(aDispatcher.sink<UpdateEvent>().connect<&ServerListService::OnUpdate>(this))
    , m_nextAnnounce(std::chrono::seconds(0))
{
    if (!bAnnounceServer)
        spdlog::warn("bAnnounceServer is set to false. The server will not show up as a public server. "
                     "If you are just playing with friends, this is probably what you want.");
}

void ServerListService::OnUpdate(const UpdateEvent& acEvent) noexcept
{
    if (m_announcement.valid() && m_announcement.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        if (m_announcement.get())
            GameServer::Get()->Kill();
    }
    if (m_nextAnnounce < std::chrono::steady_clock::now())
    {
        Announce();

        m_nextAnnounce = (std::chrono::steady_clock::now() + std::chrono::minutes(1));
    }
}

void ServerListService::OnPlayerJoin(const PlayerJoinEvent& acEvent) noexcept
{
    Announce();

    m_nextAnnounce = (std::chrono::steady_clock::now() + std::chrono::minutes(1));
}

void ServerListService::OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept
{
    Announce();

    m_nextAnnounce = (std::chrono::steady_clock::now() + std::chrono::minutes(1));
}

void ServerListService::Announce() noexcept
{
    // Private sessions do not need a public-list heartbeat. Snapshot on the server thread.
    if (!bAnnounceServer || m_announcement.valid())
        return;
    auto* pServer = GameServer::Get();
    const auto info = pServer->GetInfo();
    const auto port = pServer->GetPort();
    const auto count = static_cast<uint16_t>(m_world.GetPlayerManager().Count());
    const auto maxCount = uMaxPlayerCount.value_as<uint16_t>();
    const bool password = pServer->IsPasswordProtected();
    m_announcement = std::async(std::launch::async, [info, port, count, maxCount, password] {
        return PostAnnouncement(info.name, info.desc, info.icon_url, port, info.tick_rate, count, maxCount,
                                info.tagList, true, password, kIsPublic | (password ? kHasPassword : 0));
    });
}

bool ServerListService::PostAnnouncement(String acName, String acDesc, String acIconUrl, uint16_t aPort, uint16_t aTick,
                                         uint16_t aPlayerCount, uint16_t aPlayerMaxCount, String acTagList,
                                         bool aPublic, bool aPassword, int32 aFlags) noexcept
{
    const std::string kVersion{BUILD_COMMIT};
    const httplib::Params params{
        {"name", std::string(acName.c_str(), acName.size())},
        {"desc", std::string(acDesc.c_str(), acDesc.size())},
        {"icon_url", std::string(acIconUrl.c_str(), acIconUrl.size())},
        {"version", std::string(kVersion.c_str(), kVersion.size())},
        {"port", std::to_string(aPort)},
        {"tick", std::to_string(aTick)},
        {"player_count", std::to_string(aPlayerCount)},
        {"max_player_count", std::to_string(aPlayerMaxCount)},
        {"tags", std::string(acTagList.c_str(), acTagList.size())},
        {"public", aPublic ? "true" : "false"},
        {"pass", aPassword ? "true" : "false"},
        {"flags", std::to_string(aFlags)},
    };

    httplib::Client client(kMasterServerEndpoint);
    client.set_connection_timeout(std::chrono::seconds(5));
    client.set_read_timeout(std::chrono::seconds(5));
    client.set_write_timeout(std::chrono::seconds(5));
    const auto response = client.Post("/announce", params);

    // If we send a 403 it means we banned this server
    if (response)
    {
        if (response->status == 403)
            return true;
        else if (response->status != 200)
            spdlog::error("Server list error! {}", response->body);
    }
    else
    {
        spdlog::error("Server could not reach the server list! {}", to_string(response.error()));
    }
    return false;
}
