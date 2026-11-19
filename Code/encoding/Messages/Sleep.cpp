#include <Messages/Sleep.h>

void RequestSleep::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    aWriter.WriteBits(Session, 32); aWriter.WriteBits(Step, 32);
    aWriter.WriteBits(Seconds, 16); aWriter.WriteBits(Hours, 8); aWriter.WriteBits(State, 8);
}
void RequestSleep::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    uint64_t value{};
    aReader.ReadBits(value, 32); Session = static_cast<uint32_t>(value);
    aReader.ReadBits(value, 32); Step = static_cast<uint32_t>(value);
    aReader.ReadBits(value, 16); Seconds = static_cast<uint16_t>(value);
    aReader.ReadBits(value, 8); Hours = static_cast<uint8_t>(value);
    aReader.ReadBits(value, 8); State = static_cast<uint8_t>(value);
}
void NotifySleep::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    aWriter.WriteBits(Session, 32); aWriter.WriteBits(Step, 32);
    aWriter.WriteBits(Ready, 32); aWriter.WriteBits(Total, 32); aWriter.WriteBits(State, 8);
    Time.Serialize(aWriter);
}
void NotifySleep::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);
    uint64_t value{};
    aReader.ReadBits(value, 32); Session = static_cast<uint32_t>(value);
    aReader.ReadBits(value, 32); Step = static_cast<uint32_t>(value);
    aReader.ReadBits(value, 32); Ready = static_cast<uint32_t>(value);
    aReader.ReadBits(value, 32); Total = static_cast<uint32_t>(value);
    aReader.ReadBits(value, 8); State = static_cast<uint8_t>(value);
    Time.Deserialize(aReader);
}
