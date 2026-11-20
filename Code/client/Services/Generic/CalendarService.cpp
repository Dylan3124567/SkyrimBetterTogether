#include <Services/CalendarService.h>
#include <Services/HelgenService.h>

#include <Events/DisconnectedEvent.h>
#include <Events/UpdateEvent.h>
#include <Messages/ServerTimeSettings.h>
#include <Messages/Sleep.h>
#include <Interface/UI.h>
#include <OverlayApp.hpp>
#include <atomic>
#include <World.h>

#include <Forms/TESObjectCELL.h>
#include <PlayerCharacter.h>
#include <TimeManager.h>

constexpr float kTransitionSpeed = 5.f;

namespace
{
std::atomic<uint32_t> s_sleepSession{}, s_sleepPermit{};
std::atomic<bool> s_sleepActive{}, s_sleepRunning{}, s_sleepFinished{};
thread_local bool s_nativeSleepTick = false;
using StartSleep = void(PlayerCharacter*, uint32_t);
using UpdateSleep = void(PlayerCharacter*);
using StopSleep = void(PlayerCharacter*, bool);
StartSleep* RealStartSleep;
UpdateSleep* RealUpdateSleep;
StopSleep* RealStopSleep;

void SendSleep(RequestSleep aMessage)
{
    World::Get().GetRunner().Queue([aMessage]() {
        if (World::Get().GetTransport().IsOnline() && aMessage.Session == s_sleepSession.load())
            World::Get().GetTransport().Send(aMessage);
    });
}

void HookStartSleep(PlayerCharacter* apPlayer, uint32_t aHours)
{
    if (World::Get().GetTransport().IsOnline() && apPlayer == PlayerCharacter::Get())
    {
        // This is the bed-only entry point. Ordinary waiting is not a sleep vote.
        static VersionDbPtr<uint32_t> sleepStep(371317);
        const uint32_t seconds = *sleepStep.Get();
        if (!aHours || aHours > 24 || !seconds || seconds > 3600 || 3600 % seconds)
            return;
        s_sleepActive = true;
        s_sleepRunning = false;
        s_sleepFinished = false;
        s_sleepPermit = 0;
        RequestSleep request;
        request.Session = ++s_sleepSession;
        request.Hours = static_cast<uint8_t>(aHours);
        request.Seconds = static_cast<uint16_t>(seconds);
        request.State = RequestSleep::Begin;
        SendSleep(request);
    }
    RealStartSleep(apPlayer, aHours);
}

void HookUpdateSleep(PlayerCharacter* apPlayer)
{
    if (!s_sleepActive || apPlayer != PlayerCharacter::Get())
        return RealUpdateSleep(apPlayer);
    const uint32_t step = s_sleepPermit.exchange(0);
    if (!step || s_sleepFinished)
        return;
    // Only this native sleep increment may bypass the multiplayer clock lock.
    // Keep native recovery, elapsed-day accounting and Papyrus sleep events intact.
    auto* time = TimeData::Get();
    time->rawDaysPassed = time->GameDaysPassed->f - time->GameHour->f / 24.f;
    s_nativeSleepTick = true;
    RealUpdateSleep(apPlayer);
    s_nativeSleepTick = false;
    RequestSleep request;
    request.Session = s_sleepSession;
    request.Step = step;
    request.State = RequestSleep::Tick;
    SendSleep(request);
}

void HookStopSleep(PlayerCharacter* apPlayer, bool aRecover)
{
    RealStopSleep(apPlayer, aRecover);
    if (apPlayer != PlayerCharacter::Get() || !s_sleepActive)
        return;
    s_sleepFinished = true;
    if (!s_nativeSleepTick)
    {
        RequestSleep request;
        request.Session = s_sleepSession;
        request.State = RequestSleep::Cancel;
        SendSleep(request);
    }
}

void EndSleep(bool aCompleted)
{
    s_sleepPermit = 0;
    s_sleepRunning = false;
    if (s_sleepActive && !s_sleepFinished.exchange(true))
        if (auto* player = PlayerCharacter::Get())
            RealStopSleep(player, aCompleted);
    // Keep the tick gate until the native menu has closed; otherwise its final
    // update could advance another chunk after we shortened a longer sleep.
}
}

