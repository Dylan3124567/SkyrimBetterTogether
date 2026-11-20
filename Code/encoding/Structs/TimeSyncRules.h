#pragma once

#include <cstdint>

// Clock rules of the server and the clients, free of game headers so TPTests covers them.
namespace TimeSyncRules
{
// Every client advances its copy of the server clock (common/DateTime) by its own frame times in float, and the server by
// its own tick times. The rounding differs with the update rate: 7 ms frames run 0.6% slow and 16/17 ms frames 0.3% fast,
// several game minutes apart per real hour at the default time scale (upstream #180). The server resends its clock this often.
inline constexpr uint64_t kResyncIntervalMs = 60000;

// A client that keeps its own calendar (SyncPlayerCalendar off) only moves to the next day when its own clock passes
// midnight, so a resync across midnight would skip or repeat a day. The drift between two resyncs is game seconds.
[[nodiscard]] constexpr bool CanResyncAt(float aHour) noexcept
{
    return aHour >= 0.1f && aHour <= 23.9f;
}

// GameDaysPassed is a float the game rebuilds from whole days plus GameHour / 24 (TimeData::SimulateTime, id 36291).
// One frame of game time added to it directly is below its precision once a save is a few weeks old: the days then stop
// or run at twice the speed depending on the frame rate. Survival's needs, Utility.GetCurrentGameTime and every
// game-time Papyrus update run on that value. This keeps the sum in double and rounds only what it writes.
struct DaysPassed
{
    double Days = 0.0;
    float Written = -1.f;

    [[nodiscard]] float Advance(float aCurrent, double aHours) noexcept
    {
        // Changed by the game since the last write: native sleep steps, a save load, a script.
        if (aCurrent != Written)
            Days = aCurrent;
        Days += aHours / 24.0;
        Written = static_cast<float>(Days);
        return Written;
    }
};
} // namespace TimeSyncRules
