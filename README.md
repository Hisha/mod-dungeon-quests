# mod-dungeon-quests

An AzerothCore (WotLK 3.3.5a) module that places a reusable **Dungeon Quest Guide** NPC at
dungeon entrances. The guide automatically determines which quests belong to the dungeon the
player is standing in and offers only the quests that player is legitimately eligible to accept —
without requiring administrators to hand-maintain a `map_id -> quest_id` table.

> **Status: Pass 2 implementation.** Reusable guide, player eligibility, individual acceptance,
> optional Accept All, and one Deadmines test spawn. See `PASS2_REPORT.md` for actual validation
> results and remaining in-game checks.

## Purpose

Dungeon quests in WotLK are scattered across the world: starters in cities, objectives inside an
instance, turn-ins back outside. This pass places one neutral guide NPC inside Deadmines whose
gossip lists that dungeon's quests, so a group can pick them up on arrival.

Unlike classic quest-giver relations (`creature_queststarter`), the module derives which quests
belong to which dungeon automatically from the world database at startup, and it filters every quest
through AzerothCore's normal eligibility rules at gossip time. It never bypasses quest prerequisites,
levels, faction, race, class, completion state, or quest-log capacity.

## Architecture

```
 src/
   DungeonQuestMgr.h            manager API (singleton, cached lookup)
   DungeonQuestMgr.cpp          discovery pipeline + override loading + membership math
   mod_dungeon_quests.cpp       WorldScript (config + startup/reload) + diagnostic commands
   npc_dungeon_quest_guide.cpp  dynamic gossip + normal quest acceptance
   loader.h                     AzerothCore script loader entry point
 conf/mod_dungeon_quests.conf.dist
 data/sql/db-world/base/mod_dungeon_quest_override.sql
 data/sql/db-world/base/mod_dungeon_quest_guide.sql
```

`DungeonQuestMgr` is the only component that touches the world database for discovery. It runs once
at server startup (or an explicit reload) and caches everything in memory:

- dungeon map ids
- creature / gameobject spawn entries per dungeon map
- quest-gated loot per dungeon map
- quest objectives (`RequiredNpcOrGo`, `RequiredItemId`)
- inverted `creature_queststarter` / `creature_questender` relations
- validated manual overrides

Gossip calls `sDungeonQuestMgr->GetDungeonQuests(mapId)` and performs the normal
per-player eligibility checks. No expensive database joins run while a player talks to the NPC.

### Interfaces
- `GetDungeonQuests(mapId)` — final quest list for a dungeon (auto, minus excludes, plus includes).
- `GetDiscoveredQuests(mapId)` — auto-discovered list, before overrides.
- `IsDungeonMap(mapId)` — is this map tracked as a dungeon (or force-include target)?
- `Load()` / `Reload()` — build / rebuild the cached mappings.
- `GetStats()` — discovery statistics for logging and `.dungeonquest stats`.

## Automatic discovery

At load time the manager builds `dungeon map -> quest ids` from authoritative world-database
relationships. A quest belongs to a dungeon when **any** of the following signals match against the
creatures/gameobjects spawned on that dungeon's map:

1. **Creature objective** (`RequiredNpcOrGo`) targets a creature entry spawned on the map.
2. **GameObject objective** (`RequiredNpcOrGo` negative) targets a gameobject entry spawned on the map.
3. **Required item loot**: a `RequiredItemId` is a **quest-gated drop** (`QuestRequired = 1` in
   `creature_loot_template` / `gameobject_loot_template`) from a creature/gameobject spawned on the map.
4. **Starter / ender relations**: the quest is started or turned in at a creature
   (`creature_queststarter` / `creature_questender`) spawned on the map.

Discovery is one stage per data source (a handful of `SELECT`s at startup), then merged in memory.
Signals 1/2 correspond to the requested "creature objective" and "gameobject objective" paths.
Signal 3 covers "required item" quests (many dungeon quests require an item dropped by a dungeon
creature without a direct kill objective). Signal 4 is a bonus that also catches dungeon-quest
starters/turn-ins whose givers live inside the instance.

### Dungeon detection

A map is treated as a dungeon when it is both:

