#pragma once

#include <atomic>
#include <cstdint>

/**
 * @brief Set from engine hooks, which can run on any thread, read by PlayerService.
 *
 * While active the local player can neither take nor deal damage. Trying to deal damage ends it, so protection can't be
 * used to fight back for free.
 */
namespace RespawnProtection
{
inline std::atomic_bool g_active{false};
inline std::atomic_bool g_broken{false};

inline bool IsActive() noexcept
{
    return g_active.load(std::memory_order_relaxed);
}

inline void SetActive(const bool aActive) noexcept
{
    g_active.store(aActive, std::memory_order_relaxed);
    if (!aActive)
        g_broken.store(false, std::memory_order_relaxed);
}

inline void Break() noexcept
{
    if (IsActive())
        g_broken.store(true, std::memory_order_relaxed);
}

inline bool ConsumeBreak() noexcept
{
    return g_broken.exchange(false, std::memory_order_relaxed);
}
} // namespace RespawnProtection

/**
 * @brief The player's way back from going down, as a single state machine with no game dependencies.
 *
 * Every step that was once a separate timer lives here because they used to race: a hit landing between the teleport and
 * the knockdown started a second death on top of the first, which lost the saved controls (stuck in place, only the head
 * moves) or left the screen faded out for good.
 *
 * - Going down always ends in a respawn, and a respawn always ends in a fade in, whatever happens along the way.
 * - The player is invulnerable from the moment of the respawn, so it can't be interrupted by another death.
 * - Timers don't start until the world has loaded, otherwise they elapse behind a loading screen and the fade in is lost.
 */
class RespawnFlow
{
public:
    enum class Phase : uint8_t
    {
        Alive,
        Dying,
        Recovering,
        Protected,
    };

    enum Command : uint32_t
    {
        kNone = 0,
        kBeginDeath = 1 << 0, // Fade out and settle the death
        kRespawn = 1 << 1,    // Become invulnerable, then heal and move the player, who can now get up from bleedout
        kKnock = 1 << 2,      // Last resort: ragdoll a player who still can't get up, the get up from it always works
        kFadeIn = 1 << 3,     // Start the fade in, hidden behind the getting up for its first seconds
        kFadeInNow = 1 << 4,  // Fade in right away, this is harmless when the screen is clear
        kForceGraph = 1 << 5, // Send the player's behavior graph back to its default state
        kForceState = 1 << 6, // Clear the actor state that keeps the player down
        kStartProtection = 1 << 7,
        kEndProtection = 1 << 8,
        kGetUp = 1 << 9, // Hand back the controls the death took, the player stands up from bleedout on their own
    };

    struct Frame
    {
        double Now{};     // Seconds
        bool IsDown{};    // Bleeding out
        bool IsStuck{};   // Bleeding out or knocked down, so not able to act yet
        bool IsLoading{}; // A loading screen is up, or the player isn't in the world
        bool HasMovementInput{};
        bool HasBroken{}; // The player tried to do something, see RespawnProtection
    };

    static constexpr double cRespawnDelay = 5.0;
    // How long the world gets to settle after loading before the player gets up
    static constexpr double cSettleDelay = 1.5;
    // Nothing loads for this long, so whatever claims it's still loading is wrong and must not leave the screen black
    static constexpr double cLoadingLimit = 60.0;
    // Getting up from bleedout takes a few seconds, each step below only runs if the previous one didn't get the player up
    static constexpr double cForceGraphDelay = 5.0;
    static constexpr double cForceStateDelay = 8.0;
    static constexpr double cKnockDelay = 10.0;
    static constexpr double cRecoveryLimit = 14.0;
    // The fade in hides the first moments, keys held since before the death shouldn't end protection unseen
    static constexpr double cInputGrace = 1.0;
    // Idling can't be a way to stay invulnerable forever, say to keep enemies busy while the rest of the party kills them
    static constexpr double cProtectionLimit = 30.0;

    uint32_t Update(const Frame& acFrame) noexcept
    {
        switch (m_phase)
        {
        case Phase::Alive: return UpdateAlive(acFrame);
        case Phase::Dying: return UpdateDying(acFrame);
        case Phase::Recovering: return UpdateRecovering(acFrame);
        case Phase::Protected: return UpdateProtected(acFrame);
        }
        return kNone;
    }

