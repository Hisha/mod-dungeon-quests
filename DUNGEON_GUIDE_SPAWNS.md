# Dungeon Quest Guide spawn manifest

**66 planned guides on 51 maps; 52 maps considered; 44 maps have one guide, seven maps have multiple.**
Map 44 is unresolved and has no generated spawn. Counts describe the canonical state after every successful guide SQL execution.
All existing creature rows with id 14999991 are intentionally replaced with this audited set.

Guide **14999991**, **Dungeon Quest Guide <Expedition Coordinator>**, script
`npc_dungeon_quest_guide`, display **24969** from Northrend **Hemet Nesingwary (27986)**.
No changes to Hemet's original records. No queststarter/questender relationships.

## Source and selection method

Reference Playerbot core commit **06234df3d5ab26c93f4f1f06f3edb828b73ecd3c**.

- [instance_template](https://github.com/mod-playerbots/azerothcore-wotlk/blob/06234df3d5ab26c93f4f1f06f3edb828b73ecd3c/data/sql/base/db_world/instance_template.sql), intersected with type **1** from the
  [AzerothCore WotLK Map.dbc content table](https://github.com/azerothcore/wiki/blob/ba2b61857517b11864ca8a847c87d8a99785fbb2/docs/map.md).
  This exactly implements the manager's selection criterion: `IsNonRaidDungeon()` is `map_type == MAP_INSTANCE`.
  No max-player-count filter was added: historical DBC capacity fields include 10/0 on some supported maps.
- Arrival coordinates from [areatrigger_teleport](https://github.com/mod-playerbots/azerothcore-wotlk/blob/06234df3d5ab26c93f4f1f06f3edb828b73ecd3c/data/sql/base/db_world/areatrigger_teleport.sql), checked against the
  [areatrigger origin maps](https://github.com/mod-playerbots/azerothcore-wotlk/blob/06234df3d5ab26c93f4f1f06f3edb828b73ecd3c/data/sql/base/db_world/areatrigger.sql).
  Selected normal world-map -> dungeon arrivals, rather than blindly using every row targeting a dungeon.
- Applied [2026_07_12_01.sql](https://github.com/mod-playerbots/azerothcore-wotlk/blob/06234df3d5ab26c93f4f1f06f3edb828b73ecd3c/data/sql/updates/db_world/2026_07_12_01.sql) after the base dump: Gundrak north arrival 5206 and normal/heroic LFG overrides 216/217.
- Maraudon alternate starts come from [lfg_dungeon_template](https://github.com/mod-playerbots/azerothcore-wotlk/blob/06234df3d5ab26c93f4f1f06f3edb828b73ecd3c/data/sql/base/db_world/lfg_dungeon_template.sql) row 273, and
  [spell_target_position](https://github.com/mod-playerbots/azerothcore-wotlk/blob/06234df3d5ab26c93f4f1f06f3edb828b73ecd3c/data/sql/base/db_world/spell_target_position.sql) spell 21128 effect 0.
  The latter is the destination of gameobject template 178404 (Portal to Inner Maraudon), with exterior
  gameobject spawn 5417 on map 1. These are distinct from both physical entrances and from each other.
- Spawn masks come from normal/heroic bits observed in the reference `creature` data per selected map:
  **1** normal only; **3** normal and heroic. One database row serves both difficulties.

The map count matches the reported live count of 52, but the inaccessible live server's ID list has
not been compared directly. The reference base spawn schema has id1/id2/id3; later core code uses id
plus creature_multispawn. Delivery SQL consistently uses the user's proven **creature.id** schema.
The live Gundrak north teleport may still use the older base destination
(1894.58, 652.713, 176.666, 4.078), roughly 25 yards from the updated destination below; inspect this
entrance against the live `areatrigger_teleport` row before importing or adjust the module placement.

## Placement policy — every row requires visual validation

**V** means exact authoritative arrival coordinates retained because a safe terrain offset could not
be established without live geometry/in-game inspection. These can overlap the arriving player;
they are a deliberate fallback allowed by this pass, not a claim of collision-free or hostile-free
placement. Do not add an arbitrary offset. In game, move each guide a few yards aside only after
checking the floor, walls, doors, hostile packs, visibility and reachability.

Both the base and upgrade SQL restore exactly this canonical set on every execution. Missing
spawns return; extra module guide spawns and manual placement edits are removed. New GUIDs are
allocated. Creatures with other IDs and administrator override rows remain untouched. Persisting
site-specific placement changes requires maintaining a corresponding custom canonical SQL set;
manual in-game moves alone are reset by the next reconciliation.

## Every generated spawn

Coordinates are in yards; orientation is in radians. Sources are row IDs, not quest IDs.

| Slot | Dungeon | Map | X | Y | Z | O | Coordinate source | Guides on map | Spawn mask | Notes |
|---:|---|---:|---:|---:|---:|---:|---|---:|---:|---|
| 1 | Shadowfang Keep | 33 | -229.135 | 2109.18 | 76.8898 | 1.267 | areatrigger_teleport 145 | 1 | 1 | Shadowfang Keep Entrance; V |
| 2 | Stormwind Stockade | 34 | 54.23 | 0.28 | -18.34 | 6.26 | areatrigger_teleport 101 | 1 | 1 | Stormwind Stockades Entrance; V |
| 3 | Deadmines | 36 | -16.4 | -383.07 | 61.78 | 1.86 | areatrigger_teleport 78 | 1 | 1 | DeadMines Entrance; V; existing live-tested spawn, reused |
| 4 | Wailing Caverns | 43 | -163.49 | 132.9 | -73.66 | 5.83 | areatrigger_teleport 228 | 1 | 1 | The Barrens - Wailing Caverns; V |
| 5 | Razorfen Kraul | 47 | 1943 | 1544.63 | 82 | 1.38 | areatrigger_teleport 244 | 1 | 1 | Razorfen Kraul Entrance; V |
| 6 | Blackfathom Deeps | 48 | -151.89 | 106.96 | -39.87 | 4.53 | areatrigger_teleport 257 | 1 | 1 | Blackphantom Deeps Entrance; V |
| 7 | Uldaman | 70 | -226.8 | 49.09 | -46.03 | 1.39 | areatrigger_teleport 286 | 2 | 1 | Uldaman Entrance; V |
| 8 | Uldaman | 70 | -214.02 | 383.607 | -38.7687 | 0.5 | areatrigger_teleport 902 | 2 | 1 | Uldaman Exit; V; source map 0, despite misleading Exit label |
| 9 | Gnomeregan | 90 | -332.22 | -2.28 | -150.86 | 2.77 | areatrigger_teleport 324 | 2 | 1 | Gnomeregan Entrance; V |
| 10 | Gnomeregan | 90 | -736.51 | 2.71 | -249.99 | 3.14 | areatrigger_teleport 523 | 2 | 1 | Gnomeregan Train Depot Entrance; V |
| 11 | Sunken Temple | 109 | -319.24 | 99.9 | -131.85 | 3.19 | areatrigger_teleport 446 | 1 | 1 | Altar of Atal'Hakkar Entrance; V |
| 12 | Razorfen Downs | 129 | 2592.55 | 1107.5 | 51.29 | 4.74 | areatrigger_teleport 442 | 1 | 1 | Razorfen Downs Entrance; V |
| 13 | Scarlet Monastery | 189 | 1688.99 | 1053.48 | 18.6775 | 0.00117 | areatrigger_teleport 45 | 4 | 1 | Scarlet Monastery - Graveyard (Entrance); V |
| 14 | Scarlet Monastery | 189 | 855.683 | 1321.5 | 18.6709 | 0.001747 | areatrigger_teleport 610 | 4 | 1 | Scarlet Monastery - Cathedral (Entrance); V |
| 15 | Scarlet Monastery | 189 | 1610.83 | -323.433 | 18.6738 | 6.28022 | areatrigger_teleport 612 | 4 | 1 | Scarlet Monastery - Armory (Entrance); V |
| 16 | Scarlet Monastery | 189 | 255.346 | -209.09 | 18.6773 | 6.26656 | areatrigger_teleport 614 | 4 | 1 | Scarlet Monastery - Library (Entrance); V |
| 17 | Zul'Farrak | 209 | 1213.52 | 841.59 | 8.93 | 6.09 | areatrigger_teleport 924 | 1 | 1 | Zul'Farrak Entrance; V |
| 18 | Blackrock Spire | 229 | 78.5083 | -225.044 | 49.839 | 5.1 | areatrigger_teleport 1468 | 1 | 1 | Blackrock Spire - Searing Gorge Instance (Inside); V |
| 19 | Blackrock Depths | 230 | 456.929 | 34.0923 | -68.0896 | 4.71239 | areatrigger_teleport 1466 | 1 | 1 | Blackrock Depths Entrance; V |
| 20 | Opening of the Dark Portal | 269 | -1496.24 | 7034.7 | 32.5619 | 1.75699 | areatrigger_teleport 4320 | 1 | 3 | Caverns Of Time, Black Morass (Entrance); V |
| 21 | Scholomance | 289 | 196.37 | 127.05 | 134.91 | 6.09 | areatrigger_teleport 2567 | 1 | 1 | Scholomance Entrance; V |
| 22 | Stratholme | 329 | 3593.15 | -3646.56 | 138.5 | 5.33 | areatrigger_teleport 2214 | 2 | 1 | Stratholme - Eastern Plaguelands Instance; V |
| 23 | Stratholme | 329 | 3395.09 | -3380.25 | 142.702 | 0.1 | areatrigger_teleport 2216, 2217 | 2 | 1 | Stratholme - Eastern Plaguelands Instance; V; identical destinations consolidated |
| 24 | Maraudon | 349 | 1019.69 | -458.31 | -43.43 | 0.31 | areatrigger_teleport 3133 | 4 | 1 | Maraudon, Foulspore Cavern [Orange Wing] (Entrance); V |
| 25 | Maraudon | 349 | 752.91 | -616.53 | -33.11 | 1.37 | areatrigger_teleport 3134 | 4 | 1 | Maraudon, The Wicked Grotto [Purple Wing] (Entrance); V |
| 26 | Maraudon | 349 | 495.702 | 17.3372 | -96.3128 | 3.11854 | lfg_dungeon_template 273 | 4 | 1 | Maraudon - Pristine Waters; V |
| 27 | Maraudon | 349 | 386.27 | 33.4144 | -130.934 | 0 | spell_target_position 21128/0 | 4 | 1 | Portal to Inner Maraudon (gameobject 178404, exterior spawn 5417); V |
| 28 | Ragefire Chasm | 389 | 3.81 | -14.82 | -17.84 | 4.39 | areatrigger_teleport 2230 | 1 | 1 | Ragefire Chasm - Ogrimmar Instance; V |
| 29 | Dire Maul | 429 | 44.4499 | -154.822 | -2.71201 | 0 | areatrigger_teleport 3183 | 6 | 1 | Dire Maul, East Wing [West] (Entrance); V |
| 30 | Dire Maul | 429 | -201.11 | -328.66 | -2.72 | 5.22 | areatrigger_teleport 3184 | 6 | 1 | Dire Maul, East Wing [South] (Entrance); V |
| 31 | Dire Maul | 429 | 9.31119 | -837.085 | -32.5305 | 0 | areatrigger_teleport 3185 | 6 | 1 | Dire Maul, East Wing [East] (Entrance); V |
| 32 | Dire Maul | 429 | -62.9658 | 159.867 | -3.46206 | 3.14788 | areatrigger_teleport 3186 | 6 | 1 | Dire Maul, West Wing [South] (Entrance); V |
| 33 | Dire Maul | 429 | 31.5609 | 159.45 | -3.4777 | 0.01 | areatrigger_teleport 3187 | 6 | 1 | Dire Maul, West Wing [North] (Entrance); V |
| 34 | Dire Maul | 429 | 255.249 | -16.0561 | -2.58737 | 4.7 | areatrigger_teleport 3189 | 6 | 1 | Dire Maul, North Wing (Entrance); V |
| 35 | Hellfire Citadel: The Shattered Halls | 540 | -40.8716 | -19.7538 | -13.8065 | 1.11133 | areatrigger_teleport 4151 | 1 | 3 | The Shattered Halls (Entrance); V |
| 36 | Hellfire Citadel: The Blood Furnace | 542 | -3.9967 | 14.6363 | -44.8009 | 4.88748 | areatrigger_teleport 4152 | 1 | 3 | The Blood Furnace (Entrance); V |
| 37 | Hellfire Citadel: Ramparts | 543 | -1355.24 | 1641.12 | 68.2491 | 0.6687 | areatrigger_teleport 4150 | 1 | 3 | Hellfire Ramparts (Entrance); V |
| 38 | Coilfang: The Steamvault | 545 | -13.8425 | 6.7542 | -4.2586 | 0 | areatrigger_teleport 4364 | 1 | 3 | The Steamvault (Entrance); V |
| 39 | Coilfang: The Underbog | 546 | 9.71391 | -16.2008 | -2.75334 | 5.57082 | areatrigger_teleport 4363 | 1 | 3 | The Underbog (Entrance); V |
| 40 | Coilfang: The Slave Pens | 547 | 120.101 | -131.957 | -0.801547 | 1.47574 | areatrigger_teleport 4365 | 1 | 3 | The Slave Pens (Entrance); V |
| 41 | Tempest Keep: The Arcatraz | 552 | -1.23165 | 0.0143459 | -0.204293 | 0.0157123 | areatrigger_teleport 4468 | 1 | 3 | The Arcatraz (Entrance); V |
| 42 | Tempest Keep: The Botanica | 553 | 40.0395 | -28.613 | -1.1189 | 2.35856 | areatrigger_teleport 4467 | 1 | 3 | The Botanica (Entrance); V |
| 43 | Tempest Keep: The Mechanar | 554 | -28.906 | 0.680314 | -1.81282 | 0.0345509 | areatrigger_teleport 4469 | 1 | 3 | The Mechanar (Entrance); V |
| 44 | Auchindoun: Shadow Labyrinth | 555 | 0.488033 | -0.215935 | -1.12788 | 3.15888 | areatrigger_teleport 4407 | 1 | 3 | Shadow Labyrinth (Entrance); V |
| 45 | Auchindoun: Sethekk Halls | 556 | -4.6811 | -0.0930796 | 0.0062 | 0.0353424 | areatrigger_teleport 4406 | 1 | 3 | Sethekk Halls (Entrance); V |
| 46 | Auchindoun: Mana-Tombs | 557 | 0.0191 | 0.9478 | -0.9543 | 3.03164 | areatrigger_teleport 4405 | 1 | 3 | Mana Tombs (Entrance); V |
| 47 | Auchindoun: Auchenai Crypts | 558 | -21.8975 | 0.16 | -0.1206 | 0.0353412 | areatrigger_teleport 4404 | 1 | 3 | Auchenai Crypts (Entrance); V |
| 48 | The Escape From Durnholde | 560 | 2741.87 | 1315.25 | 14.0423 | 2.96016 | areatrigger_teleport 4321 | 1 | 3 | Caverns Of Time, Old Hillsbrad Foothills (Entrance); V |
| 49 | Utgarde Keep | 574 | 153.789 | -86.548 | 12.551 | 0.304 | areatrigger_teleport 4745 | 1 | 3 | Utgarde Keep (entrance); V |
| 50 | Utgarde Pinnacle | 575 | 584.117 | -327.974 | 110.138 | 3.122 | areatrigger_teleport 4747 | 1 | 3 | Utgarde Pinnacle (entrance); V |
| 51 | The Nexus | 576 | 145.87 | -10.554 | -16.636 | 1.528 | areatrigger_teleport 4983 | 1 | 3 | The Nexus (entrance); V |
| 52 | The Oculus | 578 | 1055.93 | 986.85 | 361.07 | 5.745 | areatrigger_teleport 5246 | 1 | 3 | The Oculus (entrance); V |
| 53 | Magister's Terrace | 585 | 7.09 | -0.45 | -2.8 | 0.05 | areatrigger_teleport 4887 | 1 | 3 | Magisters' Terrace (Entrance); V |
| 54 | The Culling of Stratholme | 595 | 1431.1 | 556.92 | 36.69 | 5.16 | areatrigger_teleport 5150 | 1 | 3 | Culling of Stratholme (entrance); V |
| 55 | Halls of Stone | 599 | 1153.24 | 806.164 | 195.937 | 4.715 | areatrigger_teleport 5010 | 1 | 3 | Ulduar, Halls of Stone (entrance); V |
| 56 | Drak'Tharon Keep | 600 | -517.343 | -487.976 | 11.01 | 4.831 | areatrigger_teleport 4998 | 1 | 3 | Drak'Tharon Keep (entrance); V |
| 57 | Azjol-Nerub | 601 | 413.314 | 795.968 | 831.351 | 5.5 | areatrigger_teleport 5117 | 1 | 3 | Azjol-Nerub (entrance); V |
| 58 | Halls of Lightning | 602 | 1331.47 | 259.619 | 53.398 | 4.772 | areatrigger_teleport 5093 | 1 | 3 | Ulduar, Halls of Lightning (entrance); V |
| 59 | Gundrak | 604 | 1891.84 | 832.169 | 176.669 | 2.109 | areatrigger_teleport 5205 | 2 | 3 | Gundrak (entrance south); V |
| 60 | Gundrak | 604 | 1882.32 | 631.027 | 176.696 | 3.1415927 | areatrigger_teleport 5206 | 2 | 3 | Gundrak (North Entrance); V; July reference update |
| 61 | Violet Hold | 608 | 1808.82 | 803.93 | 44.364 | 6.282 | areatrigger_teleport 5209 | 1 | 3 | Violet Hold (entrance); V |
| 62 | Ahn'kahet: The Old Kingdom | 619 | 333.351 | -1109.94 | 69.772 | 0.553 | areatrigger_teleport 5215 | 1 | 3 | Ahn'Kahet (entrance); V |
| 63 | The Forge of Souls | 632 | 4922.86 | 2175.63 | 638.734 | 2.00355 | areatrigger_teleport 5635 | 1 | 3 | Forge of Souls (Entrance); V |
| 64 | Trial of the Champion | 650 | 805.227 | 618.038 | 412.393 | 3.1456 | areatrigger_teleport 5505 | 1 | 3 | Trial of the Champion (Entrance); V |
| 65 | Pit of Saron | 658 | 435.743 | 212.413 | 528.709 | 6.25646 | areatrigger_teleport 5637 | 1 | 3 | Pit of Saron (Entrance); V |
| 66 | Halls of Reflection | 668 | 5239.01 | 1932.64 | 707.695 | 0.800565 | areatrigger_teleport 5636 | 1 | 3 | Halls of Reflection (Entrance); V |

## Maps with multiple guides

| Dungeon | Map | Guides | Why one guide cannot serve these arrivals |
|---|---:|---:|---|
| Uldaman | 70 | 2 | Main and rear world entrances, approximately 335 yards apart. Trigger 902's origin is map 0, so its misleading "Exit" name does not make it an outbound teleport. |
| Gnomeregan | 90 | 2 | Main and Train Depot entrances, approximately 416 yards apart with about 99 yards of elevation difference. |
| Scarlet Monastery | 189 | 4 | Graveyard, Cathedral, Armory and Library are separate wings sharing one map. |
| Stratholme | 329 | 2 | Main gate and service entrance. Triggers 2216/2217 are exactly the same main-gate destination and generate only one guide. |
| Maraudon | 349 | 4 | Orange and purple physical wings, Pristine Waters dungeon-finder arrival, and Inner Maraudon portal. The two inner starts are approximately 116 yards apart with about 35 yards of elevation difference, so no unverified shared-visibility assumption is made. |
| Dire Maul | 429 | 6 | Three East-wing entries, two West-wing entries and one North-wing entry. The closest pair (West south/north) is about 95 yards apart; distant portal arrivals should each have an immediately accessible guide. |
| Gundrak | 604 | 2 | North and south entrances are approximately 201 yards apart. Both difficulties share these same two spawn rows. |

## Maps with one guide

Shadowfang Keep (33), Stormwind Stockade (34), Deadmines (36), Wailing Caverns (43), Razorfen Kraul (47), Blackfathom Deeps (48), Sunken Temple (109), Razorfen Downs (129), Zul'Farrak (209), Blackrock Spire (229), Blackrock Depths (230), Opening of the Dark Portal (269), Scholomance (289), Ragefire Chasm (389), Hellfire Citadel: The Shattered Halls (540), Hellfire Citadel: The Blood Furnace (542), Hellfire Citadel: Ramparts (543), Coilfang: The Steamvault (545), Coilfang: The Underbog (546), Coilfang: The Slave Pens (547), Tempest Keep: The Arcatraz (552), Tempest Keep: The Botanica (553), Tempest Keep: The Mechanar (554), Auchindoun: Shadow Labyrinth (555), Auchindoun: Sethekk Halls (556), Auchindoun: Mana-Tombs (557), Auchindoun: Auchenai Crypts (558), The Escape From Durnholde (560), Utgarde Keep (574), Utgarde Pinnacle (575), The Nexus (576), The Oculus (578), Magister's Terrace (585), The Culling of Stratholme (595), Halls of Stone (599), Drak'Tharon Keep (600), Azjol-Nerub (601), Halls of Lightning (602), Violet Hold (608), Ahn'kahet: The Old Kingdom (619), The Forge of Souls (632), Trial of the Champion (650), Pit of Saron (658), Halls of Reflection (668).

## Unresolved Dungeon Entrances

| Dungeon | Map | Reason | Action |
|---|---:|---|---|
| &lt;unused&gt; Monastery | 44 | Present in instance_template and classified as a non-raid instance in the Map.dbc table, but has no trustworthy entrance destination or normal reference creature spawns. It is unused content. | No spawn generated; resolve manually only if this map is intentionally supported on the live realm. |

## Intentionally excluded maps/routes and consolidated arrivals

- The 33 other `instance_template` rows fail the manager's non-raid criterion (raids, battlegrounds,
  arenas or non-instance maps). None gets a guide. Stormwind Vault map 35 has a teleport row but is
  neither a selected instance-template map nor a party instance; no guide is generated there.
- Blackrock Spire trigger 3728 originates inside raid map 469 (Blackwing Lair): it is a raid exit,
  not a normal dungeon entrance, and is excluded. Map 229 still has its normal entrance guide.
- Stratholme 2216/2217 collapse into one location. Normal/heroic LFG records with the same arrival
  do not create extra guides. Scarlet Monastery and Dire Maul LFG wing starts match selected physical entries.
- Blackrock Depths LFG 30/276 share (458.32, 26.52, -70.67, 4.95), only about 8 yards from its physical
  entrance guide on the same start area; no second guide is generated. Check both arrival methods visually.
- Seasonal boss-specific LFG 285/286/287/288 are not normal dungeon entrance deployments; the normal
  map entrances remain covered. Internal transport/boss teleport destinations do not automatically become
  guide spawns. No additional holiday, raid-return, GM, death-return or mid-run teleport guides were added.

## Manual inspection priorities

Inspect all V-marked rows, especially the six Dire Maul entries, four Scarlet Monastery wings, four
Maraudon starts, both Gnomeregan elevations, Uldaman's rear entry, and updated Gundrak north.
Check normal and heroic instances where mask=3. Verify the Hemet model and Expedition Coordinator
subtitle, one Deadmines guide, and unchanged dynamic quest selection/acceptance. Turn-ins stay with
the original questgivers. The operator reports this deployment working on Eitrigg. This release preparation did not repeat live or in-game placement testing.