- listed in `instance_template` (the world DB's instance map set), **and**
- classified by `Map.dbc` as a **non-raid dungeon** (`MapEntry::IsNonRaidDungeon()`).

This keeps out normal outdoor zones, battlegrounds, arenas, and raids, while covering every normal
and heroic 5-player instance. Raids are intentionally out of scope for now and can be enabled later
by relaxing the DBC check.

### False-positive handling (heuristics)

Automatic discovery is inherently heuristic; the module deliberately errs toward **strong evidence**:

- Loot-based matches require `QuestRequired = 1`. A trash mob dropping generic trade goods
  (cloth, herbs, ore) therefore cannot pull unrelated "collect 50 wool" world quests into every dungeon.
- A quest-gated item that also drops outdoors can still attach a borderline world quest to a
  dungeon (rare and acceptable: the objective is genuinely progressable inside that dungeon).
- A creature entry spawned on more than one map (dungeon + outdoors) counts for each map where it is
  actually spawned; if such a shared mob is also the quest's target, the quest follows the spawns.
- Shared gameobjects and scripted objectives follow the same rule: membership requires a real
  spawn on the map, never a template-only assumption.

See *Limitations* for cases this cannot resolve automatically.

## Manual overrides

A small override table tunes the results without maintaining a full quest table:

```
mod_dungeon_quest_override
  map_id    SMALLINT UNSIGNED
  quest_id  INT UNSIGNED
  action    TINYINT    -- 1 = force include, 0 = force exclude
  PRIMARY KEY (map_id, quest_id)
```

- **force include** adds a quest to a map even if discovery did not find it (for example a quest
  whose objective references dynamically spawned, scripted NPCs).
- **force exclude** removes a quest a map even if discovery found it (for example a shared item
  that produced a false positive).

Rows are validated at load: an unknown quest id, an unknown map id, or an action other than `0`/`1`
is logged with the `[DungeonQuests]` prefix and skipped — it never crashes the server. `force
include` may target any map id that exists in `Map.dbc`, so quests can also be attached to maps
discovery did not classify as dungeons.

## Configuration

`conf/mod_dungeon_quests.conf.dist`:

```
DungeonQuests.Enable = 1
DungeonQuests.Debug  = 0
DungeonQuests.EnableAcceptAll = 1
```

- `Enable` — turns automatic discovery and guide acceptance on/off.
- `EnableAcceptAll` — shows the batch-accept option; cached on config load/reload.
- `Debug` — logs individual quest membership decisions during discovery/reload. Leave off in
  production; it is noisy by design.

## Commands

- `.dungeonquest stats` — prints discovery statistics:
  ```
  Dungeon quest discovery:
  49 dungeon maps
  312 auto-discovered quest mappings
  8 force-includes
  3 force-excludes
  ```
- `.dungeonquest list [mapId]` — lists the quest mappings for the given map (or the current map
  when called from inside an instance). Each quest is tagged `AUTO`, `INCLUDE OVERRIDE`, or
  `EXCLUDED OVERRIDE`.

Both require administrator rights and work from the console.

## NPC behavior

The reusable `npc_dungeon_quest_guide` script:

1. Read the player's current map and fetch `GetDungeonQuests(mapId)`.
2. If no mapping exists: *"No dungeon quests are known for this location."*
3. If all mapped quests fail eligibility: *"You have no available quests for this dungeon."*
4. Otherwise list the eligible quest names plus an **Accept All Available Quests** option.
5. Accept individual quests with AzerothCore's normal add-quest path, revalidating eligibility
   at the moment of acceptance (including quest-log capacity).

Automatic discovery answers only "does this quest belong to this dungeon?"; whether a *player* may
take it is always answered by AzerothCore's standard eligibility APIs. Quest chains are preserved: a
later quest in a chain is offered only when the core considers the player eligible, so prerequisites
are never auto-granted.

## Installing the guide

Apply the changed files to the module, rebuild worldserver, and import
`data/sql/db-world/base/mod_dungeon_quest_guide.sql` into the **world** database while worldserver
is stopped. Preserve your real `.conf` settings and add `DungeonQuests.EnableAcceptAll = 1` there.
The distributed `.conf.dist` does not overwrite an existing configuration.

The SQL targets **creature.id**, the supplied live schema. Entry **14999991** uses WotLK display
**5233** (Spirit Healer appearance), faction 35 and gossip-only NPC flags. The only test spawn is
map **36**, **(-16.4, -383.07, 61.78)**, orientation **1.86**, from the core's Deadmines entrance
teleport destination (areatrigger 78). Verify placement in game before moving beyond this test.
The spawn GUID is allocated by the database. No static queststarter/questender relations are added.

Check entry 14999991 in your live world database before importing. SQL does not overwrite an
occupied template or model; a conflicting identity skips installation and prints a result message.
An already installed guide is retained; the map-36 spawn is not duplicated on rerun. Run imports
serially, with no concurrent world-data writers.

The guide uses `GetQuestStatus` and `IsQuestRewarded`, then `CanTakeQuest(quest, false)` and
`CanAddQuest(quest, false)`. Selection checks current-map membership and repeats eligibility
immediately before `AddQuestAndCheckCompletion(quest, creature)`. Batch acceptance snapshots all
currently eligible quests across pages and revalidates each; it skips cleanly when capacity is lost.
Menus show 24 quests per page to fit the core's gossip limit.

Previously rewarded repeatable quests are intentionally hidden, as required by this pass; the core
itself normally permits some repeatables again. Disabled quests are rejected through `CanTakeQuest`.
A test/development quest that passes normal core eligibility can still appear: there is no title-based
filter or C++ blacklist. Correct such membership using the existing force-exclude override table.
Quest-specific behavior tied to the original questgiver still needs in-game validation; the guide
does not impersonate or spawn the original questgiver or grant prerequisite quests.

## Troubleshooting / debug mode

- Enable `DungeonQuests.Debug = 1` and reload config (or restart) to see, per dungeon map, which
  quests were matched during discovery.
- `.dungeonquest stats` shows how many maps and mappings were discovered.
- `.dungeonquest list 36` (Deadmines) lists the actual detected quest ids so you can sanity-check
  against game data.
- Full startup/reload logs use the `[DungeonQuests]` prefix and report maps examined, mappings
  discovered, overrides loaded, invalid overrides, and any SQL/query errors.

## Limitations of automatic discovery

- **`QuestRequired = 1` loot gate is required for loot-based matching.** A dungeon quest item that a
  stock DB marks without `QuestRequired` will not be matched by the item path (it may still be
  matched by a creature objective or starter/ender relation).
- **Event/scripted dungeons** whose quests reference dynamic (script-spawned) NPCs, or whose
  starters live outside the instance, may produce no matches (for example wilderness-camped quest
  chains). Use a force-include override for these.
- **Shared drops** can attach an occasional borderline world quest to a dungeon.
- **Raid content is not discovered** in this pass.
- **Membership is derived, not gospel.** It is a proxy signal and can produce both false positives
  (use force-exclude) and false negatives (use force-include).

## Next step

Validate the single Deadmines guide in game using `PASS2_REPORT.md`. Additional dungeon spawns
are outside this pass and should wait until gameplay is verified.

## Schema assumptions to verify on the actual server

These queries target the current AzerothCore WotLK (Playerbot) world database and were written after
studying the public `mod-dungeon-questgivers` generator, which reads the same world schema. They must
be re-verified against the live world DB on first deployment:

| Table | Columns used | Notes |
| --- | --- | --- |
| `instance_template` | `map` | instance map id set |
| `creature` | `map`, `id` | Supplied live Playerbot schema; preserved |
| `gameobject` | `map`, `id` | |
| `creature_loot_template` / `gameobject_loot_template` | `Entry`, `Item`, `QuestRequired` | quest-gated drop filter |
| `quest_template` | `ID`, `LogTitle`, `RequiredNpcOrGo1..4`, `RequiredItemId1..6` | signed `RequiredNpcOrGo` |
| `creature_queststarter` / `creature_questender` | `id`, `quest` | |
| `mod_dungeon_quest_override` | `map_id`, `quest_id`, `action` | module-owned table |

## Validation status

See `PASS2_REPORT.md` for the exact core revision, build results, limitations and deployment
checklist. The live server's previously reported 52 maps / 326 mappings is a baseline supplied by
the operator, not a new runtime result from this pass. Discovery source remains unchanged.

## License

Mirrors the MIT license of the repository (`LICENSE`).

Discovery logic (quest-to-dungeon membership via starter/ender relations, `RequiredNpcOrGo`
objectives, and `QuestRequired = 1` loot) was informed by the public
[`and-elf/mod-dungeon-questgivers`](https://github.com/and-elf/mod-dungeon-questgivers) project,
specifically its ARCHITECTURE.md derivation rules. No source code was copied from that project; it
was studied for database relationships and the general algorithm, then independently implemented in
this module's own (C++, runtime) architecture. No license file was present in the reference
repository at the time of writing; if you copy code from it elsewhere, check its current terms.