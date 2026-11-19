#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Allocator.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>
#include <optional>
#include <glm/glm.hpp>
#include <Messages/ClientMessageFactory.h>
#include <Messages/ServerMessageFactory.h>
#include <Structs/SleepBarrier.h>
#include <catch2/catch.hpp>
#include <array>

TEST_CASE("Shared sleep waits for everyone and advances each chunk exactly once", "[sleep]")
{
    SleepBarrier sleep;
    const std::array<uint32_t, 2> pair{1, 2};
    REQUIRE_FALSE(sleep.Begin(1, 1, 0, 900));
    REQUIRE_FALSE(sleep.Begin(1, 1, 25, 900));
    REQUIRE_FALSE(sleep.Begin(1, 1, 8, 0));
    REQUIRE_FALSE(sleep.Begin(1, 1, 8, 901));
    REQUIRE(sleep.Begin(1, 1, 8, 900));
    REQUIRE_FALSE(sleep.Start(pair));
    REQUIRE_FALSE(sleep.Tick(1, 1, 0));
    REQUIRE_FALSE(sleep.Begin(1, 2, 8, 900));
    REQUIRE(sleep.Begin(2, 7, 1, 900));
    REQUIRE(sleep.Start(pair));
    REQUIRE(sleep.RemainingSeconds == 3600);
    REQUIRE_FALSE(sleep.Begin(3, 1, 8, 900));
    for (uint32_t step = 1; step <= 4; ++step)
    {
        REQUIRE_FALSE(sleep.Tick(99, 1, step));
        REQUIRE_FALSE(sleep.Tick(1, 99, step));
        REQUIRE_FALSE(sleep.Tick(1, 1, step + 1));
        REQUIRE_FALSE(sleep.Tick(1, 1, step));
        REQUIRE_FALSE(sleep.Tick(1, 1, step));
        REQUIRE(sleep.Tick(2, 7, step));
        REQUIRE_FALSE(sleep.Tick(2, 7, step));
        REQUIRE(sleep.RemainingSeconds == 3600 - step * 900);
    }
    REQUIRE_FALSE(sleep.Tick(1, 1, 5));
    sleep = {}; // Cancellation/disconnect invalidates all previous votes and grants.
    REQUIRE_FALSE(sleep.Tick(2, 7, 4));
    REQUIRE(sleep.Begin(1, 2, 1, 900));
    REQUIRE(sleep.Begin(2, 8, 1, 600));
    REQUIRE_FALSE(sleep.Start(pair));
}

TEST_CASE("Sleep protocol survives the actual message factories", "[sleep][encoding]")
{
    TiltedPhoques::Buffer buffer(256);
    TiltedPhoques::Buffer::Writer writer(&buffer);
    RequestSleep request;
    request.Session = UINT32_MAX; request.Step = 96;
    request.Seconds = 900; request.Hours = 24; request.State = RequestSleep::Tick;
    request.Serialize(writer);
    NotifySleep response;
    response.Session = request.Session; response.Step = 97;
    response.Ready = 2; response.Total = 2; response.State = NotifySleep::Finished;
    response.Time.Time = 23.75f;
    response.Serialize(writer);
    TiltedPhoques::Buffer::Reader reader(&buffer);
    auto decodedRequest = ClientMessageFactory{}.Extract(reader);
    REQUIRE(decodedRequest);
    REQUIRE(decodedRequest->GetOpcode() == kRequestSleep);
    REQUIRE(static_cast<RequestSleep&>(*decodedRequest) == request);
    auto decodedResponse = ServerMessageFactory{}.Extract(reader);
    REQUIRE(decodedResponse);
    REQUIRE(decodedResponse->GetOpcode() == kNotifySleep);
    REQUIRE(static_cast<NotifySleep&>(*decodedResponse) == response);
}
