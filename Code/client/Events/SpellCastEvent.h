#pragma once

#include <cstdint>

/**
 * @brief Dispatched when a spell has been cast locally.
 */
struct SpellCastEvent
{
    SpellCastEvent(uint32_t aCasterFormID, int32_t aCastingSource, bool aIsDualCasting, uint32_t aSpellId, uint32_t aDesiredTargetID)
        : CasterFormID(aCasterFormID)
        , CastingSource(aCastingSource)
        , IsDualCasting(aIsDualCasting)
        , SpellId(aSpellId)
        , DesiredTargetID(aDesiredTargetID)
    {
    }

    uint32_t CasterFormID;
    int32_t CastingSource;
    bool IsDualCasting;
    uint32_t SpellId;
    uint32_t DesiredTargetID;
};
