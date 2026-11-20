#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>

/**
 * @brief Server rules for an NPC's death state: who may change it and when a remembered death still counts.
 *
 * The owner of an actor reports its death and its recovery. Another member of the owner's party standing near it may
 * bring it back (the console's resurrect, a script on that client), which a copy can't do on its own as its life follows
 * its owner. Nobody but the owner kills.
 *
 * A death outlives the actor's entity for a while, for the next client to come by with the actor still alive in its
 * save. That client never saw the death. One that did and reports the actor alive has brought it back since.
 */
namespace DeathMemory
{
// iHoursToRespawnCell, the earliest a cell can bring its actors back
inline constexpr int64_t cRecordHours = 240;

// No actor in a hitter or killer id. entt::null, as entity 0 is the first one made, often the first player's character.
inline constexpr uint32_t cNoActor = 0xFFFFFFFFu;

// At most this many deaths keep their witnesses, a client making up deaths of made-up actors can't grow it further
inline constexpr size_t cMaxWitnessRecords = 4096;

/**
 * @brief The game hour death records are kept in: good enough to tell days apart, months are short of 31 days by a day or two.
 */
[[nodiscard]] constexpr int64_t GameHour(const int64_t aYear, const int64_t aMonth, const int64_t aDay, const int64_t aHour) noexcept
{
    return ((aYear * 12 + aMonth) * 31 + aDay) * 24 + aHour;
}

/**
 * @brief Who a witness is: the client's Discord id when it has one, the same after reconnecting, otherwise the server's id
 * for the connection. Discord ids keep the top bit clear, so the two never meet.
 */
[[nodiscard]] constexpr uint64_t WitnessId(const uint64_t aDiscordId, const uint32_t aPlayerId) noexcept
{
    return aDiscordId != 0 ? aDiscordId : uint64_t{1} << 63 | aPlayerId;
}

/**
 * @brief Whether a remembered death overrides the alive state a client reports for an actor it is about to own.
 * @param aReporterSawDeath The reporting player was there when the actor died, or had it dead in its own game.
 */
[[nodiscard]] constexpr bool KeepsDeath(const int64_t aHoursSinceDeath, const bool aReporterSawDeath) noexcept
{
    return aHoursSinceDeath < cRecordHours && !aReporterSawDeath;
}

/**
 * @brief Drops the witness records that can no longer keep a death, then the oldest until there is room for another.
 * @param aRecords Map whose values have the game hour of the death as Hour.
 */
template <class TRecords> void PruneWitnesses(TRecords& aRecords, const int64_t aNow)
{
    for (auto it = aRecords.begin(); it != aRecords.end();)
        it = aNow - it->second.Hour >= cRecordHours ? aRecords.erase(it) : std::next(it);

    while (aRecords.size() >= cMaxWitnessRecords)
        aRecords.erase(std::min_element(aRecords.begin(), aRecords.end(), [](const auto& acLeft, const auto& acRight) { return acLeft.second.Hour < acRight.second.Hour; }));
}

/**
 * @brief Whether the server accepts a death state change for an actor.
 * @param aSharesParty The sender is in the owner's party, or the actor has no owner.
 */
[[nodiscard]] constexpr bool AcceptsChange(
    const bool aFromOwner, const bool aEpochMatches, const bool aWantsDead, const bool aIsDead, const bool aIsPlayer, const bool aInRange, const bool aSharesParty) noexcept
{
    if (!aEpochMatches)
        return false;

    if (aFromOwner)
        return true;

    return !aWantsDead && aIsDead && !aIsPlayer && aInRange && aSharesParty;
}

/**
 * @brief Whether the server passes on the killer the owner named with a death, which OnDeath scripts get as akKiller.
 *
 * A killer is never just taken from the client: it is the sender's own player, an actor the sender runs, or the player
 * whose client the server last relayed damage to the actor from. Either way it has to be near the actor.
 */
[[nodiscard]] constexpr bool AcceptsKiller(const bool aIsSendersPlayer, const bool aOwnedBySender, const bool aIsLastHitter, const bool aInRange) noexcept
{
    return aInRange && (aIsSendersPlayer || aOwnedBySender || aIsLastHitter);
}
} // namespace DeathMemory
