#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Allocator.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>

#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <catch2/catch.hpp>

#include <Messages/ClientMessageFactory.h>
#include <Messages/ServerMessageFactory.h>
#include <Structs/DropLedger.h>
#include <Systems/DropRules.h>

using namespace TiltedPhoques;

namespace
{
DroppedItem MakeStack()
{
    DroppedItem item;
    item.CellId = GameId(1, 0x9712);
    item.WorldSpaceId = GameId(0, 0x3C);
    item.Coords = GridCellCoords(5, -11);
    item.Position = glm::vec3(20575.f, -45066.f, -32.f);
    item.Item.BaseId = GameId(0, 0x1397D);
    item.Item.Count = 14;
    item.Item.ExtraHealth = 1.2f;
    return item;
}
} // namespace

TEST_CASE("One item can only be taken once", "[drop.ledger]")
{
    DropLedger ledger;
    const uint32_t cId = ledger.Add(MakeStack(), 1).Id;

    // Both players reached for it, the second request only arrives later
    REQUIRE(ledger.Take(cId).has_value());
    REQUIRE_FALSE(ledger.Take(cId).has_value());
    REQUIRE(ledger.GetRecords().empty());
}

TEST_CASE("Taking an item gives back the whole stack", "[drop.ledger]")
{
    DropLedger ledger;
    const uint32_t cId = ledger.Add(MakeStack(), 1).Id;

    const auto cItem = ledger.Take(cId);
    REQUIRE(cItem.has_value());
    REQUIRE(cItem->Item.Count == 14);
    REQUIRE(cItem->Item.ExtraHealth == 1.2f);
}

TEST_CASE("Items have an ID clients can tell apart from a missing one", "[drop.ledger]")
{
    DropLedger ledger;
    const uint32_t cFirst = ledger.Add(MakeStack(), 1).Id;
    const uint32_t cSecond = ledger.Add(MakeStack(), 1).Id;

    REQUIRE(cFirst != 0);
    REQUIRE(cSecond != 0);
    REQUIRE(cFirst != cSecond);

    // An ID is not reused, a late request for a taken item must not get whatever came next
    ledger.Take(cSecond);
    REQUIRE(ledger.Add(MakeStack(), 1).Id != cSecond);
}

TEST_CASE("Only the dropper is told the cookie", "[drop.ledger]")
{
    DroppedItem item = MakeStack();
    item.Cookie = 77;

    DropLedger ledger;
    REQUIRE(ledger.Add(item, 1).Cookie == 0);
    REQUIRE(ledger.GetRecords()[0].Item.Cookie == 0);
}

TEST_CASE("A leaving player takes only their own items along", "[drop.ledger]")
{
    DropLedger ledger;
    const uint32_t cFirst = ledger.Add(MakeStack(), 1).Id;
    const uint32_t cOther = ledger.Add(MakeStack(), 2).Id;
    const uint32_t cSecond = ledger.Add(MakeStack(), 1).Id;

    const auto cIds = ledger.TakeAllFrom(1);

    REQUIRE(cIds.size() == 2);
    REQUIRE(std::find(cIds.begin(), cIds.end(), cFirst) != cIds.end());
    REQUIRE(std::find(cIds.begin(), cIds.end(), cSecond) != cIds.end());
    REQUIRE(ledger.GetRecords().size() == 1);
    REQUIRE(ledger.Take(cOther).has_value());
    REQUIRE_FALSE(ledger.Take(cFirst).has_value());
}

