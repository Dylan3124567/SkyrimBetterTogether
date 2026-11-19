#pragma once

#include <algorithm>
#include <cmath>

/**
 * @brief Rules for the combat experience a party shares. These have no game dependencies so they can be unit tested.
 *
 * A kill passes on the experience the killer earned since their last kill. It lands on the receiver's last used combat skill,
 * so it only counts for someone who is fighting too, and it's capped because the killer's skills can be far ahead of theirs.
 */
namespace PartyExperience
{
// How long after using a combat skill a player still counts as fighting, in seconds.
inline constexpr double cFightWindow = 30.0;
// The most one kill can add to a skill, as a part of what the skill's next level costs.
inline constexpr float cMaxKillCredit = 0.1f;

/**
 * @param aPending Experience earned since the last kill.
 * @param aDelta The change in a skill's progress. Levelling up resets the progress, which shows up as a negative change.
 * @return The new pending experience.
 */
inline float AddSkillUse(const float aPending, const float aDelta) noexcept
{
    return std::isfinite(aDelta) && aDelta > 0.f ? aPending + aDelta : aPending;
}

/**
 * @param aNow The current time in seconds.
 * @param aLastUse When a combat skill was last used, negative if it never was.
 */
inline bool IsFighting(const double aNow, const double aLastUse) noexcept
{
    return aLastUse >= 0.0 && aNow - aLastUse <= cFightWindow;
}

/**
 * @param aShared The experience a party member's kill passed on.
 * @param aLevelCost What the receiver's skill needs for its next level.
 * @return The experience to add to the receiver's skill.
 */
inline float GetKillCredit(const float aShared, const float aLevelCost) noexcept
{
    if (!std::isfinite(aShared) || aShared <= 0.f || !std::isfinite(aLevelCost) || aLevelCost <= 0.f)
        return 0.f;

    return std::min(aShared, aLevelCost * cMaxKillCredit);
}
} // namespace PartyExperience
