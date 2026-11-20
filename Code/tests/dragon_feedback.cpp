#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>
#include <glm/glm.hpp>

#include <catch2/catch.hpp>

#include <Messages/ClientMessageFactory.h>
#include <Structs/QuestRole.h>

using namespace TiltedPhoques;

TEST_CASE("A quest alias tells a temporary actor apart", "[dragon]")
{
    // WIChangeLocation09 (0x50AF1) alias 12 "Dragon", a Create Reference to lvlDragon
    REQUIRE(QuestRole::IsRole(0x50AF1, 12, true));
    // DGIntimidateQuest's opponent is alias 0
    REQUIRE(QuestRole::IsRole(0x47AE6, 0, true));
    REQUIRE_FALSE(QuestRole::IsRole(0x50AF1, 12, false));
    REQUIRE_FALSE(QuestRole::IsRole(0, 12, true));
    REQUIRE_FALSE(QuestRole::IsRole(0x50AF1, QuestRole::cMaxAliasId + 1, true));
}

TEST_CASE("Two party members' dragons of one town attack are one dragon", "[dragon]")
{
    using QuestRole::IsSameSpawn;

    // Upstream #712: both games' story managers started the attack, each made a dragon and saw the other's too
    REQUIRE(IsSameSpawn(true, false, true, true, false, false));
    // Both corpses, as when they come back after a reload
    REQUIRE(IsSameSpawn(true, false, true, true, true, true));

    // Another quest or alias
    REQUIRE_FALSE(IsSameSpawn(false, false, true, true, false, false));
    // The requester's own earlier dragon: the quest ran again and made a new one
    REQUIRE_FALSE(IsSameSpawn(true, true, true, true, false, false));
    // A player outside the party, or with none (solo)
    REQUIRE_FALSE(IsSameSpawn(true, false, false, true, false, false));
    // The party split up, each part met its own dragon
    REQUIRE_FALSE(IsSameSpawn(true, false, true, false, false, false));
    // A living dragon is never the corpse of the last one
    REQUIRE_FALSE(IsSameSpawn(true, false, true, true, true, false));
    REQUIRE_FALSE(IsSameSpawn(true, false, true, true, false, true));
}

TEST_CASE("The assignment request carries the quest role", "[dragon]")
{
    AssignCharacterRequest request;
    request.Cookie = 7;
    request.ReferenceId = GameId(0xFFFFFFFF, 0xFF001234);
    request.FormId = GameId(0, 0x5EACE);
    request.RoleQuestId = GameId(0, 0x50AF1);
    request.RoleAliasId = 12;

    Buffer buffer(4000);
    Buffer::Writer writer(&buffer);
    request.Serialize(writer);

    Buffer::Reader reader(&buffer);
    const ClientMessageFactory factory;
    auto pMessage = factory.Extract(reader);
    REQUIRE(pMessage);
    REQUIRE(pMessage->GetOpcode() == request.GetOpcode());

    const auto pRequest = CastUnique<AssignCharacterRequest>(std::move(pMessage));
    REQUIRE(pRequest->RoleQuestId == request.RoleQuestId);
    REQUIRE(pRequest->RoleAliasId == 12);
    REQUIRE(pRequest->ReferenceId == request.ReferenceId);
}
