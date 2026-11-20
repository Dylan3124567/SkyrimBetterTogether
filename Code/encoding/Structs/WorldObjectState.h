#pragma once

#include <Structs/GameId.h>
#include <Structs/LockData.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

/**
 * @brief What the server remembers about a door after the players who changed it have left its cell.
 *
 * Activations and lock changes are relayed only to players in the same cell, and the server's object entities go away
 * with the last player in the cell. Each save keeps its own copy of a door, so a player who arrives later found it as
 * they left it: the Ragged Flagon's Cistern door that another player opened stayed shut for them.
 */
struct WorldObjectState
{
    bool operator==(const WorldObjectState&) const noexcept = default;

    GameId Id{};
    // TESObjectREFR::OpenState after the last activation by a player, kNone when unknown
    uint8_t OpenState{};
    bool HasLock{};
    LockData Lock{};
};

namespace WorldObjectRules
{
// TESObjectREFR::OpenState
enum : uint8_t
{
    kNone,
    kOpen,
    kOpening,
    kClosed,
    kClosing,
};

// TESForm record flag 0x400, which references keep at runtime: always loaded, the same reference on every client.
inline constexpr uint32_t kPersistentFormFlag = 1u << 10;

// More doors than any load order has persistent ones. Past it the least recently changed door is forgotten.
inline constexpr std::size_t kMaxRemembered = 65536;
// A change counts as given to at most this many players; the oldest of them would get it once more.
inline constexpr std::size_t kMaxHolders = 64;
// AssignObjectsRequest::kMaxObjects: the most objects one cell can have on the server
inline constexpr std::size_t kMaxObjectsPerCell = 4096;
// Object entities of all cells together; past it those in cells nobody is in are freed first
inline constexpr std::size_t kMaxObjects = std::size_t{1} << 17;

[[nodiscard]] constexpr bool IsOpen(uint8_t aState) noexcept
{
    return aState == kOpen || aState == kOpening;
}

// Activating a door turns it around, so the state its activator saw before tells where it ends up.
// Anything that isn't a door reports kNone and isn't remembered.
[[nodiscard]] constexpr uint8_t OpenStateAfterActivation(uint8_t aBefore) noexcept
{
    switch (aBefore)
    {
    case kOpen:
    case kOpening: return kClosed;
    case kClosed:
    case kClosing: return kOpen;
    default: return kNone;
    }
}

// Only persistent doors: a load door's lock is shared by both of its sides and every client's NPC schedules change it,
// and other references aren't always the same object or loaded on every client.
[[nodiscard]] constexpr bool CanApplyTo(bool aIsDoor, uint32_t aFormFlags, bool aIsLoadDoor) noexcept
{
    return aIsDoor && (aFormFlags & kPersistentFormFlag) && !aIsLoadDoor;
}

[[nodiscard]] constexpr bool NeedsOpenChange(uint8_t aRemembered, uint8_t aLocal) noexcept
{
    return aRemembered != kNone && IsOpen(aRemembered) != IsOpen(aLocal);
}

// A client speaks for an object only from inside the cell the server has it in. The ids and cells in its packets are its
// own claims; the object's cell is where the server assigned it, the sender's cell where it reported entering.
[[nodiscard]] inline bool InSendersCell(const std::optional<GameId>& acObjectCell, const GameId& acSenderCell) noexcept
{
    return acObjectCell && *acObjectCell && *acObjectCell == acSenderCell;
}
} // namespace WorldObjectRules

/**
 * @brief The server's object entities by game id, so a request of thousands of objects isn't a scan per object, with
 * ceilings a client can't push the entity count past.
 */
template <class THandle> class WorldObjectIndex
{
public:
    [[nodiscard]] std::optional<THandle> Find(const GameId& aId) const
    {
        const auto it = m_byId.find(aId);
        return it == m_byId.end() ? std::nullopt : std::optional<THandle>(it->second);
    }

    [[nodiscard]] std::optional<GameId> CellOf(const GameId& aId) const
    {
        const auto it = m_byId.find(aId);
        if (it == m_byId.end())
            return std::nullopt;
        return m_byHandle.at(it->second).Cell;
    }

    [[nodiscard]] std::size_t CountIn(const GameId& aCell) const
    {
        const auto it = m_perCell.find(aCell);
        return it == m_perCell.end() ? 0 : it->second;
    }

    [[nodiscard]] bool CellFull(const GameId& aCell) const { return CountIn(aCell) >= WorldObjectRules::kMaxObjectsPerCell; }
    [[nodiscard]] bool Full() const noexcept { return m_byHandle.size() >= WorldObjectRules::kMaxObjects; }
    [[nodiscard]] std::size_t Size() const noexcept { return m_byHandle.size(); }

    bool Add(const GameId& aId, const GameId& aCell, THandle aHandle)
    {
        if (!aId || !aCell || Full() || CellFull(aCell) || m_byId.contains(aId) || m_byHandle.contains(aHandle))
            return false;
        m_byId.emplace(aId, aHandle);
        m_byHandle.emplace(aHandle, Entry{aId, aCell});
        ++m_perCell[aCell];
        return true;
    }

    void Remove(THandle aHandle)
    {
        const auto it = m_byHandle.find(aHandle);
        if (it == m_byHandle.end())
            return;
        m_byId.erase(it->second.Id);
        if (const auto cell = m_perCell.find(it->second.Cell); cell != m_perCell.end() && --cell->second == 0)
            m_perCell.erase(cell);
        m_byHandle.erase(it);
    }

private:
    struct Entry
    {
        GameId Id;
        GameId Cell;
    };

    std::unordered_map<GameId, THandle> m_byId;
    std::unordered_map<THandle, Entry> m_byHandle;
    std::unordered_map<GameId, std::size_t> m_perCell;
};

