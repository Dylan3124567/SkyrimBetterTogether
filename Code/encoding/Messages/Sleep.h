#pragma once
#include "Message.h"
#include <Structs/TimeModel.h>

struct RequestSleep final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestSleep;
    RequestSleep() : ClientMessage(Opcode) {}
    enum : uint8_t { Begin, Tick, Cancel };
    uint32_t Session{}, Step{};
    uint16_t Seconds{};
    uint8_t Hours{}, State{};
    void SerializeRaw(TiltedPhoques::Buffer::Writer&) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader&) noexcept override;
    bool operator==(const RequestSleep& aOther) const noexcept { return Session == aOther.Session && Step == aOther.Step && Seconds == aOther.Seconds && Hours == aOther.Hours && State == aOther.State; }
};

struct NotifySleep final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifySleep;
    NotifySleep() : ServerMessage(Opcode) {}
    enum : uint8_t { Waiting, Advance, Finished, Cancelled };
    uint32_t Session{}, Step{}, Ready{}, Total{};
    uint8_t State{};
    TimeModel Time;
    void SerializeRaw(TiltedPhoques::Buffer::Writer&) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader&) noexcept override;
    bool operator==(const NotifySleep& aOther) const noexcept { return Session == aOther.Session && Step == aOther.Step && Ready == aOther.Ready && Total == aOther.Total && State == aOther.State && Time == aOther.Time; }
};
