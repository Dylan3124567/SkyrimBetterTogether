#include <Services/HostSession.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include <string_view>

namespace
{
// Quote one Windows argv value, including quotes and trailing backslashes.
std::wstring PasswordArgument(const std::string& aPassword)
{
    std::wstring result = L"\"-GameServer:sPassword=";
    size_t slashes = 0;
    for (const unsigned char ch : aPassword)
    {
        if (ch == '\\')
            ++slashes;
        else
        {
            result.append(slashes * (ch == '"' ? 2 : 1) + (ch == '"' ? 1 : 0), L'\\');
            result += ch;
            slashes = 0;
        }
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}

std::string GetLanAddress()
{
    DWORD index = 0;
    if (GetBestInterface(htonl(0x01010101), &index) != NO_ERROR)
        return {};
    ULONG size = 0;
    if (GetIpAddrTable(nullptr, &size, FALSE) != ERROR_INSUFFICIENT_BUFFER)
        return {};
    std::vector<unsigned char> buffer(size);
    auto* table = reinterpret_cast<MIB_IPADDRTABLE*>(buffer.data());
    if (GetIpAddrTable(table, &size, FALSE) != NO_ERROR)
        return {};
    for (DWORD i = 0; i < table->dwNumEntries; ++i)
        if (table->table[i].dwIndex == index && table->table[i].dwAddr)
        {
            in_addr address{};
            address.s_addr = table->table[i].dwAddr;
            char text[INET_ADDRSTRLEN]{};
            return InetNtopA(AF_INET, &address, text, sizeof(text)) ? text : "";
        }
    return {};
}

bool IsListening(DWORD aProcess, uint16_t aPort)
{
    ULONG size = 0;
    if (GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) != ERROR_INSUFFICIENT_BUFFER)
        return false;
    std::vector<unsigned char> buffer(size);
    auto* pTable = reinterpret_cast<MIB_UDPTABLE_OWNER_PID*>(buffer.data());
    if (GetExtendedUdpTable(pTable, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) != NO_ERROR)
        return false;
    for (DWORD i = 0; i < pTable->dwNumEntries; ++i)
        if (pTable->table[i].dwOwningPid == aProcess && ntohs(static_cast<u_short>(pTable->table[i].dwLocalPort)) == aPort)
            return true;
    return false;
}
} // namespace

HostSession::~HostSession()
{
    Stop();
    // Join before destroying the mutex/status accessed by the worker.
    if (m_worker.joinable())
        m_worker.join();
}

HostSession::Status HostSession::GetStatus()
{
    std::lock_guard lock(m_mutex);
    return m_status;
}

void HostSession::Publish(const Status& aStatus)
{
    std::lock_guard lock(m_mutex);
    const auto revision = m_status.Revision + 1;
    m_status = aStatus;
    m_status.Revision = revision;
}

bool HostSession::Start(const std::filesystem::path& aServer, uint16_t aPort, const std::string& aPassword, bool aInternet)
{
    if (!m_finished)
        return false;
    if (aPort == 0 || aPassword.empty() || aPassword.size() > 64 ||
        std::any_of(aPassword.begin(), aPassword.end(), [](unsigned char ch) { return ch < 32 || ch > 126; }))
    {
        Publish({"error", "Use a port from 1 to 65535 and a password of 1-64 English letters, numbers, spaces or symbols.", {}, {}, aPort});
        return false;
    }
    if (m_worker.joinable())
        m_worker.join();
    m_finished = false;
    Publish({"starting", "Starting your private server...", {}, {}, aPort});
    m_worker = std::jthread([=, this](std::stop_token aStop) { Run(aStop, aServer, aPort, aPassword, aInternet); });
    return true;
}

void HostSession::Stop()
{
    if (m_worker.joinable())
        m_worker.request_stop();
}

