# mod-dungeon-quests

A server-side AzerothCore WotLK module that helps players find and accept the quests available for
the dungeon they are in. The **Dungeon Quest Guide <Expedition Coordinator>** uses stock Hemet
Nesingwary artwork and a dynamic gossip menu. No client patch or AzerothCore core changes are needed.

## How it works

At startup, the module discovers dungeon membership from world data: creature/gameobject objectives,
quest-required loot, and quest starter/ender relationships. It intersects `instance_template` with
non-raid instance maps and caches the final map-to-quest lists. Gossip never runs discovery SQL.

When a player talks to a guide, the script uses the player's current map to read that cache. It then
checks quest status and reward history, followed by the core's `CanTakeQuest` and `CanAddQuest` APIs.
Normal prerequisite, class/race/faction, level, reputation, exclusive-group, breadcrumb, disabled-quest,
quest-log and starting-item requirements apply. Discovery establishes dungeon membership; eligibility
establishes whether this player can accept a quest now.

- Individual quests show their real titles with the supported stock yellow gossip marker,
  **`GOSSIP_ICON_DOT` (10)**. It is a yellow point, not a fabricated exclamation mark.
- Selecting a quest rechecks current-map membership and eligibility, then uses
  `AddQuestAndCheckCompletion`, including normal starting-item handling.
- **Accept All Available Quests** checks every currently eligible quest again before adding it,
  respects capacity and reports how many were accepted.
- **Accept All**, **Goodbye**, **Previous page** and **Next page** retain ordinary chat icons.
- Menus show up to 24 quests per page. Active quests disappear; abandoning a quest makes it available
  again if the core permits it. Previously rewarded quests, including repeatables, remain hidden.

This is a convenience gossip NPC, not a native questgiver. There are no static queststarter relations,
no automatic granting on dungeon entry, and no universal turn-ins. Complete and turn in quests through
the original game NPCs.

## Guide placement and appearance

Entry **14999991** uses script **npc_dungeon_quest_guide**, neutral faction 35 and gossip-only NPC flags.
Its stock display **24969** comes from Northrend Hemet Nesingwary, template **27986**; the original
Hemet records are not modified. The guide's subtitle is **Expedition Coordinator**.

The audited fresh-install set contains **66 placements on 51 maps**. Separate arrivals on Uldaman,
Gnomeregan, Scarlet Monastery, Stratholme, Maraudon, Dire Maul and Gundrak retain multiple guides.
Normal and heroic variants use the recorded spawn masks rather than duplicate rows.

See [DUNGEON_GUIDE_SPAWNS.md](DUNGEON_GUIDE_SPAWNS.md) for every coordinate, source and per-map count.
The discovered set contains 52 maps, but **map 44 — <unused> Monastery** has no trustworthy arrival
and intentionally has no guide. Placement rows use audited arrival positions; inspect them against
your server's geometry and teleport data before making small manual adjustments.

## Requirements

- An AzerothCore WotLK/Playerbot source build with module support.
- The world database schema using **`creature.id`** and `creature_template_model`.
- An unused custom entry 14999991 for a fresh installation, or the recognized existing guide identity
  identified by the exact name Dungeon Quest Guide and script npc_dungeon_quest_guide when upgrading.
- A MySQL client supporting `DELIMITER`, and database import privileges for CREATE/ALTER ROUTINE
  and EXECUTE in addition to the normal table permissions. The reconciliation helper is removed after success.

The reference build and database checks are recorded in [RELEASE_REPORT.md](RELEASE_REPORT.md).
Additional modules and different core/database revisions may need their own compatibility checks.

## Fresh installation

1. Place the module at `azerothcore-wotlk/modules/mod-dungeon-quests` and build/install worldserver
   using your normal AzerothCore build procedure.
2. Copy `conf/mod_dungeon_quests.conf.dist` into your server's module configuration directory as
   `mod_dungeon_quests.conf`, if your install process has not already created it. Preserve an existing
   configuration rather than replacing it.
3. Enable the normal world database updater (`Updates.EnableDatabases` must include world, bit 4),
   ensure the configured source directory points to this checkout, and start worldserver. The module
   must be present in the compiled module list so the updater can discover its SQL.
4. Check database-update and module startup logs, then inspect the guide and spawn manifest in game.

The base SQL is the complete current state: the optional override table, final guide template/model,
and all 66 audited spawns. Fresh users do not replay development history. The release migration is
self-contained and performs the same reconciliation as the base; either guide file repairs partial state.

### Manual database import

If your server disables automatic database updates, stop worldserver and use your normal database
client to execute these **complete files**, in this order, against the **world** database:

1. `data/sql/db-world/base/mod_dungeon_quest_guide.sql`
2. `data/sql/db-world/base/mod_dungeon_quest_override.sql`
3. `data/sql/db-world/updates/mod_dungeon_quests_2026_09_11_01_release.sql`

