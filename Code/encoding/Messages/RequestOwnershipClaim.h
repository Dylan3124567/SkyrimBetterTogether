#pragma once

#include "Message.h"

struct RequestOwnershipClaim final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestOwnershipClaim;

    RequestOwnershipClaim()
        : ClientMessage(Opcode)
    {
    }

    virtual ~RequestOwnershipClaim() = default;

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestOwnershipClaim& achRhs) const noexcept
    {
        return ServerId == achRhs.ServerId && ExpectedOwnershipEpoch == achRhs.ExpectedOwnershipEpoch && ForDialogue == achRhs.ForDialogue && GetOpcode() == achRhs.GetOpcode();
    }

    uint32_t ServerId{};
    uint32_t ExpectedOwnershipEpoch{};
    // The sender's player is talking to the actor, which only stands still for it where its AI runs
    bool ForDialogue{};
};
