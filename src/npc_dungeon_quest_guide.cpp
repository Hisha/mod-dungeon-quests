#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DungeonQuestMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"

#include <algorithm>
#include <vector>

namespace
{
bool EnableAcceptAll = true;
constexpr uint32 QuestSender = 1;
constexpr uint32 PageSender = 2;
constexpr uint32 ControlSender = 3;
constexpr uint32 AcceptAllAction = 1;
constexpr uint32 GoodbyeAction = 2;
// Leave room for previous/next, accept-all and goodbye below the core's 32-item limit.
constexpr uint32 PageSize = 24;

bool IsGuideAvailable(Player* player, Creature* creature)
{
    return sDungeonQuestMgr->IsEnabled() && player->IsAlive()
        && player->GetMap() == creature->GetMap()
        && sDungeonQuestMgr->IsDungeonMap(player->GetMapId());
}

Quest const* GetEligibleQuest(Player* player, uint32 questId)
{
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    // IsQuestRewarded also covers previously rewarded repeatables, as requested.
    // GetQuestRewardStatus alone deliberately does not cover those in the core.
    if (!quest || player->GetQuestStatus(questId) != QUEST_STATUS_NONE || player->IsQuestRewarded(questId))
        return nullptr;

    // Core authority for disabled quests, chains, conditions, class/race, level,
    // reputation, exclusive groups, timers, quest-log and source-item capacity.
    return player->CanTakeQuest(quest, false) && player->CanAddQuest(quest, false) ? quest : nullptr;
}

std::vector<uint32> GetEligibleQuests(Player* player)
{
    std::vector<uint32> result;
    for (uint32 questId : sDungeonQuestMgr->GetDungeonQuests(player->GetMapId()))
        if (GetEligibleQuest(player, questId))
            result.push_back(questId);
    return result;
}

bool AcceptQuest(Player* player, Creature* creature, uint32 questId)
{
    if (!IsGuideAvailable(player, creature))
        return false;

    // Recheck current-map membership too: never trust a stale client selection.
    auto const& quests = sDungeonQuestMgr->GetDungeonQuests(player->GetMapId());
    if (std::find(quests.begin(), quests.end(), questId) == quests.end())
        return false;

    Quest const* quest = GetEligibleQuest(player, questId);
    if (!quest)
        return false;

    player->AddQuestAndCheckCompletion(quest, creature);
    QuestStatus status = player->GetQuestStatus(questId);
    return status == QUEST_STATUS_INCOMPLETE || status == QUEST_STATUS_COMPLETE;
}

void ShowMenu(Player* player, Creature* creature, uint32 page = 0)
{
    ClearGossipMenuFor(player);
    if (!IsGuideAvailable(player, creature)
        || sDungeonQuestMgr->GetDungeonQuests(player->GetMapId()).empty())
    {
        ChatHandler(player->GetSession()).SendSysMessage("No dungeon quests are known for this location.");
        CloseGossipMenuFor(player);
        return;
    }

    std::vector<uint32> quests = GetEligibleQuests(player);
    if (quests.empty())
    {
        ChatHandler(player->GetSession()).SendSysMessage("You have no available quests for this dungeon.");
        CloseGossipMenuFor(player);
        return;
    }

    uint32 lastPage = static_cast<uint32>((quests.size() - 1) / PageSize);
    page = std::min(page, lastPage);
    std::size_t begin = static_cast<std::size_t>(page) * PageSize;
    std::size_t end = std::min(begin + PageSize, quests.size());
    for (std::size_t index = begin; index < end; ++index)
        AddGossipItemFor(player, GOSSIP_ICON_DOT, sDungeonQuestMgr->GetQuestTitle(quests[index]),
            QuestSender, quests[index]);

    if (page > 0)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Previous page", PageSender, page - 1);
    if (page < lastPage)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Next page", PageSender, page + 1);
    if (EnableAcceptAll)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Accept All Available Quests", ControlSender, AcceptAllAction);
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Goodbye", ControlSender, GoodbyeAction);
    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
}

class npc_dungeon_quest_guide : public CreatureScript
{
public:
    npc_dungeon_quest_guide() : CreatureScript("npc_dungeon_quest_guide") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ShowMenu(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        ClearGossipMenuFor(player);
        if (!IsGuideAvailable(player, creature) || (sender == ControlSender && action == GoodbyeAction))
        {
            CloseGossipMenuFor(player);
            return true;
        }

        if (sender == PageSender)
        {
            ShowMenu(player, creature, action);
            return true;
        }

        if (sender == QuestSender)
        {
            if (!AcceptQuest(player, creature, action))
                ChatHandler(player->GetSession()).SendSysMessage("This quest could not be accepted.");
        }
        else if (sender == ControlSender && action == AcceptAllAction && EnableAcceptAll)
        {
            // Snapshot only currently eligible quests; never automatically traverse a chain.
            std::vector<uint32> quests = GetEligibleQuests(player);
            uint32 accepted = 0;
            for (uint32 questId : quests)
                if (AcceptQuest(player, creature, questId))
                    ++accepted;

            if (accepted)
                ChatHandler(player->GetSession()).PSendSysMessage("{} dungeon quests accepted.", accepted);
            else
                ChatHandler(player->GetSession()).SendSysMessage("No dungeon quests could be accepted.");
        }

        ShowMenu(player, creature);
        return true;
    }
};
}

void LoadDungeonQuestGuideConfig()
{
    EnableAcceptAll = sConfigMgr->GetOption<bool>("DungeonQuests.EnableAcceptAll", true);
}

void AddSC_npc_dungeon_quest_guide()
{
    new npc_dungeon_quest_guide();
}
