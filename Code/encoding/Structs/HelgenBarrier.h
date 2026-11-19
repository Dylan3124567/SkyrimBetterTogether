#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_map>

// 0: creating characters, 1: local cinematics, 2: ordinary gameplay.
struct HelgenBarrier
{
    uint8_t Phase{};
    uint32_t ScenePhases{0xFFFEFFFE};
    bool LeaderAtBlock{};
    std::unordered_map<uint32_t, uint8_t> Players;

    bool Report(uint32_t aPlayer, uint8_t aState, std::span<const uint32_t> aMembers)
    {
        if (aState > 2 || aState > Phase + 1 || std::find(aMembers.begin(), aMembers.end(), aPlayer) == aMembers.end())
            return false;
        if (!Players.contains(aPlayer) && (Phase != 0 || aState != 0))
            return false;
        Players[aPlayer] = std::max(Players[aPlayer], aState);
        Update(aMembers);
        return true;
    }

    uint32_t Count(std::span<const uint32_t> aMembers) const
    {
        return static_cast<uint32_t>(std::count_if(aMembers.begin(), aMembers.end(), [this](uint32_t aId) {
            auto it = Players.find(aId);
            return it != Players.end() && it->second >= std::min<int>(Phase + 1, 2);
        }));
    }

    void Update(std::span<const uint32_t> aMembers)
    {
        std::erase_if(Players, [aMembers](const auto& aEntry) {
            return std::find(aMembers.begin(), aMembers.end(), aEntry.first) == aMembers.end();
        });
        if (Phase < 2 && aMembers.size() >= (Phase == 0 ? 2u : 1u) && Count(aMembers) == aMembers.size())
            ++Phase;
    }
};
