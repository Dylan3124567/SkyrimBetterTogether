#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Allocator.hpp>
#include <TiltedCore/Buffer.hpp>

#include <optional>

#include <Structs/Inventory.h>
#include <catch2/catch.hpp>

namespace
{
template <class T> void DecodeInto(const T& acSource, T& aDestination)
{
    TiltedPhoques::Buffer buffer(4096);
    TiltedPhoques::Buffer::Writer writer(&buffer);
    acSource.Serialize(writer);
    TiltedPhoques::Buffer::Reader reader(&buffer);
    aDestination.Deserialize(reader);
}
} // namespace

TEST_CASE("Inventory preserves distinct enchantments", "[encoding.inventory]")
{
    Inventory::Entry first;
    first.BaseId = GameId(0, 0x123);
    first.Count = 1;
    first.ExtraEnchantId = GameId(0xFFFFFFFF, 0x456);
    first.EnchantData.IsWeapon = true;
    first.EnchantData.Effects.push_back({10.f, 5, 20, 30.f, GameId(0, 0x789)});
    auto second = first;
    REQUIRE(first.CanBeMerged(second));

    SECTION("Magnitude")
    {
        second.EnchantData.Effects[0].Magnitude += 1.f;
    }
    SECTION("Area")
    {
        ++second.EnchantData.Effects[0].Area;
    }
    SECTION("Duration")
    {
        ++second.EnchantData.Effects[0].Duration;
    }
    SECTION("Cost")
    {
        second.EnchantData.Effects[0].RawCost += 1.f;
    }
    SECTION("Effect ID")
    {
        ++second.EnchantData.Effects[0].EffectId.BaseId;
    }
    SECTION("Effect count")
    {
        second.EnchantData.Effects.clear();
    }
    SECTION("Enchantment type")
    {
        second.EnchantData.IsWeapon = false;
    }

    REQUIRE(first != second);
    REQUIRE_FALSE(first.CanBeMerged(second));
    Inventory inventory;
    inventory.AddOrRemoveEntry(first);
    inventory.AddOrRemoveEntry(second);
    REQUIRE(inventory.Entries.size() == 2);
    inventory.AddOrRemoveEntry(first);
    REQUIRE(inventory.Entries.size() == 2);
    REQUIRE(inventory.Entries[0].Count == 2);
    REQUIRE(inventory.Entries[1].Count == 1);
}

TEST_CASE("Inventory decoding replaces previous contents", "[encoding.inventory]")
{
    Inventory source;
    Inventory::Entry item;
    item.BaseId = GameId(0, 0x123);
    item.Count = 2;
    item.EnchantData.IsWeapon = true;
    item.EnchantData.Effects.push_back({10.f, 5, 20, 30.f, GameId(0, 0x789)});
    source.Entries.push_back(item);
    source.CurrentMagicEquipment.LeftHandSpell = GameId(0, 1);
    source.CurrentMagicEquipment.RightHandSpell = GameId(0, 2);
    source.CurrentMagicEquipment.Shout = GameId(0, 3);

    SECTION("Full snapshots do not accumulate items")
    {
        Inventory destination = source;
        DecodeInto(source, destination);
        REQUIRE(destination.Entries.size() == 1);
        REQUIRE(destination == source);
        DecodeInto(Inventory{}, destination);
        REQUIRE(destination.Entries.empty());
        REQUIRE(destination.CurrentMagicEquipment == MagicEquipment{});
    }

    SECTION("Repeated entry decoding does not accumulate effects")
    {
        auto destination = item;
        DecodeInto(item, destination);
        REQUIRE(destination.EnchantData.Effects.size() == 1);
        REQUIRE(destination == item);
        DecodeInto(Inventory::Entry{}, destination);
        REQUIRE(destination.EnchantData.Effects.empty());
        REQUIRE(destination == Inventory::Entry{});
    }

    SECTION("Absent magic slots clear previously equipped spells and shouts")
    {
        auto destination = source.CurrentMagicEquipment;
        MagicEquipment replacement;
        replacement.RightHandSpell = GameId(0, 4);
        DecodeInto(replacement, destination);
        REQUIRE(destination == replacement);
        DecodeInto(MagicEquipment{}, destination);
        REQUIRE(destination == MagicEquipment{});
    }
}

TEST_CASE("Inventory equality includes magic equipment", "[encoding.inventory]")
{
    Inventory first;
    Inventory second;
    REQUIRE(first == second);
    SECTION("Left spell")
    {
        second.CurrentMagicEquipment.LeftHandSpell = GameId(0, 1);
    }
    SECTION("Right spell")
    {
        second.CurrentMagicEquipment.RightHandSpell = GameId(0, 2);
    }
    SECTION("Shout")
    {
        second.CurrentMagicEquipment.Shout = GameId(0, 3);
    }
    REQUIRE(first != second);
}

TEST_CASE("Equipment snapshots clear worn flags and retain inventory contents", "[encoding.inventory]")
{
    Inventory inventory;
    for (uint32_t id = 1; id <= 32; ++id)
    {
        Inventory::Entry item;
        item.BaseId = GameId(0, id);
        item.Count = 2;
        item.ExtraWorn = id % 2 == 0;
        item.ExtraWornLeft = id % 2 != 0;
        inventory.Entries.push_back(item);
    }

    Inventory equipment;
    auto item = inventory.Entries.back();
    item.ExtraWorn = false;
    item.ExtraWornLeft = true;
    equipment.Entries.push_back(item);
    equipment.CurrentMagicEquipment.Shout = GameId(0, 123);
    inventory.UpdateEquipment(equipment);
    REQUIRE(inventory.Entries.size() == 32);
    for (size_t i = 0; i < inventory.Entries.size(); ++i)
    {
        REQUIRE(inventory.Entries[i].Count == 2);
        REQUIRE_FALSE(inventory.Entries[i].ExtraWorn);
        REQUIRE(inventory.Entries[i].ExtraWornLeft == (i == 31));
    }
    REQUIRE(inventory.CurrentMagicEquipment == equipment.CurrentMagicEquipment);
    inventory.UpdateEquipment(Inventory{});
    REQUIRE(std::none_of(inventory.Entries.begin(), inventory.Entries.end(), [](const auto& acEntry) { return acEntry.IsWorn(); }));
    REQUIRE(inventory.CurrentMagicEquipment == MagicEquipment{});
}

TEST_CASE("Inventory lookups and count changes preserve entry data", "[encoding.inventory]")
{
    Inventory inventory;
    GameId id(0, 0x123);
    REQUIRE_FALSE(inventory.GetEntryById(id));
    REQUIRE(inventory.GetEntryCountById(id) == 0);

    Inventory::Entry item;
    item.BaseId = id;
    item.Count = 2;
    item.EnchantData.Effects.push_back({10.f, 5, 20, 30.f, GameId(0, 0x789)});
    inventory.AddOrRemoveEntry(item);
    REQUIRE(inventory.GetEntryById(id) == item);
    REQUIRE(inventory.GetEntryCountById(id) == 2);

    item.Count = -1;
    inventory.AddOrRemoveEntry(item);
    REQUIRE(inventory.GetEntryCountById(id) == 1);
    REQUIRE(inventory.Entries[0].EnchantData.Effects.size() == 1);
    inventory.AddOrRemoveEntry(item);
    REQUIRE(inventory.Entries.empty());
}
