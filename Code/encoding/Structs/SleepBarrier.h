#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <span>

// One shared clock: every connected player must consent, not just one party.
struct SleepBarrier
{
    struct Entry { uint32_t Session{}, Completed{}, Seconds{}; uint8_t Hours{}; };
    std::map<uint32_t, Entry> Players;
    uint32_t Step{}, StepSeconds{}, RemainingSeconds{};

    bool Begin(uint32_t aPlayer, uint32_t aSession, uint8_t aHours, uint16_t aSeconds)
    {
        if (!aSession || !aHours || aHours > 24 || !aSeconds || aSeconds > 3600 || 3600 % aSeconds || Step)
            return false;
        if (Players.contains(aPlayer))
            return false;
        Players.emplace(aPlayer, Entry{aSession, 0, aSeconds, aHours});
        return true;
    }

    bool Start(std::span<const uint32_t> aPlayers)
    {
        if (Step || aPlayers.empty() || Players.size() != aPlayers.size())
            return false;
        uint32_t seconds = Players.begin()->second.Seconds;
        uint32_t duration = 24 * 3600;
        for (uint32_t id : aPlayers)
        {
            auto it = Players.find(id);
            if (it == Players.end() || it->second.Seconds != seconds)
                return false;
            duration = std::min(duration, uint32_t(it->second.Hours) * 3600);
        }
        StepSeconds = seconds;
        RemainingSeconds = duration;
        Step = 1;
        return true;
    }

    bool Tick(uint32_t aPlayer, uint32_t aSession, uint32_t aStep)
    {
        auto it = Players.find(aPlayer);
        if (!Step || !RemainingSeconds || aStep != Step || it == Players.end() || it->second.Session != aSession || it->second.Completed >= aStep)
            return false;
        it->second.Completed = aStep;
        for (const auto& [id, entry] : Players)
            if (entry.Completed != Step)
                return false;
        RemainingSeconds -= StepSeconds;
        ++Step;
        return true;
    }
};
