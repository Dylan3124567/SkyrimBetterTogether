#pragma once

#include <Structs/DroppedItem.h>

#include <algorithm>
#include <optional>

/**
 * @brief The items lying in the world, as the server knows them.
 *
 * This is what keeps one item from being collected twice: an item only leaves the ledger once, so of any number of
 * players reaching for it at the same time, only the first gets it.
 */
class DropLedger
{
public:
    struct Record
    {
        DroppedItem Item;
        uint32_t DropperId;
    };

    /**
     * @return The stored item, with its Id set. Its Cookie is cleared, as that is only meant for the dropper.
     */
    const DroppedItem& Add(DroppedItem aItem, const uint32_t aDropperId) noexcept
    {
        aItem.Id = m_nextId++;
        aItem.Cookie = 0;
        m_records.push_back({std::move(aItem), aDropperId});
        return m_records.back().Item;
    }

    /**
     * @return The item, if nobody took it before.
     */
    std::optional<DroppedItem> Take(const uint32_t aId) noexcept
    {
        const auto it = std::find_if(m_records.begin(), m_records.end(), [aId](const Record& acRecord) { return acRecord.Item.Id == aId; });
        if (it == m_records.end())
            return std::nullopt;

        DroppedItem item = std::move(it->Item);
        m_records.erase(it);
        return item;
    }

    /**
     * @brief Removes everything dropped by a player who is leaving.
     * @return The IDs of the items removed.
     */
    TiltedPhoques::Vector<uint32_t> TakeAllFrom(const uint32_t aDropperId) noexcept
    {
        TiltedPhoques::Vector<uint32_t> ids;
        std::erase_if(m_records, [&ids, aDropperId](const Record& acRecord) {
            if (acRecord.DropperId != aDropperId)
                return false;

            ids.push_back(acRecord.Item.Id);
            return true;
        });
        return ids;
    }

    const TiltedPhoques::Vector<Record>& GetRecords() const noexcept { return m_records; }

private:
    TiltedPhoques::Vector<Record> m_records;
    // Clients use 0 for an item the server has not numbered yet.
    uint32_t m_nextId = 1;
};