TEST_CASE("Drop messages survive the network", "[drop.encoding]")
{
    Buffer buffer(1000);

    {
        RequestItemDrop request;
        request.Item = MakeStack();
        request.Item.Cookie = 12;

        Buffer::Writer writer(&buffer);
        request.Serialize(writer);

        Buffer::Reader reader(&buffer);
        auto pMessage = ClientMessageFactory{}.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == request.GetOpcode());
        REQUIRE(*CastUnique<RequestItemDrop>(std::move(pMessage)) == request);
    }

    {
        RequestDroppedItems request;
        request.CellId = GameId(1, 2);
        request.WorldSpaceId = GameId(0, 0x3C);
        request.Coords = GridCellCoords(-3, 4);

        Buffer::Writer writer(&buffer);
        request.Serialize(writer);

        Buffer::Reader reader(&buffer);
        auto pMessage = ClientMessageFactory{}.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == request.GetOpcode());
        REQUIRE(*CastUnique<RequestDroppedItems>(std::move(pMessage)) == request);
    }

    {
        RequestDroppedItemPickUp request;
        request.Id = 0xFFFFFFFF;

        Buffer::Writer writer(&buffer);
        request.Serialize(writer);

        Buffer::Reader reader(&buffer);
        auto pMessage = ClientMessageFactory{}.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == request.GetOpcode());
        REQUIRE(*CastUnique<RequestDroppedItemPickUp>(std::move(pMessage)) == request);
    }

    {
        NotifyDroppedItems notify;
        notify.Items.push_back(MakeStack());
        notify.Items.push_back(MakeStack());
        notify.Items[0].Id = 1;
        notify.Items[1].Id = 2;
        notify.Items[1].Cookie = 3;

        Buffer::Writer writer(&buffer);
        notify.Serialize(writer);

        Buffer::Reader reader(&buffer);
        auto pMessage = ServerMessageFactory{}.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == notify.GetOpcode());
        REQUIRE(*CastUnique<NotifyDroppedItems>(std::move(pMessage)) == notify);
    }

    {
        NotifyDroppedItemRemoved notify;
        notify.Id = 9;
        notify.Granted = true;

        Buffer::Writer writer(&buffer);
        notify.Serialize(writer);

        Buffer::Reader reader(&buffer);
        auto pMessage = ServerMessageFactory{}.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == notify.GetOpcode());
        REQUIRE(*CastUnique<NotifyDroppedItemRemoved>(std::move(pMessage)) == notify);
    }
}

TEST_CASE("A camera away from the player is not used to place an item", "[drop.rules]")
{
    const glm::vec3 cPlayer{20575.f, -45066.f, -80.f};

    // Eye height, and a third person camera pulled back
    REQUIRE_FALSE(DropRules::IsDropOriginStale({cPlayer.x, cPlayer.y, cPlayer.z + 120.f}, cPlayer));
    REQUIRE_FALSE(DropRules::IsDropOriginStale({cPlayer.x - 150.f, cPlayer.y + 150.f, cPlayer.z + 150.f}, cPlayer));

    // The play log's item at (-46.5, -89.9, 59.8) needs a camera near the origin, the engine only adds about 150 to it
    REQUIRE(DropRules::IsDropOriginStale({0.f, 0.f, 0.f}, cPlayer));
    REQUIRE(DropRules::IsDropOriginStale({0.f, 0.f, 120.f}, cPlayer));
}

TEST_CASE("Items are shown where they can be reached", "[drop.rules]")
{
    const glm::vec3 cItem{5000.f, 5000.f, 0.f};

    SECTION("Interiors only care about the cell, their coordinates mean nothing elsewhere")
    {
        REQUIRE(DropRules::ShouldShowDrop(true, true, false, cItem, {-90000.f, 0.f, 0.f}));
        REQUIRE_FALSE(DropRules::ShouldShowDrop(true, false, false, cItem, cItem));
    }

    SECTION("Exteriors need the same world space and loaded surroundings")
    {
        REQUIRE(DropRules::ShouldShowDrop(false, false, true, cItem, {cItem.x + DropRules::cSpawnDistance - 1.f, cItem.y, 0.f}));
        REQUIRE_FALSE(DropRules::ShouldShowDrop(false, false, true, cItem, {cItem.x + DropRules::cSpawnDistance + 1.f, cItem.y, 0.f}));
        REQUIRE_FALSE(DropRules::ShouldShowDrop(false, true, false, cItem, cItem));
    }
}

TEST_CASE("A copy without the item's extra data is never picked up as it is", "[drop.rules]")
{
    REQUIRE(DropRules::CanPickUpReference(true, true));
    REQUIRE(DropRules::CanPickUpReference(true, false));
    REQUIRE(DropRules::CanPickUpReference(false, false));
    REQUIRE_FALSE(DropRules::CanPickUpReference(false, true));
}
