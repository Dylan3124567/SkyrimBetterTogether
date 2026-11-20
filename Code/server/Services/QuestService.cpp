#include <GameServer.h>
#include <Components.h>

#include <World.h>
#include <Services/QuestService.h>

#include <Messages/RequestQuestUpdate.h>
#include <Messages/NotifyQuestUpdate.h>

#include <Structs/QuestSharing.h>

QuestService::QuestService(World& aWorld, entt::dispatcher& aDispatcher)
    : m_world(aWorld)
{
    m_questUpdateConnection = aDispatcher.sink<PacketEvent<RequestQuestUpdate>>().connect<&QuestService::OnQuestChanges>(this);
}

void QuestService::OnQuestChanges(const PacketEvent<RequestQuestUpdate>& acMessage) noexcept
{
    const auto& message = acMessage.Packet;
    if (!QuestSharing::IsValid(message.Status))
        return;

    auto* pPlayer = acMessage.pPlayer;

    auto& questComponent = pPlayer->GetQuestLogComponent();
    auto& entries = questComponent.QuestContent.Entries;

    auto questIt = std::find_if(entries.begin(), entries.end(), [&message](const auto& e) { return e.Id == message.Id; });

    NotifyQuestUpdate notify{};
    notify.Id = message.Id;
    notify.Stage = message.Stage;
    notify.Status = message.Status;
    notify.ClientQuestType = message.ClientQuestType;

    // See QuestSharing.h for the rules
    if (!QuestSharing::IsShared(message.ClientQuestType))
        return;

    // The player's own quest log, kept up to date for their character data
    if (QuestSharing::IsStop(message.Status))
    {
        spdlog::debug("Stopped quest: {:X}, stage: {}, status: {}", message.Id.LogFormat(), message.Stage, message.Status);

        if (questIt != entries.end())
            entries.erase(questIt);
    }
    else if (questIt == entries.end())
    {
        spdlog::debug("Started quest: {:X} stage: {}", message.Id.LogFormat(), message.Stage);

        auto& newQuest = entries.emplace_back();
        newQuest.Id = message.Id;
        newQuest.Stage = message.Stage;
    }
    else
    {
        spdlog::debug("Updated quest: {:X}, stage: {}", message.Id.LogFormat(), message.Stage);
        questIt->Stage = std::max(questIt->Stage, message.Stage);
    }

    const auto& partyComponent = acMessage.pPlayer->GetParty();
    PartyService::Party* pParty = m_world.GetPartyService().GetPlayerParty(pPlayer);
    if (!pParty)
        return;

    // Party members' scripts often reach the same start, stage or stop on their own, only relay it once. The relayed
    // status is the reported one: a start stays a start even if this player had the quest before, so it starts elsewhere,
    // and a stop keeps how the quest ended here, so it ends the same way elsewhere.
    if (!pParty->QuestRuns[message.Id].Relay(message.Status, message.Stage))
    {
        spdlog::debug("Dropped duplicate quest update from player {}, quest: {:X}, stage: {}, status: {}", pPlayer->GetId(), message.Id.LogFormat(), message.Stage, message.Status);
        return;
    }

    GameServer::Get()->SendToParty(notify, partyComponent, acMessage.GetSender());
}