bool CalendarService::s_gameClockLocked = false;

bool CalendarService::AllowGameTick() noexcept
{
    return !s_gameClockLocked || s_nativeSleepTick;
}

CalendarService::CalendarService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport)
    : m_world(aWorld), m_transport(aTransport)
{
    m_timeUpdateConnection = aDispatcher.sink<ServerTimeSettings>().connect<&CalendarService::OnTimeUpdate>(this);
    m_updateConnection = aDispatcher.sink<UpdateEvent>().connect<&CalendarService::HandleUpdate>(this);
    m_disconnectedConnection = aDispatcher.sink<DisconnectedEvent>().connect<&CalendarService::OnDisconnected>(this);
    m_sleepConnection = aDispatcher.sink<NotifySleep>().connect<&CalendarService::OnSleep>(this);
}

void CalendarService::OnTimeUpdate(const ServerTimeSettings& acMessage) noexcept
{
    // disable the game clock
    ToggleGameClock(false);
    m_onlineTime.m_timeModel.TimeScale = acMessage.timeModel.TimeScale;
    m_onlineTime.m_timeModel.Time = acMessage.timeModel.Time;

    if (m_world.GetServerSettings().SyncPlayerCalendar)
    {
        m_onlineTime.m_timeModel.Day = acMessage.timeModel.Day;
        m_onlineTime.m_timeModel.Month = acMessage.timeModel.Month;
        m_onlineTime.m_timeModel.Year = acMessage.timeModel.Year;
    }
    else
    {
        m_onlineTime.m_timeModel.Day = m_offlineTime.m_timeModel.Day;
        m_onlineTime.m_timeModel.Month = m_offlineTime.m_timeModel.Month;
        m_onlineTime.m_timeModel.Year = m_offlineTime.m_timeModel.Year;
    }
}

void CalendarService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    EndSleep(false);
    SleepStatus("");
    // signal a time transition
    m_fadeTimer = 0.f;
    ToggleGameClock(true);
}

float CalendarService::TimeInterpolate(const TimeModel& aFrom, TimeModel& aTo) const
{
    const auto t = aTo.Time - aFrom.Time;
    if (t < 0.f)
    {
        const auto v = t + 24.f;
        // interpolate on the time difference, not the time
        const auto x = TiltedPhoques::Lerp(0.f, v, m_fadeTimer / kTransitionSpeed) + aFrom.Time;

        return TiltedPhoques::Mod(x, 24.f);
    }

    return TiltedPhoques::Lerp(aFrom.Time, aTo.Time, m_fadeTimer / kTransitionSpeed);
}

void CalendarService::ToggleGameClock(bool aEnable)
{
    auto* pGameTime = TimeData::Get();
    m_offlineTime.m_timeModel.Day = pGameTime->GameDay->f;
    m_offlineTime.m_timeModel.Month = pGameTime->GameMonth->f;
    m_offlineTime.m_timeModel.Year = pGameTime->GameYear->f;
    m_offlineTime.m_timeModel.Time = pGameTime->GameHour->f;
    m_offlineTime.m_timeModel.TimeScale = pGameTime->TimeScale->f;

    s_gameClockLocked = !aEnable;
}

