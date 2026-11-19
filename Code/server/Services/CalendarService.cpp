#include <Services/CalendarService.h>

#include <GameServer.h>
#include <World.h>

#include <Events/PlayerJoinEvent.h>
#include <Events/PlayerLeaveEvent.h>
#include <Events/UpdateEvent.h>

#include <Messages/ServerTimeSettings.h>
#include <Messages/Sleep.h>

#include "Game/Player.h"

CalendarService::CalendarService(World& aWorld, entt::dispatcher& aDispatcher)
    : m_world(aWorld)
{
    m_updateConnection = aDispatcher.sink<UpdateEvent>().connect<&CalendarService::OnUpdate>(this);
    m_joinConnection = aDispatcher.sink<PlayerJoinEvent>().connect<&CalendarService::OnPlayerJoin>(this);
    m_leaveConnection = aDispatcher.sink<PlayerLeaveEvent>().connect<&CalendarService::OnPlayerLeave>(this);
    m_sleepConnection = aDispatcher.sink<PacketEvent<RequestSleep>>().connect<&CalendarService::OnSleep>(this);
}

void CalendarService::OnUpdate(const UpdateEvent&) noexcept
{
    if (!m_lastTick)
        m_lastTick = GameServer::Get()->GetTick();

    auto now = GameServer::Get()->GetTick();

    // client got ahead, we wait
    if (now < m_lastTick)
        return;

    auto delta = now - m_lastTick;
    m_lastTick = now;
    if (m_sleepDeadline && now >= m_sleepDeadline)
        CancelSleep();
    if (m_sleep.Step)
        return; // Native sleep steps, acknowledged by every client, own this clock temporarily.
    m_dateTime.Update(delta);
}

void CalendarService::OnPlayerJoin(const PlayerJoinEvent& acEvent) noexcept
{
    CancelSleep();
    ServerTimeSettings timeMsg;
    // the player with the furthest date is used
    bool playerHasFurthestTime = acEvent.PlayerTime.GetTimeInDays() > m_dateTime.GetTimeInDays();

    if (playerHasFurthestTime)
    {
        // Note that this doesn't set timescale because the server config should set that.
        if (!m_timeSetFromFirstPlayer)
        {
            m_dateTime.m_timeModel.Time = acEvent.PlayerTime.m_timeModel.Time;
            m_timeSetFromFirstPlayer = true;
        }
        m_dateTime.m_timeModel.Day = acEvent.PlayerTime.m_timeModel.Day;
        m_dateTime.m_timeModel.Month = acEvent.PlayerTime.m_timeModel.Month;
        m_dateTime.m_timeModel.Year = acEvent.PlayerTime.m_timeModel.Year;
    }
    timeMsg.timeModel.TimeScale = m_dateTime.m_timeModel.TimeScale;
    timeMsg.timeModel.Time = m_dateTime.m_timeModel.Time;
    timeMsg.timeModel.Day = m_dateTime.m_timeModel.Day;
    timeMsg.timeModel.Month = m_dateTime.m_timeModel.Month;
    timeMsg.timeModel.Year = m_dateTime.m_timeModel.Year;

    if (playerHasFurthestTime)
        GameServer::Get()->SendToPlayers(timeMsg);
    else
        acEvent.pPlayer->Send(timeMsg);
}

bool CalendarService::SetTime(int aHours, int aMinutes, float aScale) noexcept
{
    CancelSleep();
    m_dateTime.m_timeModel.TimeScale = aScale;

    if (aHours >= 0 && aHours <= 23 && aMinutes >= 0 && aMinutes <= 59)
    {
        // encode time as skyrim time
        auto minutes = static_cast<float>(aMinutes) * 0.17f;
        minutes = floor(minutes * 100) / 1000;
        m_dateTime.m_timeModel.Time = static_cast<float>(aHours) + minutes;

        SendTimeResync();

        GameServer::Get()->GetWorld().GetScriptService().HandleSetTime(aHours, aMinutes, aScale);
        return true;
    }
    return false;
}

bool CalendarService::SetDate(int aDay, int aMonth, float aYear) noexcept
{
    CancelSleep();
    if (aMonth >= 0 && aMonth < 12 && aYear >= 0 && aYear <= 999)
    {
        auto maxDays = m_dateTime.GetNumberOfDaysByMonthIndex(aMonth);
        if (aDay >= 0 && aDay < maxDays)
        {
            m_dateTime.m_timeModel.Day = aDay;
            m_dateTime.m_timeModel.Month = aMonth;
            m_dateTime.m_timeModel.Year = aYear;
            SendTimeResync();
            return true;
        }
    }
    return false;
}

void CalendarService::SendTimeResync() noexcept
{
    ServerTimeSettings timeMsg;
    timeMsg.timeModel.TimeScale = m_dateTime.m_timeModel.TimeScale;
    timeMsg.timeModel.Time = m_dateTime.m_timeModel.Time;
    timeMsg.timeModel.Day = m_dateTime.m_timeModel.Day;
    timeMsg.timeModel.Month = m_dateTime.m_timeModel.Month;
    timeMsg.timeModel.Year = m_dateTime.m_timeModel.Year;
    GameServer::Get()->SendToLoaded(timeMsg);
}

