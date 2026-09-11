#ifndef MOD_DUNGEON_QUESTS_DUNGEON_QUEST_MGR_H
#define MOD_DUNGEON_QUESTS_DUNGEON_QUEST_MGR_H

#include "Define.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct DungeonQuestStats
{
    uint32 DungeonCount = 0;         // number of dungeon maps examined
    uint32 DiscoveredMappings = 0;   // auto-discovered (map, quest) pairs
    uint32 ForceIncluded = 0;        // valid force-include overrides applied
    uint32 ForceExcluded = 0;        // valid force-exclude overrides applied
    uint32 InvalidOverrides = 0;     // invalid override rows logged and skipped
};

class DungeonQuestMgr
{
public:
    static DungeonQuestMgr* instance();

    void LoadConfig();

    // Full pipeline: dungeon detection, spawn/loot/quest extraction, override
    // load, membership computation. Runs once at startup (or an explicit reload).
    void Load();
    void Reload();

    bool IsEnabled() const { return _enabled; }

    // True when the map is a tracked 5-player dungeon, or has force-include
    // overrides (admins may attach quests to any existing map id).
    bool IsDungeonMap(uint32 mapId) const;

    // Final quest list for a map: auto-discovered quests, minus force-exclude
    // overrides, plus force-include overrides.
    std::vector<uint32> const& GetDungeonQuests(uint32 mapId) const;

    // Auto-discovered quests for a map, before manual overrides are applied.
    std::vector<uint32> const& GetDiscoveredQuests(uint32 mapId) const;

    std::string const& GetQuestTitle(uint32 questId) const;

    bool IsForceIncluded(uint32 mapId, uint32 questId) const;
    bool IsForceExcluded(uint32 mapId, uint32 questId) const;

    DungeonQuestStats const& GetStats() const { return _stats; }

private:
    DungeonQuestMgr() = default;

    void Clear();

    void DiscoverDungeons();
    void LoadSpawns();
    void LoadQuestLoot();
    void LoadQuestObjectives();
    void LoadQuestRelations();
    void LoadOverrides();
    void ComputeMemberships();
    void ApplyOverrides();

    bool _enabled = false;
    bool _debug = false;

    // Maps classified as 5-player dungeons (present in instance_template AND
    // classified by Map.dbc as a non-raid dungeon).
    std::unordered_set<uint32> _dungeonMaps;

    // Spawn-derived entry sets, keyed by map id, built only for dungeon maps.
    std::unordered_map<uint32, std::unordered_set<uint32>> _mapCreatures;
    std::unordered_map<uint32, std::unordered_set<uint32>> _mapGameObjects;

    // Quest-gated loot (QuestRequired=1), keyed by dropping source entry.
    std::unordered_map<uint32, std::unordered_set<uint32>> _creatureQuestLoot;
    std::unordered_map<uint32, std::unordered_set<uint32>> _gameObjectQuestLoot;

    // Required item ids that are quest-gated drops from anything on the map.
    std::unordered_map<uint32, std::unordered_set<uint32>> _mapQuestLootItems;

    struct QuestObjective
    {
        uint32 Id = 0;
        std::vector<int32> NpcOrGo; // signed: positive = creature, negative = gameobject
        std::vector<uint32> Items;  // RequiredItemId values
    };

    std::unordered_map<uint32, QuestObjective> _quests;
    std::unordered_map<uint32, std::string> _questTitles;

    // Inverted creature_queststarter / creature_questender: quest -> creature entries.
    std::unordered_map<uint32, std::unordered_set<uint32>> _startersByQuest;
    std::unordered_map<uint32, std::unordered_set<uint32>> _endersByQuest;

    // Per-map auto-discovered quest ids.
    std::unordered_map<uint32, std::unordered_set<uint32>> _autoDiscovered;

    // Manual overrides, validated against Map.dbc and quest_template.
    std::unordered_map<uint32, std::unordered_set<uint32>> _forceInclude;
    std::unordered_map<uint32, std::unordered_set<uint32>> _forceExclude;

    // Final per-map quest lists (auto, minus excludes, plus includes), sorted.
    std::unordered_map<uint32, std::vector<uint32>> _finalAutoQuests;
    std::unordered_map<uint32, std::vector<uint32>> _finalQuests;

    DungeonQuestStats _stats;
};

#define sDungeonQuestMgr DungeonQuestMgr::instance()

#endif // MOD_DUNGEON_QUESTS_DUNGEON_QUEST_MGR_H