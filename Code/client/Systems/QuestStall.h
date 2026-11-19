#pragma once

#include <cstdint>

/**
 * @brief Decides when the scene the leader's held quest stages wait on has stopped moving.
 *
 * A held stage is meant to wait for the leader's scene to catch up, but that scene can stop for good: a package action
 * never completes for an NPC another client owns, so it can sit in one phase until the leader leaves the area. Often the
 * held stage is also the one that would stop the scene (MQ103 stage 50 stops MQ103IntroScene), so waiting never ends.
 *
 * Has no game dependencies so it can be unit tested.
 */
class QuestStall
{
public:
    // A long walk with lines in it can legitimately take this long, the party's other member is already past it anyway.
    static constexpr double cStallTime = 45.0;
    // Between stages released from a stalled scene, so each stage's fragments can run before the next.
    static constexpr double cReleaseGap = 5.0;

    /**
     * @param aScenes What the quest's scenes are doing now. Any change counts as progress.
     * @param aDelta Seconds since the last call.
     * @return Whether a held stage should be released now.
     */
    bool Update(const uint64_t aScenes, const double aDelta) noexcept
    {
        if (!m_started || aScenes != m_scenes)
        {
            m_started = true;
            m_scenes = aScenes;
            m_idle = 0.0;
            return false;
        }

        m_idle += aDelta;
        return m_idle >= cStallTime;
    }

    void OnReleased() noexcept { m_idle = cStallTime - cReleaseGap; }

private:
    bool m_started = false;
    uint64_t m_scenes = 0;
    double m_idle = 0.0;
};
