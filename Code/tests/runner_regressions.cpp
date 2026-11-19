#include <entt/entt.hpp>
#include <Services/RunnerService.h>
#include <Events/UpdateEvent.h>
#include <Events/SpellCastEvent.h>
#include <catch2/catch.hpp>

TEST_CASE("Runner callbacks can enqueue work without locking or running it in the same frame", "[sync.runner]")
{
    entt::dispatcher dispatcher;
    RunnerService runner(dispatcher);
    int count = 0;
    runner.Queue([&] {
        ++count;
        runner.Queue([&] { ++count; });
    });
    runner.OnUpdate(UpdateEvent{0.0});
    REQUIRE(count == 1);
    runner.OnUpdate(UpdateEvent{0.0});
    REQUIRE(count == 2);
    runner.OnUpdate(UpdateEvent{0.0});
    REQUIRE(count == 2);
}

TEST_CASE("Queued spell casts retain their original caster and casting state", "[sync.runner]")
{
    entt::dispatcher dispatcher;
    RunnerService runner(dispatcher);
    struct Listener
    {
        SpellCastEvent received{0, 0, false, 0, 0};
        void Receive(const SpellCastEvent& event) { received = event; }
    } listener;
    dispatcher.sink<SpellCastEvent>().connect<&Listener::Receive>(listener);
    {
        SpellCastEvent event{0x14, 1, true, 0x123, 0x456};
        runner.Trigger(event);
        event = {0, 0, false, 0, 0};
    }
    runner.OnUpdate(UpdateEvent{0.0});
    REQUIRE(listener.received.CasterFormID == 0x14);
    REQUIRE(listener.received.CastingSource == 1);
    REQUIRE(listener.received.IsDualCasting);
    REQUIRE(listener.received.SpellId == 0x123);
    REQUIRE(listener.received.DesiredTargetID == 0x456);
}