    /**
     * @brief Drops out of the flow, the caller has to undo whatever the current phase put in place.
     */
    void Reset() noexcept { m_phase = Phase::Alive; }

    Phase GetPhase() const noexcept { return m_phase; }
    bool IsInvulnerable() const noexcept { return m_phase == Phase::Recovering || m_phase == Phase::Protected; }
    bool IsScreenFaded() const noexcept { return m_phase == Phase::Dying || m_phase == Phase::Recovering; }
    /**
     * @brief Whether the player is up and playing, so their controls can be trusted to be what they should be.
     */
    bool IsUp() const noexcept { return m_phase == Phase::Alive || m_phase == Phase::Protected; }

private:
    uint32_t UpdateAlive(const Frame& acFrame) noexcept
    {
        if (!acFrame.IsDown)
            return kNone;

        m_phase = Phase::Dying;
        m_deadline = acFrame.Now + cRespawnDelay;
        return kBeginDeath;
    }

    uint32_t UpdateDying(const Frame& acFrame) noexcept
    {
        // Carries on if the player got up in the meantime, as the screen is fading out and only a respawn fades it back in.
        if (acFrame.Now < m_deadline || (acFrame.IsLoading && acFrame.Now < m_deadline + cLoadingLimit))
            return kNone;

        m_phase = Phase::Recovering;
        m_deadline = acFrame.Now + cSettleDelay;
        m_loadingLimit = acFrame.Now + cLoadingLimit;
        m_getUpTime = -1.0;
        m_forcedGraph = m_forcedState = m_knocked = false;
        return kRespawn;
    }

    uint32_t UpdateRecovering(const Frame& acFrame) noexcept
    {
        if (m_getUpTime < 0.0)
        {
            if (acFrame.IsLoading && acFrame.Now < m_loadingLimit)
            {
                m_deadline = acFrame.Now + cSettleDelay;
                return kNone;
            }

            if (acFrame.Now < m_deadline)
                return kNone;

            m_getUpTime = acFrame.Now;
            return kGetUp | kFadeIn;
        }

        const double cElapsed = acFrame.Now - m_getUpTime;
        if ((!acFrame.IsStuck && cElapsed >= 0.5) || cElapsed >= cRecoveryLimit)
            return StartProtection(acFrame);

        if (!m_knocked && cElapsed >= cKnockDelay)
        {
            m_knocked = true;
            return kKnock;
        }

        if (!m_forcedState && cElapsed >= cForceStateDelay)
        {
            m_forcedState = true;
            return kForceState | kForceGraph;
        }

        if (!m_forcedGraph && cElapsed >= cForceGraphDelay)
        {
            m_forcedGraph = true;
            return kForceGraph;
        }

        return kNone;
    }

    uint32_t UpdateProtected(const Frame& acFrame) noexcept
    {
        m_wasUp |= !acFrame.IsDown;

        // Can't come from a hit, but a script can still put the player down
        if (acFrame.IsDown && m_wasUp)
        {
            m_phase = Phase::Dying;
            m_deadline = acFrame.Now + cRespawnDelay;
            return kBeginDeath | kEndProtection;
        }

        const double cElapsed = acFrame.Now - m_protectionStart;
        const bool cHasInput = acFrame.HasMovementInput && cElapsed >= cInputGrace;
        if (!acFrame.HasBroken && !cHasInput && cElapsed < cProtectionLimit)
            return kNone;

        m_phase = Phase::Alive;
        return kEndProtection;
    }

    uint32_t StartProtection(const Frame& acFrame) noexcept
    {
        m_phase = Phase::Protected;
        m_protectionStart = acFrame.Now;
        m_wasUp = !acFrame.IsDown;
        return kStartProtection | kFadeInNow;
    }

    Phase m_phase{Phase::Alive};
    double m_deadline{};
    double m_loadingLimit{};
    double m_getUpTime{-1.0};
    double m_protectionStart{};
    bool m_forcedGraph{};
    bool m_forcedState{};
    bool m_knocked{};
    bool m_wasUp{};
};
