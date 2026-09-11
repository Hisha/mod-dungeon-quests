#include "DungeonQuestMgr.h"

#include "Config.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Log.h"
#include "QueryResult.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
// True when the two sets share at least one member. Iterates the smaller set,
// which keeps the quest-vs-dungeon membership checks cheap.
bool SetsOverlap(std::unordered_set<uint32> const& left, std::unordered_set<uint32> const& right)
{
    if (left.empty() || right.empty())
        return false;

    auto const& small = left.size() <= right.size() ? left : right;
    auto const& big = left.size() <= right.size() ? right : left;

    for (uint32 value : small)
        if (big.count(value) != 0)
            return true;

    return false;
}

uint32 CountOverrides(std::unordered_map<uint32, std::unordered_set<uint32>> const& overrides)
{
    uint32 total = 0;
    for (auto const& [mapId, quests] : overrides)
    {
        (void)mapId;
        total += static_cast<uint32>(quests.size());
    }
    return total;
}
}

DungeonQuestMgr* DungeonQuestMgr::instance()
{
    static DungeonQuestMgr instance;
    return &instance;
}

void DungeonQuestMgr::LoadConfig()
{
    _enabled = sConfigMgr->GetOption<bool>("DungeonQuests.Enable", true);
    _debug = sConfigMgr->GetOption<bool>("DungeonQuests.Debug", false);
}

void DungeonQuestMgr::Clear()
{
    _dungeonMaps.clear();
    _mapCreatures.clear();
    _mapGameObjects.clear();
    _creatureQuestLoot.clear();
    _gameObjectQuestLoot.clear();
    _mapQuestLootItems.clear();
    _quests.clear();
    _questTitles.clear();
    _startersByQuest.clear();
    _endersByQuest.clear();
    _autoDiscovered.clear();
    _forceInclude.clear();
    _forceExclude.clear();
    _finalAutoQuests.clear();
    _finalQuests.clear();
    _stats = DungeonQuestStats{};
}

void DungeonQuestMgr::Load()
{
    Clear();

    if (!_enabled)
    {
        LOG_INFO("module", "[DungeonQuests] Disabled; automatic dungeon quest discovery skipped.");
        return;
    }

    DiscoverDungeons();
    if (_dungeonMaps.empty())
    {
        LOG_ERROR("module", "[DungeonQuests] No dungeon maps could be identified; discovery aborted.");
        return;
    }

    LoadSpawns();
    LoadQuestLoot();
    LoadQuestObjectives();
    LoadQuestRelations();
    LoadOverrides();
    ComputeMemberships();
    ApplyOverrides();

    LOG_INFO("module", "[DungeonQuests] Discovery complete: {} dungeon map(s), {} auto-discovered quest mapping(s), "
        "{} force-include(s), {} force-exclude(s), {} invalid override(s).",
        _stats.DungeonCount, _stats.DiscoveredMappings, _stats.ForceIncluded,
        _stats.ForceExcluded, _stats.InvalidOverrides);
}

void DungeonQuestMgr::Reload()
{
    LOG_INFO("module", "[DungeonQuests] Reloading dungeon quest discovery.");
    Load();
}

// ---------------------------------------------------------------------------
// Dungeon detection
// ---------------------------------------------------------------------------

