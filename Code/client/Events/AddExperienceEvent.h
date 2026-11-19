#pragma once

/**
 * @brief Combat experience the player's kill shares with the party.
 */
struct AddExperienceEvent
{
    AddExperienceEvent(float aExperience)
        : Experience(aExperience)
    {
    }

    float Experience{};
};