void CalendarService::HandleUpdate(const UpdateEvent& aEvent) noexcept
{
    if (s_sleepActive && s_sleepFinished && !s_sleepRunning && UI::Get() && !UI::Get()->GetMenuOpen("Sleep/Wait Menu"))
        s_sleepActive = false;
    if (s_gameClockLocked)
    {
        const auto updateDelta = static_cast<float>(aEvent.Delta);
        auto* pGameTime = TimeData::Get();

        if (!m_lastTick)
            m_lastTick = m_world.GetTick();

        const auto now = m_world.GetTick();

        // we got disconnected or the client got ahead of us
        if (now < m_lastTick)
            return;

        const auto delta = now - m_lastTick;
        m_lastTick = now;

        if (s_sleepRunning)
            return; // Native sleep already updates GameDaysPassed; never count it twice.

        m_onlineTime.Update(delta);
        // Keep the scripted intro's lighting while the creation menu and scene run.
        if (m_world.ctx().at<HelgenService>().DeferActors())
        {
            m_fadeTimer = 0.f;
            return;
        }
        pGameTime->TimeScale->f = m_onlineTime.m_timeModel.TimeScale;
        pGameTime->GameDay->f = m_onlineTime.m_timeModel.Day;
        pGameTime->GameMonth->f = m_onlineTime.m_timeModel.Month;
        pGameTime->GameYear->f = m_onlineTime.m_timeModel.Year;
        // In bed waiting for the others the days stand still, as in vanilla's paused sleep menu. Survival restores
        // what passed between OnSleepStart and OnSleepStop, so the wait would count as sleep for whoever lay down first.
        const bool cWaitingInBed = s_sleepActive && !s_sleepFinished;
        pGameTime->GameDaysPassed->f = m_daysPassed.Advance(pGameTime->GameDaysPassed->f, cWaitingInBed ? 0.0 : m_onlineTime.GetDeltaTime(delta));

        // time transition in
        if (m_fadeTimer < kTransitionSpeed)
        {
            pGameTime->GameHour->f = TimeInterpolate(m_offlineTime.m_timeModel, m_onlineTime.m_timeModel);
            m_fadeTimer += updateDelta;
        }
        else
            pGameTime->GameHour->f = m_onlineTime.m_timeModel.Time;

        // The game's own clock rebuilds the days from this plus GameHour / 24 once it runs again (native sleep,
        // disconnecting), which would otherwise drop every day that passed while connected.
        pGameTime->rawDaysPassed = pGameTime->GameDaysPassed->f - pGameTime->GameHour->f / 24.f;
    }
}

void CalendarService::SleepStatus(const String& aText) noexcept
{
    spdlog::info("Shared sleep: {}", aText);
    auto args = CefListValue::Create();
    args->SetString(0, aText.c_str());
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("helgenStatus", args);
}

void CalendarService::OnSleep(const NotifySleep& aMessage) noexcept
{
    // A cancelled native menu may close before the server's cancellation reply.
    // Still clear its waiting banner and accept the final clock for this session.
    if (aMessage.Session != s_sleepSession.load())
        return;
    if (aMessage.State == NotifySleep::Waiting)
    {
        SleepStatus(fmt::format("Sleeping: {}/{} ready. Everyone must use a bed and choose Sleep. Cancel to get up.", aMessage.Ready, aMessage.Total).c_str());
        return;
    }
    ServerTimeSettings time;
    time.timeModel = aMessage.Time;
    OnTimeUpdate(time);
    m_lastTick = m_world.GetTick();
    m_fadeTimer = kTransitionSpeed;
    if (aMessage.State == NotifySleep::Advance && !s_sleepFinished)
    {
        if (!s_sleepRunning.exchange(true))
            SleepStatus("");
        s_sleepPermit = aMessage.Step;
    }
    else if (aMessage.State == NotifySleep::Finished || aMessage.State == NotifySleep::Cancelled)
    {
        EndSleep(aMessage.State == NotifySleep::Finished);
        SleepStatus("");
    }
}

static TiltedPhoques::Initializer s_sleepHooks([] {
    // Source-only implementation authorised by the user. Installed 1.7.104
    // disassembly confirms bed start, 900-second update and native stop paths.
    // No IDA/Ghidra MCP verification; Survival effects still need runtime proof.
    POINTER_SKYRIMSE(StartSleep, start, 40414);
    POINTER_SKYRIMSE(UpdateSleep, update, 40485);
    POINTER_SKYRIMSE(StopSleep, stop, 40417);
    RealStartSleep = start.Get();
    RealUpdateSleep = update.Get();
    RealStopSleep = stop.Get();
    TP_HOOK(&RealStartSleep, HookStartSleep);
    TP_HOOK(&RealUpdateSleep, HookUpdateSleep);
    TP_HOOK(&RealStopSleep, HookStopSleep);
});
