#include <Messages/RequestHelgenReady.h>
#include <Messages/NotifyHelgenState.h>
#include <TiltedCore/Serialization.hpp>

using namespace TiltedPhoques;
void RequestHelgenReady::SerializeRaw(Buffer::Writer& aWriter) const noexcept
{
    aWriter.WriteBits(Session, 32);
    aWriter.WriteBits(ScenePhases, 32);
    aWriter.WriteBits(State, 8);
    Serialization::WriteString(aWriter, Name);
    aWriter.WriteBits(LeaderAtBlock, 1);
}
void RequestHelgenReady::DeserializeRaw(Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    uint64_t value{};
    aReader.ReadBits(value, 32); Session = static_cast<uint32_t>(value);
    aReader.ReadBits(value, 32); ScenePhases = static_cast<uint32_t>(value);
    aReader.ReadBits(value, 8); State = static_cast<uint8_t>(value);
    Name = Serialization::ReadString(aReader);
    aReader.ReadBits(value, 1); LeaderAtBlock = value != 0;
}
void NotifyHelgenState::SerializeRaw(Buffer::Writer& aWriter) const noexcept
{
    aWriter.WriteBits(Session, 32);
    aWriter.WriteBits(ScenePhases, 32);
    aWriter.WriteBits(Phase, 8);
    aWriter.WriteBits(Ready, 32);
    aWriter.WriteBits(Total, 32);
    aWriter.WriteBits(LeaderFinished, 1);
    aWriter.WriteBits(LeaderAtBlock, 1);
}
void NotifyHelgenState::DeserializeRaw(Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);
    uint64_t value{};
    aReader.ReadBits(value, 32); Session = static_cast<uint32_t>(value);
    aReader.ReadBits(value, 32); ScenePhases = static_cast<uint32_t>(value);
    aReader.ReadBits(value, 8); Phase = static_cast<uint8_t>(value);
    aReader.ReadBits(value, 32); Ready = static_cast<uint32_t>(value);
    aReader.ReadBits(value, 32); Total = static_cast<uint32_t>(value);
    aReader.ReadBits(value, 1); LeaderFinished = value != 0;
    aReader.ReadBits(value, 1); LeaderAtBlock = value != 0;
}