// Each change is given once to every player whose world doesn't show it yet: the one who made it and those in the cell,
// who got it live, already have it, and later changes a client makes on its own (a script closing the door again) must not
// be undone each time they come back. Bounded: past kMaxRemembered the least recently changed door is forgotten.
class WorldObjectMemory
{
public:
    void RecordActivation(const GameId& aId, uint8_t aPreActivationOpenState, std::span<const uint32_t> aHolders)
    {
        const uint8_t cAfter = WorldObjectRules::OpenStateAfterActivation(aPreActivationOpenState);
        if (cAfter == WorldObjectRules::kNone)
            return;
        if (Remembered* pEntry = Touch(aId))
        {
            pEntry->State.OpenState = cAfter;
            SetHolders(pEntry->OpenHolders, aHolders);
        }
    }

    void RecordLock(const GameId& aId, const LockData& acLock, std::span<const uint32_t> aHolders)
    {
        if (Remembered* pEntry = Touch(aId))
        {
            pEntry->State.HasLock = true;
            pEntry->State.Lock = acLock;
            SetHolders(pEntry->LockHolders, aHolders);
        }
    }

    // What this player doesn't have yet, from then on counted as theirs
    [[nodiscard]] std::optional<WorldObjectState> TakeFor(const GameId& aId, uint32_t aPlayerId)
    {
        const auto it = m_entries.find(aId);
        if (it == m_entries.end())
            return std::nullopt;

        Remembered& entry = it->second;
        WorldObjectState state = entry.State;
        if (!Give(entry.OpenHolders, aPlayerId))
            state.OpenState = WorldObjectRules::kNone;
        if (!Give(entry.LockHolders, aPlayerId) || !state.HasLock)
        {
            state.HasLock = false;
            state.Lock = {};
        }

        if (state.OpenState == WorldObjectRules::kNone && !state.HasLock)
            return std::nullopt;
        return state;
    }

    [[nodiscard]] bool Contains(const GameId& aId) const { return m_entries.contains(aId); }
    [[nodiscard]] std::size_t Size() const noexcept { return m_entries.size(); }

private:
    struct Remembered
    {
        WorldObjectState State;
        std::vector<uint32_t> OpenHolders;
        std::vector<uint32_t> LockHolders;
        std::list<GameId>::iterator Order;
    };

    static void SetHolders(std::vector<uint32_t>& aHolders, std::span<const uint32_t> aNew)
    {
        const std::size_t cCount = std::min(aNew.size(), WorldObjectRules::kMaxHolders);
        aHolders.assign(aNew.begin(), aNew.begin() + cCount);
    }

    static bool Give(std::vector<uint32_t>& aHolders, uint32_t aPlayerId)
    {
        if (std::find(aHolders.begin(), aHolders.end(), aPlayerId) != aHolders.end())
            return false;
        if (aHolders.size() >= WorldObjectRules::kMaxHolders)
            aHolders.erase(aHolders.begin());
        aHolders.push_back(aPlayerId);
        return true;
    }

    // The entry for this id, now the most recently changed one
    Remembered* Touch(const GameId& aId)
    {
        if (!aId)
            return nullptr;
        if (const auto it = m_entries.find(aId); it != m_entries.end())
        {
            m_order.splice(m_order.end(), m_order, it->second.Order);
            return &it->second;
        }
        if (m_entries.size() >= WorldObjectRules::kMaxRemembered)
        {
            m_entries.erase(m_order.front());
            m_order.pop_front();
        }
        Remembered& entry = m_entries[aId];
        entry.State.Id = aId;
        entry.Order = m_order.insert(m_order.end(), aId);
        return &entry;
    }

    std::unordered_map<GameId, Remembered> m_entries;
    // Least recently changed first
    std::list<GameId> m_order;
};
