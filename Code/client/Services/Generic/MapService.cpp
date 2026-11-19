#include <Services/MapService.h>

#include <World.h>

#include <Events/SetWaypointEvent.h>
#include <Events/RemoveWaypointEvent.h>
#include <Events/UpdateEvent.h>
#include <Messages/NotifySetWaypoint.h>
#include <Messages/NotifyRemoveWaypoint.h>
#include <Messages/RequestSetWaypoint.h>
#include <Messages/RequestRemoveWaypoint.h>
#include <Messages/NotifyPlayerPositions.h>

#include <Services/PartyService.h>
#include <PlayerCharacter.h>
#include <Forms/TESObjectCELL.h>
#include <Forms/TESWorldSpace.h>
#include <Interface/Menus/CompassMarkers.h>
#include <Systems/UiRules.h>

MapService::MapService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld), m_dispatcher(aDispatcher), m_transport(aTransport)
{
    m_updateConnection = m_dispatcher.sink<UpdateEvent>().connect<&MapService::OnUpdate>(this);
    m_playerPositionsConnection = m_dispatcher.sink<NotifyPlayerPositions>().connect<&MapService::OnNotifyPlayerPositions>(this);
    m_playerNotifySetWaypointConnection =
        m_dispatcher.sink<NotifySetWaypoint>().connect<&MapService::OnNotifySetWaypoint>(this);
    m_playerNotifyRemoveWaypointConnection =
        m_dispatcher.sink<NotifyRemoveWaypoint>().connect<&MapService::OnNotifyRemoveWaypoint>(this);
    m_playerSetWaypointConnection =
        m_dispatcher.sink<SetWaypointEvent>().connect<&MapService::OnSetWaypoint>(this);
    m_playerRemoveWaypointConnection =
        m_dispatcher.sink<RemoveWaypointEvent>().connect<&MapService::OnRemoveWaypoint>(this);
}

