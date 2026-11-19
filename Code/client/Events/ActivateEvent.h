#pragma once

/**
 * @brief In-game activation (i.e. interacting with an object).
 */
struct ActivateEvent
{
    ActivateEvent(uint32_t aObjectId, uint32_t aActivatorId, TESObjectREFR::OpenState aPreActivationOpenState)
        : ObjectId(aObjectId)
        , ActivatorId(aActivatorId)
        , PreActivationOpenState(aPreActivationOpenState)
    {
    }

    uint32_t ObjectId;
    uint32_t ActivatorId;
    TESObjectREFR::OpenState PreActivationOpenState;
};
