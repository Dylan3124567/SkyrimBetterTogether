#include <catch2/catch.hpp>

#include <limits>

#include <Systems/PartyExperience.h>

using namespace PartyExperience;

TEST_CASE("Only skill progress counts towards the next kill's share", "[skills]")
{
    REQUIRE(AddSkillUse(0.f, 12.f) == 12.f);
    REQUIRE(AddSkillUse(12.f, 3.f) == 15.f);

    // Levelling up takes the progress from near the threshold back to near zero
    REQUIRE(AddSkillUse(15.f, -380.f) == 15.f);
    REQUIRE(AddSkillUse(15.f, 0.f) == 15.f);
    REQUIRE(AddSkillUse(15.f, std::numeric_limits<float>::quiet_NaN()) == 15.f);
    REQUIRE(AddSkillUse(15.f, std::numeric_limits<float>::infinity()) == 15.f);
}

TEST_CASE("A party kill only reaches someone who is fighting too", "[skills]")
{
    // Nobody who walks around town gets anything, however often the party kills things
    REQUIRE_FALSE(IsFighting(1000.0, -1.0));
    REQUIRE_FALSE(IsFighting(1000.0, 1000.0 - cFightWindow - 1.0));

    REQUIRE(IsFighting(1000.0, 1000.0));
    REQUIRE(IsFighting(1000.0, 1000.0 - cFightWindow));
    REQUIRE(IsFighting(0.0, 0.0));
}

TEST_CASE("A party kill can't level a skill by itself", "[skills]")
{
    REQUIRE(GetKillCredit(5.f, 393.f) == 5.f);
    REQUIRE(GetKillCredit(100000.f, 393.f) == Approx(393.f * cMaxKillCredit));

    REQUIRE(GetKillCredit(0.f, 393.f) == 0.f);
    REQUIRE(GetKillCredit(-50.f, 393.f) == 0.f);
    REQUIRE(GetKillCredit(50.f, 0.f) == 0.f);
    REQUIRE(GetKillCredit(std::numeric_limits<float>::quiet_NaN(), 393.f) == 0.f);
    REQUIRE(GetKillCredit(50.f, std::numeric_limits<float>::quiet_NaN()) == 0.f);
}
