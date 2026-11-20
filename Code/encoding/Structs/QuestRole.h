#pragma once

#include <cstdint>

/**
 * @brief One actor per spawn when several party members' games spawn it.
 *
 * World encounters and world interactions are quests without a journal (WE03, WE04, WE08, WIChangeLocation09, DLC2WE10
 * and more in Skyrim.esm and Dragonborn.esm make their dragon with a "Create Reference to Object" alias). Every client's
 * story manager starts them on its own, and a shared quest runs on every member, so each game makes its own actor: two
 * players walking into the same town got a dragon each, and each saw the other's too.
 *
 * Such an actor is told apart by the quest alias that made it, the first alias it fills, which is the same on every client
 * running the quest. A party member's actor of the same role near an actor another member already runs is that actor,
 * the member's game keeps its own one as the copy, so its quest still has its alias filled. Elsewhere, or for a player
 * outside the party, it is a spawn of its own.
 */
namespace QuestRole
{
// Generous, quests use alias ids in the hundreds
inline constexpr uint32_t cMaxAliasId = 0xFFFF;

[[nodiscard]] constexpr bool IsRole(const uint32_t aQuestBaseId, const uint32_t aAliasId, const bool aIsTemporary) noexcept
{
    return aIsTemporary && aQuestBaseId != 0 && aAliasId <= cMaxAliasId;
}

/**
 * @brief Whether an existing entity is the actor a party member's client asks to have assigned.
 * @param aOwnedByRequester The requester runs the entity already, a new actor of its own is a new spawn of the quest.
 * @param aSharesParty The entity's owner and the requester are in the same party.
 * @param aIsDead, aRequestedDead A living actor is never the same as a corpse, a quest that ran again made a new one.
 */
[[nodiscard]] constexpr bool IsSameSpawn(
    const bool aSameRole, const bool aOwnedByRequester, const bool aSharesParty, const bool aInRange, const bool aIsDead, const bool aRequestedDead) noexcept
{
    return aSameRole && !aOwnedByRequester && aSharesParty && aInRange && aIsDead == aRequestedDead;
}
} // namespace QuestRole
