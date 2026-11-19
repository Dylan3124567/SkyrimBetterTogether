#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>
#include <Structs/ActorValues.h>
#include <Structs/AnimationVariables.h>
#include <Structs/Factions.h>
#include <Structs/QuestLog.h>
#include <glm/glm.hpp>
#include <Structs/Movement.h>
#include <catch2/catch.hpp>
#include <limits>

TEST_CASE("Movement preserves the ownership epoch and following packet fields", "[encoding.sync]")
{
    for (uint32_t epoch : {0u, 1u, 256u, UINT32_MAX})
    {
        Movement source;
        source.OwnershipEpoch = epoch;
        source.Position = glm::vec3(100.f, 200.f, 300.f);
        source.Direction = 1.f;
        source.IsHeldInPlace = epoch % 2 == 1;
        TiltedPhoques::Buffer buffer(1024);
        TiltedPhoques::Buffer::Writer writer(&buffer);
        source.Serialize(writer);
        TiltedPhoques::Serialization::WriteVarInt(writer, 0x123456);
        Movement destination;
        TiltedPhoques::Buffer::Reader reader(&buffer);
        destination.Deserialize(reader);
        REQUIRE(destination == source);
        REQUIRE(TiltedPhoques::Serialization::ReadVarInt(reader) == 0x123456);
        ++destination.OwnershipEpoch;
        REQUIRE(destination != source);
    }
}

TEST_CASE("Quest log limits preserve the following packet fields", "[encoding.sync]")
{
    for (size_t count : {size_t{0}, size_t{1}, size_t{65535}, size_t{65536}})
    {
        QuestLog source;
        source.Entries.resize(count, {GameId(1, 0x3372B), 100});
        TiltedPhoques::Buffer buffer(1024 * 1024);
        TiltedPhoques::Buffer::Writer writer(&buffer);
        source.Serialize(writer);
        TiltedPhoques::Serialization::WriteVarInt(writer, 0x123456);
        TiltedPhoques::Buffer::Reader reader(&buffer);
        QuestLog destination;
        destination.Entries.resize(2);
        destination.Deserialize(reader);
        source.Entries.resize(std::min(count, size_t{65535}));
        REQUIRE(destination == source);
        REQUIRE(TiltedPhoques::Serialization::ReadVarInt(reader) == 0x123456);
    }
}

TEST_CASE("Actor value snapshots replace both current and maximum values", "[encoding.sync]")
{
    ActorValues source;
    source.ActorValuesList[24] = 75.f;
    source.ActorMaxValuesList[24] = 150.f;
    ActorValues destination = source;
    destination.ActorMaxValuesList[24] = 100.f;
    REQUIRE(destination != source);
    destination.ActorValuesList[25] = 50.f;
    destination.ActorMaxValuesList[25] = 75.f;
    for (int i = 0; i < 2; ++i)
    {
        TiltedPhoques::Buffer buffer(256);
        TiltedPhoques::Buffer::Writer writer(&buffer);
        source.Serialize(writer);
        TiltedPhoques::Buffer::Reader reader(&buffer);
        destination.Deserialize(reader);
        REQUIRE(destination == source);
        source = {};
    }
}

TEST_CASE("Faction limits cannot corrupt the following packet fields", "[encoding.sync]")
{
    for (size_t count : {size_t{0}, size_t{1}, size_t{511}, size_t{512}, size_t{1024}})
    {
        Factions source;
        source.NpcFactions.resize(count);
        source.ExtraFactions.resize(count);
        for (size_t i = 0; i < count; ++i)
        {
            source.NpcFactions[i].Id = GameId(1, static_cast<uint32_t>(i + 1));
            source.ExtraFactions[i].Id = GameId(2, static_cast<uint32_t>(i + 1));
        }
        TiltedPhoques::Buffer buffer(16384);
        TiltedPhoques::Buffer::Writer writer(&buffer);
        source.Serialize(writer);
        TiltedPhoques::Serialization::WriteVarInt(writer, 0x123456);
        TiltedPhoques::Buffer::Reader reader(&buffer);
        Factions destination;
        destination.Deserialize(reader);
        REQUIRE(destination.NpcFactions.size() == std::min(count, size_t{511}));
        REQUIRE(destination.ExtraFactions.size() == std::min(count, size_t{511}));
        REQUIRE(TiltedPhoques::Serialization::ReadVarInt(reader) == 0x123456);
        for (size_t i = 0; i < destination.NpcFactions.size(); ++i)
        {
            REQUIRE(destination.NpcFactions[i] == source.NpcFactions[i]);
            REQUIRE(destination.ExtraFactions[i] == source.ExtraFactions[i]);
        }
    }
}

TEST_CASE("Animation bit tables tolerate truncation and reject impossible counts", "[encoding.sync]")
{
    AnimationVariables vars;
    TiltedPhoques::Vector<bool> bits(17, true);
    vars.String_to_VectorBool(TiltedPhoques::String(1, '\x01'), bits);
    REQUIRE(bits[0]);
    for (size_t i = 1; i < bits.size(); ++i)
        REQUIRE_FALSE(bits[i]);

    vars.Booleans = {true, false, true};
    vars.Integers = {42};
    vars.Floats = {1.25f};
    const auto previous = vars;
    for (uint64_t count : {uint64_t{9}, std::numeric_limits<uint64_t>::max()})
    {
        TiltedPhoques::Buffer buffer(256);
        TiltedPhoques::Buffer::Writer writer(&buffer);
        TiltedPhoques::Serialization::WriteVarInt(writer, count);
        TiltedPhoques::Serialization::WriteVarInt(writer, count);
        TiltedPhoques::Serialization::WriteVarInt(writer, count);
        TiltedPhoques::Serialization::WriteString(writer, "x");
        TiltedPhoques::Buffer::Reader reader(&buffer);
        vars.ApplyDiff(reader);
        REQUIRE(vars == previous);
    }
    TiltedPhoques::Buffer buffer(256);
    TiltedPhoques::Buffer::Writer writer(&buffer);
    vars.GenerateDiff({}, writer);
    TiltedPhoques::Buffer::Reader reader(&buffer);
    AnimationVariables decoded;
    decoded.ApplyDiff(reader);
    REQUIRE(decoded == vars);
}