void MapService::OnUpdate(const UpdateEvent&) noexcept
{
    if (!m_transport.IsConnected())
    {
        m_remotePlayers.clear();
        m_waypoints.clear();
    }

    // Waypoints are only shown to the party: forget those of players who left, and show new members the local one
    const auto& members = m_world.GetPartyService().GetPartyMembers();
    if (members != m_partyMembers)
    {
        for (auto it = m_waypoints.begin(); it != m_waypoints.end();)
            it = std::find(members.begin(), members.end(), it->first) == members.end() ? m_waypoints.erase(it) : std::next(it);

        const bool cHasNewMember = std::any_of(members.begin(), members.end(), [this](uint32_t aMember) {
            return aMember != m_transport.GetLocalPlayerId() && std::find(m_partyMembers.begin(), m_partyMembers.end(), aMember) == m_partyMembers.end();
        });
        if (cHasNewMember && m_ownWaypoint && m_transport.IsConnected())
            m_transport.Send(*m_ownWaypoint);

        m_partyMembers = members;
    }

    std::array<CompassMarkers::Target, CompassMarkers::kMaxTargets> targets;
    size_t targetCount = 0;

    const auto* pPlayer = PlayerCharacter::Get();
    if (pPlayer && pPlayer->parentCell)
    {
        const auto* pWorldSpace = pPlayer->GetWorldSpace();
        const uint32_t worldSpaceId = pWorldSpace ? pWorldSpace->formID : 0;
        const uint32_t cellId = pPlayer->parentCell->formID;

        const auto addTarget = [&](uint32_t aWorldSpaceId, uint32_t aCellId, const glm::vec3& acPosition, uint32_t aPlayerId, UiRules::CompassKind aKind) {
            if (targetCount < targets.size() && UiRules::SharesCompassSpace(worldSpaceId, cellId, aWorldSpaceId, aCellId))
            {
                auto& target = targets[targetCount++];
                target.Position.x = acPosition.x;
                target.Position.y = acPosition.y;
                target.Position.z = acPosition.z;
                target.Colour = UiRules::PlayerColour(aPlayerId, members);
                target.Kind = aKind;
            }
        };

        // A loaded player moves every frame, the server's position is up to a second old.
        Vector<uint32_t> loadedPlayers;
        auto view = m_world.view<FormIdComponent, PlayerComponent>();
        for (auto entity : view)
        {
            const auto* pActor = Cast<Actor>(TESForm::GetById(view.get<FormIdComponent>(entity).Id));
            if (!pActor || pActor == pPlayer || !pActor->parentCell)
                continue;

            const uint32_t cPlayerId = view.get<PlayerComponent>(entity).Id;
            const auto* pActorWorldSpace = pActor->GetWorldSpace();
            addTarget(pActorWorldSpace ? pActorWorldSpace->formID : 0, pActor->parentCell->formID, {pActor->position.x, pActor->position.y, pActor->position.z}, cPlayerId, UiRules::CompassKind::Player);
            loadedPlayers.push_back(cPlayerId);
        }

        for (const auto& player : m_remotePlayers)
        {
            if (std::find(loadedPlayers.begin(), loadedPlayers.end(), player.PlayerId) == loadedPlayers.end())
                addTarget(player.WorldSpaceId, player.CellId, player.Position, player.PlayerId, UiRules::CompassKind::Player);
        }

        // Map markers only exist out in a worldspace, a cell of 0 matches any cell of it
        for (const auto& [playerId, waypoint] : m_waypoints)
            addTarget(waypoint.WorldSpaceId, 0, waypoint.Position, playerId, UiRules::CompassKind::Waypoint);
    }

    const bool cInParty = m_world.GetPartyService().IsInParty();
    const int32_t cLocalColour = cInParty ? static_cast<int32_t>(UiRules::PlayerColour(m_transport.GetLocalPlayerId(), members)) : -1;
    CompassMarkers::SetTargets(targets.data(), targetCount, cLocalColour);
}
void MapService::OnNotifyPlayerPositions(const NotifyPlayerPositions& acMessage) noexcept
{
    const ModSystem& modSystem = m_world.GetModSystem();

    m_remotePlayers.clear();
    for (const auto& entry : acMessage.Entries)
    {
        if (entry.PlayerId == m_transport.GetLocalPlayerId())
            continue;

        const uint32_t worldSpaceId = entry.WorldSpaceId ? modSystem.GetGameId(entry.WorldSpaceId) : 0;
        m_remotePlayers.push_back({entry.PlayerId, worldSpaceId, modSystem.GetGameId(entry.CellId), entry.Position});
    }
}

void MapService::OnSetWaypoint(const SetWaypointEvent& acMessage) noexcept
{
    RequestSetWaypoint request{};
    request.Position = acMessage.Position;

    ModSystem& modSystem = m_world.Get().GetModSystem();
    modSystem.GetServerModId(acMessage.WorldSpaceFormID, request.WorldSpaceFormID);

    m_ownWaypoint = request;
    if (m_transport.IsConnected())
        m_transport.Send(request);
}

void MapService::OnRemoveWaypoint(const RemoveWaypointEvent& acMessage) noexcept
{
    m_ownWaypoint.reset();
    if (!m_transport.IsConnected())
        return;

    RequestRemoveWaypoint request{};
    m_transport.Send(request);
}

void MapService::OnNotifySetWaypoint(const NotifySetWaypoint& acMessage) noexcept
{
    // Kept apart from the game's own custom marker, which stays the local player's
    const uint32_t cWorldSpaceId = m_world.GetModSystem().GetGameId(acMessage.WorldSpaceFormID);
    m_waypoints[acMessage.PlayerId] = {cWorldSpaceId, glm::vec3(acMessage.Position.x, acMessage.Position.y, acMessage.Position.z)};
}

void MapService::OnNotifyRemoveWaypoint(const NotifyRemoveWaypoint& acMessage) noexcept
{
    m_waypoints.erase(acMessage.PlayerId);
}