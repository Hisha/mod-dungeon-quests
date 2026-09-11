#include "Chat.h"
#include "CommandScript.h"
#include "DungeonQuestMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"

void LoadDungeonQuestGuideConfig();
void AddSC_npc_dungeon_quest_guide();

using namespace Acore::ChatCommands;

namespace
{
// Runs automatic dungeon quest discovery and keeps the cached results fresh.
class dungeon_quests_worldscript : public WorldScript
{
public:
    dungeon_quests_worldscript() : WorldScript("dungeon_quests_worldscript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD,
        WORLDHOOK_ON_STARTUP
    }) { }

    void OnAfterConfigLoad(bool reload) override
    {
        sDungeonQuestMgr->LoadConfig();
        LoadDungeonQuestGuideConfig();
        if (reload && sDungeonQuestMgr->IsEnabled())
            sDungeonQuestMgr->Reload();
    }

    void OnStartup() override
    {
        if (sDungeonQuestMgr->IsEnabled())
            sDungeonQuestMgr->Load();
    }
};

class dungeon_quests_commandscript : public CommandScript
{
public:
    dungeon_quests_commandscript() : CommandScript("dungeon_quests_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable dungeonQuestCommandTable =
        {
            { "stats", HandleStatsCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "list",  HandleListCommand,  SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable commandTable =
        {
            { "dungeonquest", dungeonQuestCommandTable }
        };

        return commandTable;
    }

private:
    static bool HandleStatsCommand(ChatHandler* handler)
    {
        DungeonQuestStats const& stats = sDungeonQuestMgr->GetStats();
        handler->PSendSysMessage("Dungeon quest discovery:");
        handler->PSendSysMessage("{} dungeon maps", stats.DungeonCount);
        handler->PSendSysMessage("{} auto-discovered quest mappings", stats.DiscoveredMappings);
        handler->PSendSysMessage("{} force-includes", stats.ForceIncluded);
        handler->PSendSysMessage("{} force-excludes", stats.ForceExcluded);
        if (stats.InvalidOverrides != 0)
            handler->PSendSysMessage("{} invalid override(s) were logged and skipped", stats.InvalidOverrides);
        return true;
    }

    static bool HandleListCommand(ChatHandler* handler, Optional<uint32> mapIdArg)
    {
        uint32 mapId = 0;
        if (mapIdArg.has_value())
        {
            mapId = mapIdArg.value();
        }
        else
        {
            Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
            if (!player)
            {
                handler->SendSysMessage("Usage: .dungeonquest list <mapId>");
                return true;
            }
            mapId = player->GetMapId();
        }

        if (!sDungeonQuestMgr->IsDungeonMap(mapId))
        {
            handler->PSendSysMessage("Map {} is not a tracked dungeon map.", mapId);
            return true;
        }

        std::vector<uint32> const& autoQuests = sDungeonQuestMgr->GetDiscoveredQuests(mapId);
        std::vector<uint32> const& finalQuests = sDungeonQuestMgr->GetDungeonQuests(mapId);

        handler->PSendSysMessage("Dungeon quest mapping for map {} ({} auto-discovered, {} final):",
            mapId, autoQuests.size(), finalQuests.size());

        for (uint32 questId : autoQuests)
        {
            std::string const tag = sDungeonQuestMgr->IsForceExcluded(mapId, questId)
                ? "EXCLUDED OVERRIDE" : "AUTO";
            handler->PSendSysMessage("  {} ({}): {}", questId, sDungeonQuestMgr->GetQuestTitle(questId), tag);
        }

        for (uint32 questId : finalQuests)
        {
            if (sDungeonQuestMgr->IsForceIncluded(mapId, questId))
                handler->PSendSysMessage("  {} ({}): INCLUDE OVERRIDE",
                    questId, sDungeonQuestMgr->GetQuestTitle(questId));
        }

        return true;
    }
};
}

void AddSC_DungeonQuests()
{
    new dungeon_quests_worldscript();
    new dungeon_quests_commandscript();
    AddSC_npc_dungeon_quest_guide();
}
