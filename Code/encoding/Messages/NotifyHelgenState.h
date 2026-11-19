#pragma once
#include "Message.h"

struct NotifyHelgenState final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyHelgenState;
    NotifyHelgenState() : ServerMessage(Opcode) {}
    uint32_t Session{};
    uint32_t ScenePhases{0xFFFEFFFE}; // Main and dragon scenes: 16 bits each; FFFE unknown, FFFF stopped.
    uint8_t Phase{}; // Creating characters, cinematics, gameplay, rejected.
    uint32_t Ready{};
    uint32_t Total{};
    bool LeaderFinished{};
    bool LeaderAtBlock{};
    bool operator==(const NotifyHelgenState& aOther) const noexcept { return LeaderAtBlock == aOther.LeaderAtBlock && ScenePhases == aOther.ScenePhases && Session == aOther.Session && Phase == aOther.Phase && Ready == aOther.Ready && Total == aOther.Total && LeaderFinished == aOther.LeaderFinished; }
    void SerializeRaw(TiltedPhoques::Buffer::Writer&) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader&) noexcept override;
};
