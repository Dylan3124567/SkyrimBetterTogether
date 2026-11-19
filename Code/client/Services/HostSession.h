#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

// A private, disposable server. No Skyrim objects are accessed from the worker.
struct HostSession
{
    struct Status
    {
        std::string State = "stopped";
        std::string Message;
        std::string Address;
        std::string LanAddress;
        uint16_t Port = 10578;
        uint64_t Revision = 0;
    };

    ~HostSession();
    bool Start(const std::filesystem::path& aServer, uint16_t aPort, const std::string& aPassword, bool aInternet);
    void Stop();
    Status GetStatus();

private:
    void Run(std::stop_token aStop, std::filesystem::path aServer, uint16_t aPort, std::string aPassword, bool aInternet);
    void Publish(const Status& aStatus);

    std::mutex m_mutex;
    Status m_status;
    std::atomic_bool m_finished{true};
    std::jthread m_worker;
};
