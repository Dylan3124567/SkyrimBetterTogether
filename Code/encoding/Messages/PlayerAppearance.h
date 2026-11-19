#pragma once
#include "Message.h"
#include <Structs/Tints.h>

struct PlayerAppearance
{
    uint32_t ActorId{}, OwnershipEpoch{}, ChangeFlags{};
    String AppearanceBuffer;
    Tints FaceTints;
    bool operator==(const PlayerAppearance&) const noexcept = default;
    void Serialize(TiltedPhoques::Buffer::Writer&) const noexcept;
    void Deserialize(TiltedPhoques::Buffer::Reader&) noexcept;
};

struct RequestPlayerAppearance final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestPlayerAppearance;
    RequestPlayerAppearance() : ClientMessage(Opcode) {}
    PlayerAppearance Appearance;
    bool operator==(const RequestPlayerAppearance& aOther) const noexcept { return Appearance == aOther.Appearance; }
    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override { Appearance.Serialize(aWriter); }
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override { ClientMessage::DeserializeRaw(aReader); Appearance.Deserialize(aReader); }
};

struct NotifyPlayerAppearance final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyPlayerAppearance;
    NotifyPlayerAppearance() : ServerMessage(Opcode) {}
    PlayerAppearance Appearance;
    bool operator==(const NotifyPlayerAppearance& aOther) const noexcept { return Appearance == aOther.Appearance; }
    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override { Appearance.Serialize(aWriter); }
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override { ServerMessage::DeserializeRaw(aReader); Appearance.Deserialize(aReader); }
};
