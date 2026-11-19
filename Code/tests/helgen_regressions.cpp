#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Allocator.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <Structs/HelgenBarrier.h>
#include <catch2/catch.hpp>
#include <array>
#include <Messages/ClientMessageFactory.h>
#include <Messages/ServerMessageFactory.h>

TEST_CASE("Helgen readiness packets preserve session and counts", "[encoding.helgen]")
{
    for (uint8_t state : {0, 1, 2, 255})
    {
        TiltedPhoques::Buffer buffer(256);
        TiltedPhoques::Buffer::Writer writer(&buffer);
        RequestHelgenReady request;
        request.Session = UINT32_MAX;
        request.State = state;
        request.ScenePhases = state == 1 ? 0xFFFF003F : 0xFFFEFFFE;
        request.Name = state == 1 ? "Bridge Two" : "";
        request.LeaderAtBlock = state == 1;
        request.Serialize(writer);
        NotifyHelgenState notification;
        notification.Session = UINT32_MAX;
        notification.Phase = state;
        notification.ScenePhases = request.ScenePhases;
        notification.Ready = 2;
        notification.Total = 8;
        notification.LeaderFinished = state >= 2;
        notification.LeaderAtBlock = request.LeaderAtBlock;
        notification.Serialize(writer);
        TiltedPhoques::Buffer::Reader reader(&buffer);
        auto receivedRequest = ClientMessageFactory{}.Extract(reader);
        REQUIRE(receivedRequest);
        REQUIRE(receivedRequest->GetOpcode() == kRequestHelgenReady);
        REQUIRE(static_cast<RequestHelgenReady&>(*receivedRequest) == request);
        auto receivedNotification = ServerMessageFactory{}.Extract(reader);
        REQUIRE(receivedNotification);
        REQUIRE(receivedNotification->GetOpcode() == kNotifyHelgenState);
        REQUIRE(static_cast<NotifyHelgenState&>(*receivedNotification) == notification);
    }
}

TEST_CASE("Helgen waits for every current member and rejects late or premature readiness", "[helgen]")
{
    HelgenBarrier barrier;
    const std::array<uint32_t, 1> solo{1};
    const std::array<uint32_t, 2> pair{1, 2};
    const std::array<uint32_t, 3> trio{1, 2, 3};
    REQUIRE_FALSE(barrier.Report(1, 1, solo));
    REQUIRE(barrier.Report(1, 0, solo));
    REQUIRE(barrier.Report(1, 1, solo));
    REQUIRE(barrier.Phase == 0);
    REQUIRE_FALSE(barrier.Report(1, 2, solo));
    REQUIRE_FALSE(barrier.Report(9, 0, pair));
    REQUIRE(barrier.Report(2, 0, pair));
    REQUIRE(barrier.Phase == 0);
    REQUIRE(barrier.Report(1, 0, pair)); // Duplicate registration cannot clear readiness.
    REQUIRE(barrier.Count(pair) == 1);
    REQUIRE(barrier.Report(2, 1, pair));
    REQUIRE(barrier.Phase == 1);
    REQUIRE_FALSE(barrier.Report(3, 0, trio));
    REQUIRE(barrier.Report(1, 2, pair));
    REQUIRE(barrier.Phase == 1);
    REQUIRE(barrier.Report(2, 2, pair));
    REQUIRE(barrier.Phase == 2);
    REQUIRE_FALSE(barrier.Report(1, 255, pair));

    HelgenBarrier disconnect;
    REQUIRE(disconnect.Report(1, 0, pair));
    REQUIRE(disconnect.Report(2, 0, pair));
    REQUIRE(disconnect.Report(1, 1, pair));
    disconnect.Update(solo);
    REQUIRE(disconnect.Phase == 0);
    REQUIRE_FALSE(disconnect.Players.contains(2));
    REQUIRE(disconnect.Report(2, 0, pair));
    REQUIRE(disconnect.Report(2, 1, pair));
    REQUIRE(disconnect.Report(1, 2, pair));
    disconnect.Update(solo);
    REQUIRE(disconnect.Phase == 2);
}

TEST_CASE("Live appearance packets preserve actor ownership and face data", "[encoding.helgen]")
{
    TiltedPhoques::Buffer buffer(4096);
    TiltedPhoques::Buffer::Writer writer(&buffer);
    RequestPlayerAppearance request;
    request.Appearance.ActorId = UINT32_MAX;
    request.Appearance.OwnershipEpoch = 17;
    request.Appearance.ChangeFlags = 0x2000800;
    request.Appearance.AppearanceBuffer = String("head\0body", 9);
    auto& tint = request.Appearance.FaceTints.Entries.emplace_back();
    tint.Alpha = 0.75f;
    tint.Color = 0x12345678;
    tint.Type = 3;
    request.Serialize(writer);
    NotifyPlayerAppearance notify;
    notify.Appearance = request.Appearance;
    notify.Serialize(writer);
    TiltedPhoques::Buffer::Reader reader(&buffer);
    auto receivedRequest = ClientMessageFactory{}.Extract(reader);
    REQUIRE(receivedRequest);
    REQUIRE(receivedRequest->GetOpcode() == kRequestPlayerAppearance);
    REQUIRE(static_cast<RequestPlayerAppearance&>(*receivedRequest) == request);
    auto receivedNotify = ServerMessageFactory{}.Extract(reader);
    REQUIRE(receivedNotify);
    REQUIRE(receivedNotify->GetOpcode() == kNotifyPlayerAppearance);
    REQUIRE(static_cast<NotifyPlayerAppearance&>(*receivedNotify) == notify);
}
