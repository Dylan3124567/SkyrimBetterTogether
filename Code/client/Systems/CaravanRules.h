#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

/**
 * @brief The three Khajiit caravans of Skyrim.esm, so a client shows the camp of a caravan another client runs.
 *
 * CaravanScript (quest Caravans) runs on every client from that save's own state. It enables a camp's marker, whose enable
 * children are the tents, once the leader's travel package ends and the leader is out of sight, and disables it 24 game
 * hours later. The caravan's people are synced from the client running their AI, the camps were not: one player saw the
 * caravan with its tents and the other the same caravan without them. Every id and position here is from Skyrim.esm.
 */
namespace CaravanRules
{
inline constexpr uint32_t kCaravansQuest = 0x3ACDB;
inline constexpr uint32_t kTamriel = 0x3C;

// The leader is camped while it is this close to its camp seat, the target of its CaravanXCampNSit package. A caravan
// only settles after this many checks in a row there, one passing by on its way doesn't.
inline constexpr float kSeatRadius = 512.f;
inline constexpr uint32_t kSettleChecks = 5;

// CaravanXCampNSit runs on CaravanIsCampedX == 1 and CaravanLocationX == N - 1: the values CaravanScript.SetGlobals and
// GetCampEnableMarker give a caravan camped at its first (CampNum 0) or second (CampNum 1) marker.
inline constexpr float kCampedValue = 1.f;

constexpr float LocationValue(const size_t aCamp) noexcept
{
    return static_cast<float>(aCamp);
}

struct Camp
{
    uint32_t EnableMarker; // CaravanScript.CampEnableMarkerXN, an XMarker in Tamriel's persistent cell
    float MarkerX;
    float MarkerY;
    uint32_t Seat; // target of the leader's CaravanXCampNSit package
    float SeatX;
    float SeatY;
};

struct Caravan
{
    uint32_t LeaderAlias; // Caravans alias LeaderX
    uint32_t LocationGlobal; // CaravanLocationX
    uint32_t CampedGlobal; // CaravanIsCampedX
    std::array<Camp, 2> Camps;
};

inline constexpr std::array<Caravan, 3> kCaravans{{
    {15, 0x72FFB, 0x73006, {{{0x10372F, 14670.6f, -9166.5f, 0x103717, 15447.4f, -9235.5f}, {0x1030E2, -160783.8f, 4919.3f, 0x1030A0, -160530.1f, 5342.9f}}}},
    {16, 0x73C98, 0x73C96, {{{0x104AD6, -67836.4f, 102863.7f, 0x104AE0, -69469.9f, 101609.f}, {0x103774, 137589.6f, 26272.8f, 0x10376D, 137610.8f, 25700.5f}}}},
    {18, 0x73CAE, 0x73C97, {{{0x10311B, 172569.3f, -89530.1f, 0x1030F2, 172563.2f, -89674.7f}, {0x10332A, 25950.8f, 101149.f, 0x10334C, 25711.f, 100753.6f}}}},
}};

// Which of its caravan's camps a leader at this position is at, -1 for none
inline int FindCamp(const Caravan& acCaravan, const float aX, const float aY) noexcept
{
    for (size_t i = 0; i < acCaravan.Camps.size(); ++i)
    {
        const float cX = aX - acCaravan.Camps[i].SeatX;
        const float cY = aY - acCaravan.Camps[i].SeatY;
        if (cX * cX + cY * cY <= kSeatRadius * kSeatRadius)
            return static_cast<int>(i);
    }

    return -1;
}

// Follows a leader over the checks, see kSettleChecks
struct CampTracker
{
    int Camp{-1};
    uint32_t Checks{0};

    // @return The camp the caravan has settled at, -1 while it isn't settled anywhere.
    int Update(const int aCamp) noexcept
    {
        if (aCamp != Camp)
        {
            Camp = aCamp;
            Checks = 0;
        }

        if (Camp < 0)
            return -1;

        if (Checks < kSettleChecks)
            ++Checks;

        return Checks >= kSettleChecks ? Camp : -1;
    }

    void Reset() noexcept { Update(-1); }
};

inline int32_t CellOf(const float aPosition) noexcept
{
    return static_cast<int32_t>(std::floor(aPosition / 4096.f));
}

/**
 * @brief Whether a camp can be taken down without its tents vanishing in front of the player.
 *
 * The tents are at most a cell away from the marker's cell, and uGridsToLoad 5 attaches two cells around the player's.
 * @param aPlayerInTamriel Outside in Tamriel, the camps' world space. Inside anything else none of it is attached.
 */
inline bool CanHideCamp(const bool aPlayerInTamriel, const int32_t aCenterGridX, const int32_t aCenterGridY, const Camp& acCamp) noexcept
{
    if (!aPlayerInTamriel)
        return true;

    return std::abs(CellOf(acCamp.MarkerX) - aCenterGridX) > 3 || std::abs(CellOf(acCamp.MarkerY) - aCenterGridY) > 3;
}
} // namespace CaravanRules