void HostSession::Run(std::stop_token aStop, std::filesystem::path aServer, uint16_t aPort, std::string aPassword, bool aInternet)
{
    Status status{"starting", {}, {}, {}, aPort};
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    PROCESS_INFORMATION process{};
    UPNPUrls urls{};
    IGDdatas data{};
    char lan[64]{}, wan[64]{};
    const std::string port = std::to_string(aPort);
    const std::string description = "SkyrimTogether-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64());
    bool ownsMapping = false;
    auto mapping = [&](bool& aOurs, bool* aReusable = nullptr) {
        char client[16]{}, internalPort[6]{}, desc[80]{}, enabled[4]{}, lease[16]{};
        const int result = UPNP_GetSpecificPortMappingEntry(urls.controlURL, data.first.servicetype, port.c_str(), "UDP", nullptr,
                                                           client, internalPort, desc, enabled, lease);
        const bool sameTarget = result == 0 && std::strcmp(client, lan) == 0 && port == internalPort;
        aOurs = sameTarget && description == desc;
        if (aReusable)
            *aReusable = sameTarget && std::string_view(desc).starts_with("SkyrimTogether-");
        return result;
    };

    // All paths converge here so partial starts, cancellation and crashes of the child are cleaned up.
    auto run = [&] {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
        {
            status.Message = "Could not create the host process group.";
            return;
        }
        std::wstring command = L"\"" + aServer.wstring() + L"\" -bConsole=false -LiveServices:bAnnounceServer=false -GameServer:uPort=" +
                               std::to_wstring(aPort) + L" -Gameplay:bAutoPartyJoin=true " + PasswordArgument(aPassword);
        STARTUPINFOW startup{sizeof(startup)};
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        if (!CreateProcessW(aServer.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                            nullptr, aServer.parent_path().c_str(), &startup, &process))
        {
            status.Message = "Could not start SkyrimTogetherServer.exe (Windows error " + std::to_string(GetLastError()) + "). Reinstall the complete package.";
            return;
        }
        if (!AssignProcessToJobObject(job, process.hProcess))
        {
            TerminateProcess(process.hProcess, 1);
            status.Message = "Could not manage the host process. Server stopped.";
            return;
        }
        ResumeThread(process.hThread);
        const auto deadline = GetTickCount64() + 30000;
        while (!aStop.stop_requested() && !IsListening(process.dwProcessId, aPort))
        {
            if (WaitForSingleObject(process.hProcess, 100) == WAIT_OBJECT_0 || GetTickCount64() >= deadline)
            {
                status.Message = "Server did not start. The port may be in use; see logs/STServerOut.log.";
                return;
            }
        }
        if (aStop.stop_requested())
            return;

        status.State = "ready";
        const auto localAddress = GetLanAddress();
        if (!localAddress.empty())
            status.LanAddress = localAddress + ":" + port;
        status.Message = aInternet ? "Server ready. Looking for your router..." : "LAN server ready. Friends on your network can join using this PC's LAN address.";
        Publish(status);

        if (aInternet)
        {
            int error = 0;
            auto* devices = upnpDiscover(2000, nullptr, nullptr, UPNP_LOCAL_PORT_ANY, 0, 2, &error);
            const int igd = UPNP_GetValidIGD(devices, &urls, &data, lan, sizeof(lan), wan, sizeof(wan));
            freeUPNPDevlist(devices);
            if (lan[0])
                status.LanAddress = std::string(lan) + ":" + port;
            if (igd != UPNP_CONNECTED_IGD)
                status.Message = igd == UPNP_PRIVATEIP_IGD ? "LAN ready. Your router is behind another router or carrier NAT. UPnP alone cannot open Internet access." :
                                                          "LAN ready. No connected UPnP router found. Enable UPnP on your router or forward UDP " + port + " manually.";
            else if (!aStop.stop_requested())
            {
                bool ours = false, reusable = false;
                int existing = mapping(ours, &reusable);
                // Some routers retain the old description on AddPortMapping. Remove only a verified STR lease for this endpoint first.
                if (reusable)
                {
                    UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port.c_str(), "UDP", nullptr);
                    existing = mapping(ours);
                }
                if (existing != 714)
                    status.Message = "LAN ready. Router port " + port + " is already mapped or could not be checked. Choose another port; existing mappings are preserved.";
                else
                {
                    const int result = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, port.c_str(), port.c_str(), lan,
                                                         description.c_str(), "UDP", nullptr, "3600");
                    // Verify even after a timeout: the router may have applied the request.
                    mapping(ownsMapping);
                    if (ownsMapping)
                    {
                        status.Address = std::string(wan) + ":" + port;
                        status.Message = "Router port opened. Share the address and password. An outside connection is still needed to verify Internet access.";
                    }
                    else
                        status.Message = "LAN ready. Router refused or could not confirm UPnP (code " + std::to_string(result) + "). Forward UDP " + port + " manually.";
                }
            }
            Publish(status);
        }

        auto renewAt = GetTickCount64() + 1200000;
        while (!aStop.stop_requested())
        {
            if (WaitForSingleObject(process.hProcess, 100) == WAIT_OBJECT_0)
            {
                status.State = "error";
                status.Message = "The host server stopped. Check logs/STServerOut.log before restarting.";
                return;
            }
            if (ownsMapping && GetTickCount64() >= renewAt)
            {
                bool ours = false;
                const int existing = mapping(ours);
                const int result = (ours || existing == 714) ? UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, port.c_str(), port.c_str(), lan,
                                                                                 description.c_str(), "UDP", nullptr, "3600") : -1;
                if (result != 0)
                {
                    status.Address.clear();
                    status.Message = "Router port renewal failed. Retrying in one minute; Internet access may be interrupted.";
                    Publish(status);
                }
                else if (status.Address.empty())
                {
                    status.Address = std::string(wan) + ":" + port;
                    status.Message = "Router port renewed. Share the address and password.";
                    Publish(status);
                }
                renewAt = GetTickCount64() + (result == 0 ? 1200000 : 60000);
            }
        }
    };
    run();
    if (aStop.stop_requested())
    {
        status.State = "stopping";
        status.Message = "Stopping the server and removing its router mapping...";
        Publish(status);
    }
    if (job)
        CloseHandle(job);
    if (process.hThread)
        CloseHandle(process.hThread);
    if (process.hProcess)
    {
        WaitForSingleObject(process.hProcess, 5000);
        CloseHandle(process.hProcess);
    }
    if (ownsMapping)
    {
        bool ours = false;
        const int existing = mapping(ours);
        bool cleanupFailed = existing != 0 && existing != 714;
        if (ours)
        {
            UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port.c_str(), "UDP", nullptr);
            const int remaining = mapping(ours);
            cleanupFailed = ours || (remaining != 0 && remaining != 714);
        }
        if (cleanupFailed)
            status.Message = "Server stopped. Router cleanup could not be confirmed. Remove UDP " + port + " in your router if it remains listed.";
    }
    FreeUPNPUrls(&urls);
    status.Address.clear();
    status.LanAddress.clear();
    status.State = aStop.stop_requested() ? "stopped" : "error";
    if (status.Message == "Stopping the server and removing its router mapping...")
        status.Message = "Hosting stopped.";
    m_finished = true;
    Publish(status);
}