void DungeonQuestMgr::DiscoverDungeons()
{
    // instance_template contains every instance map the world DB knows about.
    // The DBC check below then narrows it to non-raid dungeons, keeping raids,
    // battlegrounds, arenas and outdoor continent maps out of scope.
    QueryResult result = WorldDatabase.Query("SELECT DISTINCT `map` FROM `instance_template`");
    if (!result)
    {
        LOG_ERROR("module", "[DungeonQuests] Could not read instance_template; dungeon detection failed.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        uint32 mapId = fields[0].Get<uint32>();

        auto mapEntry = sMapStore.LookupEntry(mapId);
        if (!mapEntry || !mapEntry->IsNonRaidDungeon())
            continue;

        _dungeonMaps.insert(mapId);
    } while (result->NextRow());

    _stats.DungeonCount = static_cast<uint32>(_dungeonMaps.size());
    LOG_INFO("module", "[DungeonQuests] Identified {} dungeon map(s) from instance_template and Map.dbc.",
        _stats.DungeonCount);
}

// ---------------------------------------------------------------------------
// Spawn data
// ---------------------------------------------------------------------------

void DungeonQuestMgr::LoadSpawns()
{
    // AzerothCore's creature table stores up to five alternate spawn entries
    // per row (id1..id5). Union them all so spawns referencing an aliased entry
    // still count toward the map's creature set.
    QueryResult creatures = WorldDatabase.Query(
        "SELECT DISTINCT `map`, `id1` FROM `creature` WHERE `id1` != 0 "
        "UNION SELECT DISTINCT `map`, `id2` FROM `creature` WHERE `id2` != 0 "
        "UNION SELECT DISTINCT `map`, `id3` FROM `creature` WHERE `id3` != 0 "
        "UNION SELECT DISTINCT `map`, `id4` FROM `creature` WHERE `id4` != 0 "
        "UNION SELECT DISTINCT `map`, `id5` FROM `creature` WHERE `id5` != 0");

    if (creatures)
    {
        do
        {
            Field* fields = creatures->Fetch();
            uint32 mapId = fields[0].Get<uint32>();
            uint32 entry = fields[1].Get<uint32>();
            if (!_dungeonMaps.count(mapId))
                continue;
            _mapCreatures[mapId].insert(entry);
        } while (creatures->NextRow());
    }

    QueryResult gameObjects = WorldDatabase.Query("SELECT DISTINCT `map`, `id` FROM `gameobject`");
    if (gameObjects)
    {
        do
        {
            Field* fields = gameObjects->Fetch();
            uint32 mapId = fields[0].Get<uint32>();
            uint32 entry = fields[1].Get<uint32>();
            if (!_dungeonMaps.count(mapId))
                continue;
            _mapGameObjects[mapId].insert(entry);
        } while (gameObjects->NextRow());
    }

    uint32 creatureEntries = 0;
    for (auto const& [mapId, entries] : _mapCreatures)
    {
        (void)mapId;
        creatureEntries += static_cast<uint32>(entries.size());
    }
    uint32 goEntries = 0;
    for (auto const& [mapId, entries] : _mapGameObjects)
    {
        (void)mapId;
        goEntries += static_cast<uint32>(entries.size());
    }

    LOG_INFO("module", "[DungeonQuests] Loaded {} creature and {} gameobject spawn entry(ies) across dungeon maps.",
        creatureEntries, goEntries);
}

// ---------------------------------------------------------------------------
// Quest-gated loot
// ---------------------------------------------------------------------------

void DungeonQuestMgr::LoadQuestLoot()
{
    // Only QuestRequired = 1 ("quest item" style) drops are considered. Generic
    // trash loot (cloth, herbs, ore) must not pull unrelated collection quests
    // into every dungeon. This mirrors the filtering used by the reference
    // mod-dungeon-questgivers project.
    QueryResult creatureLoot = WorldDatabase.Query(
        "SELECT `Entry`, `Item` FROM `creature_loot_template` WHERE `QuestRequired` = 1");
    if (creatureLoot)
    {
        do
        {
            Field* fields = creatureLoot->Fetch();
            uint32 entry = fields[0].Get<uint32>();
            uint32 item = fields[1].Get<uint32>();
            if (!entry || !item)
                continue;
            _creatureQuestLoot[entry].insert(item);
        } while (creatureLoot->NextRow());
    }

    QueryResult goLoot = WorldDatabase.Query(
        "SELECT `Entry`, `Item` FROM `gameobject_loot_template` WHERE `QuestRequired` = 1");
    if (goLoot)
    {
        do
        {
            Field* fields = goLoot->Fetch();
            uint32 entry = fields[0].Get<uint32>();
            uint32 item = fields[1].Get<uint32>();
            if (!entry || !item)
                continue;
            _gameObjectQuestLoot[entry].insert(item);
        } while (goLoot->NextRow());
    }

    // Resolve, per dungeon map, every item obtainable as quest-gated loot from
    // a creature or gameobject spawned on that map.
    uint32 mapLootAssociations = 0;
    for (uint32 mapId : _dungeonMaps)
    {
        std::unordered_set<uint32>& mapItems = _mapQuestLootItems[mapId];

        auto const creatureIt = _mapCreatures.find(mapId);
        if (creatureIt != _mapCreatures.end())
        {
            for (uint32 entry : creatureIt->second)
            {
                auto const lootIt = _creatureQuestLoot.find(entry);
                if (lootIt != _creatureQuestLoot.end())
                    mapItems.insert(lootIt->second.begin(), lootIt->second.end());
            }
        }

        auto const goIt = _mapGameObjects.find(mapId);
        if (goIt != _mapGameObjects.end())
        {
            for (uint32 entry : goIt->second)
            {
                auto const lootIt = _gameObjectQuestLoot.find(entry);
                if (lootIt != _gameObjectQuestLoot.end())
                    mapItems.insert(lootIt->second.begin(), lootIt->second.end());
            }
        }

        mapLootAssociations += static_cast<uint32>(mapItems.size());
    }

    LOG_INFO("module", "[DungeonQuests] Loaded quest-gated loot for {} creature and {} gameobject source(s); "
        "{} dungeon map -> item association(s) derived.",
        _creatureQuestLoot.size(), _gameObjectQuestLoot.size(), mapLootAssociations);
}

// ---------------------------------------------------------------------------
// Quest definitions
// ---------------------------------------------------------------------------

void DungeonQuestMgr::LoadQuestObjectives()
{
    QueryResult result = WorldDatabase.Query(
        "SELECT `ID`, `LogTitle`, "
        "`RequiredNpcOrGo1`, `RequiredNpcOrGo2`, `RequiredNpcOrGo3`, `RequiredNpcOrGo4`, "
        "`RequiredItemId1`, `RequiredItemId2`, `RequiredItemId3`, `RequiredItemId4`, "
        "`RequiredItemId5`, `RequiredItemId6` "
        "FROM `quest_template`");

    if (!result)
    {
        LOG_ERROR("module", "[DungeonQuests] Could not read quest_template; discovery aborted.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        uint32 questId = fields[0].Get<uint32>();
        if (!questId)
            continue;

        QuestObjective objective;
        objective.Id = questId;
        _questTitles[questId] = fields[1].Get<std::string>();

        // RequiredNpcOrGo is signed: positive = creature entry, negative = gameobject entry.
        for (int32 i = 0; i < 4; ++i)
        {
            int32 entry = fields[2 + i].Get<int32>();
            if (entry != 0)
                objective.NpcOrGo.push_back(entry);
        }

        for (int32 i = 0; i < 6; ++i)
        {
            uint32 item = fields[6 + i].Get<uint32>();
            if (item != 0)
                objective.Items.push_back(item);
        }

        _quests.emplace(questId, std::move(objective));
    } while (result->NextRow());

    LOG_INFO("module", "[DungeonQuests] Loaded {} quest objective definition(s).", _quests.size());
}

void DungeonQuestMgr::LoadQuestRelations()
{
    QueryResult starters = WorldDatabase.Query("SELECT `id`, `quest` FROM `creature_queststarter`");
    if (starters)
    {
        do
        {
            Field* fields = starters->Fetch();
            uint32 creatureEntry = fields[0].Get<uint32>();
            uint32 questId = fields[1].Get<uint32>();
            if (creatureEntry && questId)
                _startersByQuest[questId].insert(creatureEntry);
        } while (starters->NextRow());
    }

    QueryResult enders = WorldDatabase.Query("SELECT `id`, `quest` FROM `creature_questender`");
    if (enders)
    {
        do
        {
            Field* fields = enders->Fetch();
            uint32 creatureEntry = fields[0].Get<uint32>();
            uint32 questId = fields[1].Get<uint32>();
            if (creatureEntry && questId)
                _endersByQuest[questId].insert(creatureEntry);
        } while (enders->NextRow());
    }

    LOG_INFO("module", "[DungeonQuests] Loaded {} quest starter and {} quest ender relation map(s).",
        _startersByQuest.size(), _endersByQuest.size());
}

// ---------------------------------------------------------------------------
// Manual overrides
// ---------------------------------------------------------------------------

void DungeonQuestMgr::LoadOverrides()
{
    // The override table is optional. Discovery works without it; the table only
    // tunes the derived results. Bad rows are logged and skipped, never fatal.
    QueryResult result = WorldDatabase.Query(
        "SELECT `map_id`, `quest_id`, `action` FROM `mod_dungeon_quest_override`");
    if (!result)
    {
        LOG_INFO("module", "[DungeonQuests] mod_dungeon_quest_override is empty or not installed; no manual overrides loaded.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        uint32 mapId = fields[0].Get<uint32>();
        uint32 questId = fields[1].Get<uint32>();
        int32 action = fields[2].Get<int32>();

        auto const mapEntry = sMapStore.LookupEntry(mapId);
        if (!mapEntry)
        {
            LOG_WARN("module", "[DungeonQuests] Ignoring override: map {} does not exist.", mapId);
            ++_stats.InvalidOverrides;
            continue;
        }

        if (!_quests.count(questId))
        {
            LOG_WARN("module", "[DungeonQuests] Ignoring override for map {}: quest {} does not exist.",
                mapId, questId);
            ++_stats.InvalidOverrides;
            continue;
        }

        if (action == 1)
            _forceInclude[mapId].insert(questId);
        else if (action == 0)
            _forceExclude[mapId].insert(questId);
        else
        {
            LOG_WARN("module", "[DungeonQuests] Ignoring override for map {} quest {}: action {} is not 0 or 1.",
                mapId, questId, action);
            ++_stats.InvalidOverrides;
        }
    } while (result->NextRow());

    LOG_INFO("module", "[DungeonQuests] Loaded {} force-include(s) and {} force-exclude(s); {} invalid row(s) skipped.",
        CountOverrides(_forceInclude), CountOverrides(_forceExclude), _stats.InvalidOverrides);
}

// ---------------------------------------------------------------------------
// Membership
// ---------------------------------------------------------------------------

void DungeonQuestMgr::ComputeMemberships()
{
    uint32 mappings = 0;

    std::unordered_set<uint32> const kEmptySet;

    for (uint32 mapId : _dungeonMaps)
    {
        auto const creatureIt = _mapCreatures.find(mapId);
        auto const goIt = _mapGameObjects.find(mapId);
        auto const lootIt = _mapQuestLootItems.find(mapId);

        std::unordered_set<uint32> const& creatures = creatureIt != _mapCreatures.end() ? creatureIt->second : kEmptySet;
        std::unordered_set<uint32> const& gameObjects = goIt != _mapGameObjects.end() ? goIt->second : kEmptySet;
        std::unordered_set<uint32> const& questLootItems = lootIt != _mapQuestLootItems.end() ? lootIt->second : kEmptySet;

        std::unordered_set<uint32>& hits = _autoDiscovered[mapId];

        for (auto const& [questId, objective] : _quests)
        {
            // Signal 1: the quest is started from a creature spawned on this map.
            bool belongs = false;
            auto const starterIt = _startersByQuest.find(questId);
            if (starterIt != _startersByQuest.end() && SetsOverlap(starterIt->second, creatures))
                belongs = true;

            // Signal 2: the quest ends at a creature spawned on this map.
            if (!belongs)
            {
                auto const enderIt = _endersByQuest.find(questId);
                if (enderIt != _endersByQuest.end() && SetsOverlap(enderIt->second, creatures))
                    belongs = true;
            }

            // Signal 3: a RequiredNpcOrGo objective targets a creature or
            // gameobject spawned on this map.
            if (!belongs)
            {
                for (int32 requiredEntry : objective.NpcOrGo)
                {
                    if (requiredEntry > 0)
                    {
                        if (creatures.count(static_cast<uint32>(requiredEntry)))
                        {
                            belongs = true;
                            break;
                        }
                    }
                    else if (gameObjects.count(static_cast<uint32>(-requiredEntry)))
                    {
                        belongs = true;
                        break;
                    }
                }
            }

            // Signal 4: a required item is quest-gated loot (QuestRequired = 1)
            // dropped by something spawned on this map.
            if (!belongs)
            {
                for (uint32 itemId : objective.Items)
                {
                    if (questLootItems.count(itemId))
                    {
                        belongs = true;
                        break;
                    }
                }
            }

            if (belongs)
            {
                hits.insert(questId);
                ++mappings;
                if (_debug)
                    LOG_DEBUG("module", "[DungeonQuests] Auto-discovered: map {} quest {} ('{}').",
                        mapId, questId, GetQuestTitle(questId));
            }
        }
    }

    _stats.DiscoveredMappings = mappings;
    LOG_INFO("module", "[DungeonQuests] Discovered {} quest mapping(s) across {} dungeon map(s).",
        mappings, _stats.DungeonCount);
}

void DungeonQuestMgr::ApplyOverrides()
{
    _finalAutoQuests.clear();
    _finalQuests.clear();

    for (auto const& [mapId, questSet] : _autoDiscovered)
    {
        std::vector<uint32>& autoList = _finalAutoQuests[mapId];
        autoList.assign(questSet.begin(), questSet.end());
        std::sort(autoList.begin(), autoList.end());

        std::vector<uint32> merged = autoList;

        auto const excludeIt = _forceExclude.find(mapId);
        if (excludeIt != _forceExclude.end())
        {
            auto const newEnd = std::remove_if(merged.begin(), merged.end(),
                [&excludeIt](uint32 questId) { return excludeIt->second.count(questId) != 0; });
            merged.erase(newEnd, merged.end());
        }

        _finalQuests[mapId] = std::move(merged);
    }

    // Force-includes may target maps that produced no auto-discovered quests.
    for (auto const& [mapId, questSet] : _forceInclude)
    {
        std::vector<uint32>& finalList = _finalQuests[mapId];
        finalList.insert(finalList.end(), questSet.begin(), questSet.end());
        std::sort(finalList.begin(), finalList.end());
        auto const newEnd = std::unique(finalList.begin(), finalList.end());
        finalList.erase(newEnd, finalList.end());
    }

    _stats.ForceIncluded = CountOverrides(_forceInclude);
    _stats.ForceExcluded = CountOverrides(_forceExclude);
}

// ---------------------------------------------------------------------------
// Lookups
// ---------------------------------------------------------------------------

bool DungeonQuestMgr::IsDungeonMap(uint32 mapId) const
{
    return _dungeonMaps.count(mapId) != 0 || _forceInclude.count(mapId) != 0;
}

std::vector<uint32> const& DungeonQuestMgr::GetDungeonQuests(uint32 mapId) const
{
    static std::vector<uint32> const kEmpty;
    auto const it = _finalQuests.find(mapId);
    return it != _finalQuests.end() ? it->second : kEmpty;
}

std::vector<uint32> const& DungeonQuestMgr::GetDiscoveredQuests(uint32 mapId) const
{
    static std::vector<uint32> const kEmpty;
    auto const it = _finalAutoQuests.find(mapId);
    return it != _finalAutoQuests.end() ? it->second : kEmpty;
}

std::string const& DungeonQuestMgr::GetQuestTitle(uint32 questId) const
{
    static std::string const kEmpty;
    auto const it = _questTitles.find(questId);
    return it != _questTitles.end() ? it->second : kEmpty;
}

bool DungeonQuestMgr::IsForceIncluded(uint32 mapId, uint32 questId) const
{
    auto const it = _forceInclude.find(mapId);
    return it != _forceInclude.end() && it->second.count(questId) != 0;
}

bool DungeonQuestMgr::IsForceExcluded(uint32 mapId, uint32 questId) const
{
    auto const it = _forceExclude.find(mapId);
    return it != _forceExclude.end() && it->second.count(questId) != 0;
}
