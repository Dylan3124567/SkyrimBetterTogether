
#include <Structs/QuestLog.h>

#include <TiltedCore/Serialization.hpp>
#include <Structs/Mods.h>
#include <algorithm>

using TiltedPhoques::Serialization;

bool QuestLog::operator==(const QuestLog& acRhs) const noexcept
{
    return Entries == acRhs.Entries;
}

bool QuestLog::operator!=(const QuestLog& acRhs) const noexcept
{
    return !this->operator==(acRhs);
}

void QuestLog::Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    const size_t count = std::min(Entries.size(), size_t{0xFFFF});
    aWriter.WriteBits(count, 16);
    for (size_t i = 0; i < count; ++i)
    {
        const auto& e = Entries[i];
        e.Id.Serialize(aWriter);

        // id's can be in the full 16 bit range
        aWriter.WriteBits(e.Stage, 16);
    }
}

void QuestLog::Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    uint64_t temp;
    aReader.ReadBits(temp, 16);

    const size_t count = temp & 0xFFFF;
    Entries.resize(count);

    for (auto& e : Entries)
    {
        e.Id.Deserialize(aReader);
        aReader.ReadBits(temp, 16);

        e.Stage = temp & 0xFFFF;
    }
}
