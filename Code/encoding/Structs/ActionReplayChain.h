#pragma once

#include <Structs/ActionEvent.h>

using TiltedPhoques::Vector;

struct ActionReplayChain
{
    ActionReplayChain() = default;
    ~ActionReplayChain() = default;

    bool operator==(const ActionReplayChain& acRhs) const noexcept;
    bool operator!=(const ActionReplayChain& acRhs) const noexcept;

    void Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept;
    void Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept;

    /**
     * @brief Drops everything the owner did in furniture, for an owner that is out of it now.
     *
     * Getting up isn't always recognized as an exit, so the way in can still be in the chain. Replayed on a new copy it
     * sits down wherever its owner stands now. The graph is reset as well, an actor that was already here may sit.
     */
    void DropFurnitureActions() noexcept;

    bool ResetAnimationGraph{false};
    Vector<ActionEvent> Actions;
};