CalendarService::TTime CalendarService::GetTime() const noexcept
{
    const auto hour = floor(m_dateTime.m_timeModel.Time);
    const auto minutes = (m_dateTime.m_timeModel.Time - hour) / 17.f;

    const auto flatMinutes = static_cast<int>(ceil((minutes * 100.f) * 10.f));
    return {static_cast<int>(hour), flatMinutes};
}

CalendarService::TTime CalendarService::GetRealTime() noexcept
{
    const auto t = std::time(nullptr);
    int h = (t / 3600) % 24;
    int m = (t / 60) % 60;
    return {h, m};
}

CalendarService::TDate CalendarService::GetDate() const noexcept
{
    return {m_dateTime.m_timeModel.Day, m_dateTime.m_timeModel.Month, m_dateTime.m_timeModel.Year};
}

bool CalendarService::SetTimeScale(float aScale) noexcept
{
    CancelSleep();
    if (aScale >= 0.f && aScale <= 1000.f)
    {
        m_dateTime.m_timeModel.TimeScale = aScale;

        ServerTimeSettings timeMsg;
        timeMsg.timeModel.TimeScale = m_dateTime.m_timeModel.TimeScale;
        timeMsg.timeModel.Time = m_dateTime.m_timeModel.Time;
        timeMsg.timeModel.Day = m_dateTime.m_timeModel.Day;
        timeMsg.timeModel.Month = m_dateTime.m_timeModel.Month;
        timeMsg.timeModel.Year = m_dateTime.m_timeModel.Year;
        GameServer::Get()->SendToPlayers(timeMsg);
        return true;
    }

    return false;
}

void CalendarService::SendSleep(uint8_t aState) noexcept
{
    for (const auto& [id, entry] : m_sleep.Players)
        if (auto* player = m_world.GetPlayerManager().GetById(id))
        {
            NotifySleep message;
            message.Session = entry.Session;
            message.Step = m_sleep.Step;
            message.Ready = static_cast<uint32_t>(m_sleep.Players.size());
            message.Total = m_world.GetPlayerManager().Count();
            message.State = aState;
            message.Time = m_dateTime.m_timeModel;
            player->Send(message);
        }
}

void CalendarService::CancelSleep() noexcept
{
    if (m_sleep.Players.empty())
        return;
    SendSleep(NotifySleep::Cancelled);
    m_sleep = {};
    m_sleepDeadline = 0;
    SendTimeResync();
    spdlog::info("Shared sleep cancelled; clock resumed");
}

void CalendarService::OnPlayerLeave(const PlayerLeaveEvent&) noexcept
{
    CancelSleep();
}

void CalendarService::OnSleep(const PacketEvent<RequestSleep>& acEvent) noexcept
{
    const auto& request = acEvent.Packet;
    const uint32_t id = acEvent.pPlayer->GetId();
    if (request.State == RequestSleep::Begin)
    {
        if (!m_sleep.Begin(id, request.Session, request.Hours, request.Seconds))
        {
            NotifySleep denied;
            denied.Session = request.Session;
            denied.State = NotifySleep::Cancelled;
            denied.Time = m_dateTime.m_timeModel;
            acEvent.pPlayer->Send(denied);
            return;
        }
        if (!m_sleepDeadline)
            m_sleepDeadline = GameServer::Get()->GetTick() + 120000;
        Vector<uint32_t> players;
        for (auto* player : m_world.GetPlayerManager())
            players.push_back(player->GetId());
        if (m_sleep.Start({players.data(), players.size()}))
        {
            m_sleepDeadline = GameServer::Get()->GetTick() + 30000;
            spdlog::info("Shared sleep started: {} players, {} seconds", players.size(), m_sleep.RemainingSeconds);
            SendSleep(NotifySleep::Advance);
        }
        else if (m_sleep.Players.size() == players.size())
            CancelSleep(); // Different native sleep-step settings cannot share a clock safely.
        else
            SendSleep(NotifySleep::Waiting);
    }
    else if (request.State == RequestSleep::Tick && m_sleep.Tick(id, request.Session, request.Step))
    {
        m_dateTime.m_timeModel.Time += m_sleep.StepSeconds / 3600.f;
        m_dateTime.Update(0); // Includes midnight, month and year rollover.
        m_sleepDeadline = GameServer::Get()->GetTick() + 30000;
        const bool finished = m_sleep.RemainingSeconds == 0;
        SendSleep(finished ? NotifySleep::Finished : NotifySleep::Advance);
        if (finished)
        {
            spdlog::info("Shared sleep completed at {}", m_dateTime.m_timeModel.Time);
            m_sleep = {};
            m_sleepDeadline = 0;
            SendTimeResync();
        }
    }
    else if (request.State == RequestSleep::Cancel)
    {
        auto it = m_sleep.Players.find(id);
        if (it != m_sleep.Players.end() && it->second.Session == request.Session)
            CancelSleep();
    }
}