For example, `mysql -u YOUR_DB_USER -p YOUR_WORLD_DATABASE < FILE.sql` executes a file and prompts
for the password. Substitute the actual path for `FILE.sql`; no SQL statements need to be copied
from this README. Run imports serially and inspect their result messages before restarting.

## Upgrading an existing deployment

Update the module files, rebuild/install worldserver, preserve your real configuration and restart
with the world updater enabled. The new uniquely named migration runs even if the older base files
were already recorded and redundancy checks are disabled. With automatic updates disabled, execute
the migration file listed above through your normal database client while worldserver is stopped.

Both guide SQL files converge on the canonical v1.0.0 state every time they execute. They:

- Abort with SQLSTATE `45000` before guide changes if entry 14999991 exists with a different name
  or ScriptName. Missing templates are created; missing or incorrect model rows are repaired.
- UPSERT all explicitly module-owned template fields, including the permanent subtitle.
- Replace only model rows for CreatureID 14999991 with index 0, display 24969, scale/probability 1.
- Delete only creature rows with **id = 14999991**, then insert the canonical **66 placements on 51 maps**.
  This intentionally resets manual spawn edits, restores deleted guides and removes extra guide rows.
  Spawn GUIDs are newly allocated on every run; positions, counts and other canonical fields converge.
- Leave administrator override rows untouched. Their separate SQL uses `CREATE TABLE IF NOT EXISTS`.

There is no whole-entry skip flag and correctness does not depend on updater history. Run imports
serially while worldserver is stopped. Guide data changes are transactional on the required InnoDB
tables; SQL errors roll back the reconciliation. A module-named temporary stored procedure supplies
collision/error handling and is dropped after success; a failed import may leave the helper routine,
which the next execution replaces safely. Do not use a client option that ignores SQL errors.

To repair an installation, execute either complete guide SQL file directly, even if the updater already
recorded it. With automatic updates disabled, also execute the override-table file if it is missing.

### SQL layout and updater behavior

```
data/sql/db-world/
  base/
    mod_dungeon_quest_guide.sql
    mod_dungeon_quest_override.sql
  updates/
    mod_dungeon_quests_2026_09_11_01_release.sql
```

`base/` holds current installation state; `updates/` contains a self-contained copy of the guide
reconciliation so existing deployments receive the same complete normalization. Keep the canonical
SQL body identical in both files when maintaining this release. AzerothCore's
module updater recursively scans both directories, tracks filenames/hashes in the core `updates`
table and orders files by **filename**, not directory. These filenames are module-specific and sort
base-before-migration. Do not change the filenames or edit the core's updater tables to force an upgrade.

## Configuration

```
DungeonQuests.Enable = 1
DungeonQuests.Debug = 0
DungeonQuests.EnableAcceptAll = 1
```

- **Enable** controls discovery and guide acceptance.
- **Debug** enables detailed discovery logging; normally leave it off.
- **EnableAcceptAll** shows/enables the batch action. Individual acceptance remains available when off.

Configuration is read on startup/reload. Restart or reload configuration using the normal server
workflow after changing settings or override data. No extra release-specific settings were added.

## Administrator commands

- `.dungeonquest stats` reports discovered maps, quest mappings and override counts.
- `.dungeonquest list [mapId]` lists cached mappings and override annotations; it uses the current map
  if omitted by an in-game administrator. Example: `.dungeonquest list 36`.

Both require administrator access and work from the console. IDs appear in these diagnostic commands,
not in the player's quest menu.

## Manual overrides

`mod_dungeon_quest_override` is a module-owned table keyed by `(map_id, quest_id)`:

| Column | Meaning |
|---|---|
| map_id | Map to adjust |
| quest_id | Existing quest ID |
| action | 1 = force include, 0 = force exclude |

Use it only for membership corrections; including a quest never bypasses player eligibility. The
primary key prevents duplicate override records, and installation/migration preserves existing rows.
Unknown maps/quests and invalid actions are logged and skipped. Overrides can also attach quests to
other existing maps; the audited guide deployment itself remains limited to the listed non-raid maps.

There is no title blacklist. A name containing “Test” does not make a quest invalid: **The Gordok
Taste Test** is a legitimate example. Core eligibility/disable data and explicit overrides handle
exceptional cases.

## Limitations and verification

- Discovery is evidence-based and can have false positives/negatives, especially with shared items
  and dynamically spawned objectives. It does not create prerequisite chains or replace original NPC scripts.
- Previously rewarded repeatables remain hidden under the existing guide policy.
- Starting-item inventory checks can produce normal core equip-error messages.
- There are no guides for raids, and unused map 44 remains unresolved.
- Check different entrances, normal/heroic visibility and local teleport differences against the manifest.
- The operator reports the discovery, placement and acceptance systems working on Eitrigg, including
  eight simultaneous Dire Maul quests with starting items. This release's icon rendering and migration
  still need a final live smoke test; local validation is documented separately.

## License

MIT; see [LICENSE](LICENSE). Discovery relationships were informed by the public
[and-elf/mod-dungeon-questgivers](https://github.com/and-elf/mod-dungeon-questgivers) project and
independently implemented in this module.
