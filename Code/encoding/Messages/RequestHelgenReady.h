#pragma once
#include "Message.h"

struct RequestHelgenReady final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestHelgenReady;
    RequestHelgenReady() : ClientMessage(Opcode) {}
    uint32_t Session{};
    uint32_t ScenePhases{0xFFFEFFFE}; // Main and dragon scenes: 16 bits each; FFFE unknown, FFFF stopped.
    TiltedPhoques::String Name{}; // Final character name, sent with readiness.
    uint8_t State{}; // Register, character complete, cinematic complete.
    bool LeaderAtBlock{};
    bool operator==(const RequestHelgenReady& aOther) const noexcept { return LeaderAtBlock == aOther.LeaderAtBlock && ScenePhases == aOther.ScenePhases && Session == aOther.Session && State == aOther.State && Name == aOther.Name; }
    void SerializeRaw(TiltedPhoques::Buffer::Writer&) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader&) noexcept override;
};
