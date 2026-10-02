#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DungeonQuestMgr.h"
#include "ObjectMgr.h"
#include "Map.h"
#include "Player.h"
#include "PoolMgr.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"

#include <algorithm>
#include <cmath>
#include <string>
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
// How far the guide steps off the arrival point, so players do not appear inside it.
constexpr float ArrivalOffset = 3.0f;

enum GuideText
{
    TEXT_NO_QUESTS_KNOWN,
    TEXT_NO_QUESTS_AVAILABLE,
    TEXT_PREVIOUS_PAGE,
    TEXT_NEXT_PAGE,
    TEXT_ACCEPT_ALL,
    TEXT_GOODBYE,
    TEXT_NOT_ACCEPTED,
    TEXT_ACCEPTED_COUNT,
    TEXT_NONE_ACCEPTED,
    TEXT_COUNT
};

char const* const GuideTextEn[TEXT_COUNT] =
{
    "No dungeon quests are known for this location.",
    "You have no available quests for this dungeon.",
    "Previous page",
    "Next page",
    "Accept All Available Quests",
    "Goodbye",
    "This quest could not be accepted.",
    "{} dungeon quests accepted.",
    "No dungeon quests could be accepted."
};

char const* const GuideTextRu[TEXT_COUNT] =
{
    "Для этого места заданий подземелья не найдено.",
    "Доступных заданий этого подземелья нет.",
    "Предыдущая страница",
    "Следующая страница",
    "Взять все доступные задания",
    "До свидания",
    "Не удалось взять это задание.",
    "Взято заданий подземелья: {}.",
    "Ни одно задание подземелья взять не удалось."
};

char const* Text(Player* player, GuideText id)
{
    return player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_ruRU ? GuideTextRu[id] : GuideTextEn[id];
}

std::string QuestTitle(Player* player, uint32 questId)
{
    std::string title = sDungeonQuestMgr->GetQuestTitle(questId);
    LocaleConstant locale = player->GetSession()->GetSessionDbLocaleIndex();
    if (locale != LOCALE_enUS)
        if (QuestLocale const* questLocale = sObjectMgr->GetQuestLocale(questId))
            ObjectMgr::GetLocaleString(questLocale->Title, locale, title);
    return title;
}

bool IsGuideAvailable(Player* player, Creature* creature)
{
    return sDungeonQuestMgr->IsEnabled() && player->IsAlive()
        && player->GetMap() == creature->GetMap()
        && sDungeonQuestMgr->IsDungeonMap(player->GetMapId());
}

Quest const* GetEligibleQuest(Player* player, uint32 questId)
{
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    // GetQuestStatus reports rewarded one-time quests as REWARDED; repeatables
    // return NONE and CanTakeQuest applies their daily/weekly limits.
    if (!quest || player->GetQuestStatus(questId) != QUEST_STATUS_NONE)
        return nullptr;

    // Pooled quests (heroic/normal dungeon dailies) are offered only while active.
    if (sPoolMgr->IsPartOfAPool<Quest>(questId) && !sPoolMgr->IsSpawnedObject<Quest>(questId))
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
        ChatHandler(player->GetSession()).SendSysMessage(Text(player, TEXT_NO_QUESTS_KNOWN));
        CloseGossipMenuFor(player);
        return;
    }

    std::vector<uint32> quests = GetEligibleQuests(player);
    if (quests.empty())
    {
        ChatHandler(player->GetSession()).SendSysMessage(Text(player, TEXT_NO_QUESTS_AVAILABLE));
        CloseGossipMenuFor(player);
        return;
    }

    uint32 lastPage = static_cast<uint32>((quests.size() - 1) / PageSize);
    page = std::min(page, lastPage);
    std::size_t begin = static_cast<std::size_t>(page) * PageSize;
    std::size_t end = std::min(begin + PageSize, quests.size());
    for (std::size_t index = begin; index < end; ++index)
        AddGossipItemFor(player, GOSSIP_ICON_DOT, QuestTitle(player, quests[index]),
            QuestSender, quests[index]);

    if (page > 0)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, Text(player, TEXT_PREVIOUS_PAGE), PageSender, page - 1);
    if (page < lastPage)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, Text(player, TEXT_NEXT_PAGE), PageSender, page + 1);
    if (EnableAcceptAll)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, Text(player, TEXT_ACCEPT_ALL), ControlSender, AcceptAllAction);
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, Text(player, TEXT_GOODBYE), ControlSender, GoodbyeAction);
    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
}

class npc_dungeon_quest_guide : public CreatureScript
{
public:
    npc_dungeon_quest_guide() : CreatureScript("npc_dungeon_quest_guide") { }

    struct npc_dungeon_quest_guideAI : public ScriptedAI
    {
        npc_dungeon_quest_guideAI(Creature* creature) : ScriptedAI(creature) { }

        // Spawn rows sit exactly on the instance arrival point. Before anyone can
        // see the guide, step it forward along the arrival facing (stopping at
        // walls, snapped to the ground) and turn it back toward the arrival.
        void InitializeAI() override
        {
            CreatureData const* data = me->GetCreatureData();
            if (data && me->IsInWorld())
            {
                Position arrival(data->posX, data->posY, data->posZ, data->orientation);
                Position pos = arrival;
                me->Relocate(arrival);
                me->MovePositionToFirstCollision(pos, ArrivalOffset, 0.0f);

                // Small moves only, and never across a cell border: Relocate does not regrid.
                if (arrival.GetExactDist2d(&pos) > 1.0f && std::fabs(pos.GetPositionZ() - arrival.GetPositionZ()) < 3.0f
                    && Acore::ComputeCellCoord(arrival.GetPositionX(), arrival.GetPositionY())
                        == Acore::ComputeCellCoord(pos.GetPositionX(), pos.GetPositionY()))
                {
                    pos.SetOrientation(pos.GetAngle(&arrival));
                    me->Relocate(pos);
                    me->SetHomePosition(pos);
                }
            }

            ScriptedAI::InitializeAI();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_dungeon_quest_guideAI(creature);
    }

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
                ChatHandler(player->GetSession()).SendSysMessage(Text(player, TEXT_NOT_ACCEPTED));
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
                ChatHandler(player->GetSession()).PSendSysMessage(Text(player, TEXT_ACCEPTED_COUNT), accepted);
            else
                ChatHandler(player->GetSession()).SendSysMessage(Text(player, TEXT_NONE_ACCEPTED));
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
