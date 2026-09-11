-- mod-dungeon-quests v1.0.0 canonical state. Execute serially with worldserver stopped.
-- Intentionally resets ALL module-owned guide spawns, including manual adjustments.
-- No updater history required. Only entry/id 14999991 is reconciled.
-- DELIMITER is a mysql client directive (also supported by the reference DB updater).
DROP PROCEDURE IF EXISTS `mod_dungeon_quests_reconcile_v1`;
DELIMITER $$
CREATE PROCEDURE `mod_dungeon_quests_reconcile_v1`()
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        DROP TEMPORARY TABLE IF EXISTS `mod_dungeon_quest_guide_spawn_plan`;
        RESIGNAL;
    END;

    START TRANSACTION;
    IF EXISTS (SELECT 1 FROM `creature_template` WHERE `entry` = 14999991
        AND (BINARY `name` <> 'Dungeon Quest Guide'
            OR BINARY `ScriptName` <> 'npc_dungeon_quest_guide'
            OR `name` IS NULL OR `ScriptName` IS NULL)) THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT = 'mod-dungeon-quests: entry 14999991 collision; no guide data changed';
    END IF;

    INSERT INTO `creature_template`
        (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`,
         `unit_class`, `unit_flags`, `type`, `flags_extra`, `ScriptName`)
    VALUES (14999991, 'Dungeon Quest Guide', 'Expedition Coordinator', 80, 80, 35, 1,
        1, 2, 7, 2, 'npc_dungeon_quest_guide')
    ON DUPLICATE KEY UPDATE
        `name` = 'Dungeon Quest Guide', `subname` = 'Expedition Coordinator',
        `minlevel` = 80, `maxlevel` = 80, `faction` = 35, `npcflag` = 1,
        `unit_class` = 1, `unit_flags` = 2, `type` = 7, `flags_extra` = 2,
        `ScriptName` = 'npc_dungeon_quest_guide';

    DELETE FROM `creature_template_model` WHERE `CreatureID` = 14999991;
    INSERT INTO `creature_template_model`
        (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
    VALUES (14999991, 0, 24969, 1, 1);

-- Session-local placement data; no persistent mapping or quest table is created.
DROP TEMPORARY TABLE IF EXISTS `mod_dungeon_quest_guide_spawn_plan`;
CREATE TEMPORARY TABLE `mod_dungeon_quest_guide_spawn_plan` (
    `slot` SMALLINT UNSIGNED NOT NULL PRIMARY KEY,
    `map` SMALLINT UNSIGNED NOT NULL,
    `x` DOUBLE NOT NULL, `y` DOUBLE NOT NULL, `z` DOUBLE NOT NULL, `o` DOUBLE NOT NULL,
    `spawn_mask` TINYINT UNSIGNED NOT NULL,
    UNIQUE KEY `location` (`map`, `x`, `y`, `z`)
) ENGINE=InnoDB;

-- Exact reference destinations, not guessed terrain offsets. Visual inspection required.
-- spawn_mask is the normal/heroic mask observed in this reference's creature spawns.
INSERT INTO `mod_dungeon_quest_guide_spawn_plan` (`slot`, `map`, `x`, `y`, `z`, `o`, `spawn_mask`) VALUES
-- 1: Shadowfang Keep / Shadowfang Keep Entrance / areatrigger_teleport 145
(1, 33, -229.135, 2109.18, 76.8898, 1.267, 1),
-- 2: Stormwind Stockade / Stormwind Stockades Entrance / areatrigger_teleport 101
(2, 34, 54.23, 0.28, -18.34, 6.26, 1),
-- 3: Deadmines / DeadMines Entrance / areatrigger_teleport 78
(3, 36, -16.4, -383.07, 61.78, 1.86, 1),
-- 4: Wailing Caverns / The Barrens - Wailing Caverns / areatrigger_teleport 228
(4, 43, -163.49, 132.9, -73.66, 5.83, 1),
-- 5: Razorfen Kraul / Razorfen Kraul Entrance / areatrigger_teleport 244
(5, 47, 1943, 1544.63, 82, 1.38, 1),
-- 6: Blackfathom Deeps / Blackphantom Deeps Entrance / areatrigger_teleport 257
(6, 48, -151.89, 106.96, -39.87, 4.53, 1),
-- 7: Uldaman / Uldaman Entrance / areatrigger_teleport 286
(7, 70, -226.8, 49.09, -46.03, 1.39, 1),
-- 8: Uldaman / Uldaman Exit / areatrigger_teleport 902
(8, 70, -214.02, 383.607, -38.7687, 0.5, 1),
-- 9: Gnomeregan / Gnomeregan Entrance / areatrigger_teleport 324
(9, 90, -332.22, -2.28, -150.86, 2.77, 1),
-- 10: Gnomeregan / Gnomeregan Train Depot Entrance / areatrigger_teleport 523
(10, 90, -736.51, 2.71, -249.99, 3.14, 1),
-- 11: Sunken Temple / Altar of Atal'Hakkar Entrance / areatrigger_teleport 446
(11, 109, -319.24, 99.9, -131.85, 3.19, 1),
-- 12: Razorfen Downs / Razorfen Downs Entrance / areatrigger_teleport 442
(12, 129, 2592.55, 1107.5, 51.29, 4.74, 1),
-- 13: Scarlet Monastery / Scarlet Monastery - Graveyard (Entrance) / areatrigger_teleport 45
(13, 189, 1688.99, 1053.48, 18.6775, 0.00117, 1),
-- 14: Scarlet Monastery / Scarlet Monastery - Cathedral (Entrance) / areatrigger_teleport 610
(14, 189, 855.683, 1321.5, 18.6709, 0.001747, 1),
-- 15: Scarlet Monastery / Scarlet Monastery - Armory (Entrance) / areatrigger_teleport 612
(15, 189, 1610.83, -323.433, 18.6738, 6.28022, 1),
-- 16: Scarlet Monastery / Scarlet Monastery - Library (Entrance) / areatrigger_teleport 614
(16, 189, 255.346, -209.09, 18.6773, 6.26656, 1),
-- 17: Zul'Farrak / Zul'Farrak Entrance / areatrigger_teleport 924
(17, 209, 1213.52, 841.59, 8.93, 6.09, 1),
-- 18: Blackrock Spire / Blackrock Spire - Searing Gorge Instance (Inside) / areatrigger_teleport 1468
(18, 229, 78.5083, -225.044, 49.839, 5.1, 1),
-- 19: Blackrock Depths / Blackrock Depths Entrance / areatrigger_teleport 1466
(19, 230, 456.929, 34.0923, -68.0896, 4.71239, 1),
-- 20: Opening of the Dark Portal / Caverns Of Time, Black Morass (Entrance) / areatrigger_teleport 4320
(20, 269, -1496.24, 7034.7, 32.5619, 1.75699, 3),
-- 21: Scholomance / Scholomance Entrance / areatrigger_teleport 2567
(21, 289, 196.37, 127.05, 134.91, 6.09, 1),
-- 22: Stratholme / Stratholme - Eastern Plaguelands Instance / areatrigger_teleport 2214
(22, 329, 3593.15, -3646.56, 138.5, 5.33, 1),
-- 23: Stratholme / Stratholme - Eastern Plaguelands Instance / areatrigger_teleport 2216,2217
(23, 329, 3395.09, -3380.25, 142.702, 0.1, 1),
-- 24: Maraudon / Maraudon, Foulspore Cavern [Orange Wing] (Entrance) / areatrigger_teleport 3133
(24, 349, 1019.69, -458.31, -43.43, 0.31, 1),
-- 25: Maraudon / Maraudon, The Wicked Grotto [Purple Wing] (Entrance) / areatrigger_teleport 3134
(25, 349, 752.91, -616.53, -33.11, 1.37, 1),
-- 26: Maraudon / Maraudon - Pristine Waters / lfg_dungeon_template 273
(26, 349, 495.702, 17.3372, -96.3128, 3.11854, 1),
-- 27: Maraudon / Portal to Inner Maraudon (gameobject 178404, exterior spawn 5417) / spell_target_position 21128/0
(27, 349, 386.27, 33.4144, -130.934, 0, 1),
-- 28: Ragefire Chasm / Ragefire Chasm - Ogrimmar Instance / areatrigger_teleport 2230
(28, 389, 3.81, -14.82, -17.84, 4.39, 1),
-- 29: Dire Maul / Dire Maul, East Wing [West] (Entrance) / areatrigger_teleport 3183
(29, 429, 44.4499, -154.822, -2.71201, 0, 1),
-- 30: Dire Maul / Dire Maul, East Wing [South] (Entrance) / areatrigger_teleport 3184
(30, 429, -201.11, -328.66, -2.72, 5.22, 1),
-- 31: Dire Maul / Dire Maul, East Wing [East] (Entrance) / areatrigger_teleport 3185
(31, 429, 9.31119, -837.085, -32.5305, 0, 1),
-- 32: Dire Maul / Dire Maul, West Wing [South] (Entrance) / areatrigger_teleport 3186
(32, 429, -62.9658, 159.867, -3.46206, 3.14788, 1),
-- 33: Dire Maul / Dire Maul, West Wing [North] (Entrance) / areatrigger_teleport 3187
(33, 429, 31.5609, 159.45, -3.4777, 0.01, 1),
-- 34: Dire Maul / Dire Maul, North Wing (Entrance) / areatrigger_teleport 3189
(34, 429, 255.249, -16.0561, -2.58737, 4.7, 1),
-- 35: Hellfire Citadel: The Shattered Halls / The Shattered Halls (Entrance) / areatrigger_teleport 4151
(35, 540, -40.8716, -19.7538, -13.8065, 1.11133, 3),
-- 36: Hellfire Citadel: The Blood Furnace / The Blood Furnace (Entrance) / areatrigger_teleport 4152
(36, 542, -3.9967, 14.6363, -44.8009, 4.88748, 3),
-- 37: Hellfire Citadel: Ramparts / Hellfire Ramparts (Entrance) / areatrigger_teleport 4150
(37, 543, -1355.24, 1641.12, 68.2491, 0.6687, 3),
-- 38: Coilfang: The Steamvault / The Steamvault (Entrance) / areatrigger_teleport 4364
(38, 545, -13.8425, 6.7542, -4.2586, 0, 3),
-- 39: Coilfang: The Underbog / The Underbog (Entrance) / areatrigger_teleport 4363
(39, 546, 9.71391, -16.2008, -2.75334, 5.57082, 3),
-- 40: Coilfang: The Slave Pens / The Slave Pens (Entrance) / areatrigger_teleport 4365
(40, 547, 120.101, -131.957, -0.801547, 1.47574, 3),
-- 41: Tempest Keep: The Arcatraz / The Arcatraz (Entrance) / areatrigger_teleport 4468
(41, 552, -1.23165, 0.0143459, -0.204293, 0.0157123, 3),
-- 42: Tempest Keep: The Botanica / The Botanica (Entrance) / areatrigger_teleport 4467
(42, 553, 40.0395, -28.613, -1.1189, 2.35856, 3),
-- 43: Tempest Keep: The Mechanar / The Mechanar (Entrance) / areatrigger_teleport 4469
(43, 554, -28.906, 0.680314, -1.81282, 0.0345509, 3),
-- 44: Auchindoun: Shadow Labyrinth / Shadow Labyrinth (Entrance) / areatrigger_teleport 4407
(44, 555, 0.488033, -0.215935, -1.12788, 3.15888, 3),
-- 45: Auchindoun: Sethekk Halls / Sethekk Halls (Entrance) / areatrigger_teleport 4406
(45, 556, -4.6811, -0.0930796, 0.0062, 0.0353424, 3),
-- 46: Auchindoun: Mana-Tombs / Mana Tombs (Entrance) / areatrigger_teleport 4405
(46, 557, 0.0191, 0.9478, -0.9543, 3.03164, 3),
-- 47: Auchindoun: Auchenai Crypts / Auchenai Crypts (Entrance) / areatrigger_teleport 4404
(47, 558, -21.8975, 0.16, -0.1206, 0.0353412, 3),
-- 48: The Escape From Durnholde / Caverns Of Time, Old Hillsbrad Foothills (Entrance) / areatrigger_teleport 4321
(48, 560, 2741.87, 1315.25, 14.0423, 2.96016, 3),
-- 49: Utgarde Keep / Utgarde Keep (entrance) / areatrigger_teleport 4745
(49, 574, 153.789, -86.548, 12.551, 0.304, 3),
-- 50: Utgarde Pinnacle / Utgarde Pinnacle (entrance) / areatrigger_teleport 4747
(50, 575, 584.117, -327.974, 110.138, 3.122, 3),
-- 51: The Nexus / The Nexus (entrance) / areatrigger_teleport 4983
(51, 576, 145.87, -10.554, -16.636, 1.528, 3),
-- 52: The Oculus / The Oculus (entrance) / areatrigger_teleport 5246
(52, 578, 1055.93, 986.85, 361.07, 5.745, 3),
-- 53: Magister's Terrace / Magisters' Terrace (Entrance) / areatrigger_teleport 4887
(53, 585, 7.09, -0.45, -2.8, 0.05, 3),
-- 54: The Culling of Stratholme / Culling of Stratholme (entrance) / areatrigger_teleport 5150
(54, 595, 1431.1, 556.92, 36.69, 5.16, 3),
-- 55: Halls of Stone / Ulduar, Halls of Stone (entrance) / areatrigger_teleport 5010
(55, 599, 1153.24, 806.164, 195.937, 4.715, 3),
-- 56: Drak'Tharon Keep / Drak'Tharon Keep (entrance) / areatrigger_teleport 4998
(56, 600, -517.343, -487.976, 11.01, 4.831, 3),
-- 57: Azjol-Nerub / Azjol-Nerub (entrance) / areatrigger_teleport 5117
(57, 601, 413.314, 795.968, 831.351, 5.5, 3),
-- 58: Halls of Lightning / Ulduar, Halls of Lightning (entrance) / areatrigger_teleport 5093
(58, 602, 1331.47, 259.619, 53.398, 4.772, 3),
-- 59: Gundrak / Gundrak (entrance south) / areatrigger_teleport 5205
(59, 604, 1891.84, 832.169, 176.669, 2.109, 3),
-- 60: Gundrak / Gundrak (North Entrance) / areatrigger_teleport 5206
(60, 604, 1882.32, 631.027, 176.696, 3.1415927, 3),
-- 61: Violet Hold / Violet Hold (entrance) / areatrigger_teleport 5209
(61, 608, 1808.82, 803.93, 44.364, 6.282, 3),
-- 62: Ahn'kahet: The Old Kingdom / Ahn'Kahet (entrance) / areatrigger_teleport 5215
(62, 619, 333.351, -1109.94, 69.772, 0.553, 3),
-- 63: The Forge of Souls / Forge of Souls (Entrance) / areatrigger_teleport 5635
(63, 632, 4922.86, 2175.63, 638.734, 2.00355, 3),
-- 64: Trial of the Champion / Trial of the Champion (Entrance) / areatrigger_teleport 5505
(64, 650, 805.227, 618.038, 412.393, 3.1456, 3),
-- 65: Pit of Saron / Pit of Saron (Entrance) / areatrigger_teleport 5637
(65, 658, 435.743, 212.413, 528.709, 6.25646, 3),
-- 66: Halls of Reflection / Halls of Reflection (Entrance) / areatrigger_teleport 5636
(66, 668, 5239.01, 1932.64, 707.695, 0.800565, 3);


    DELETE FROM `creature` WHERE `id` = 14999991;
    INSERT INTO `creature`
        (`id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`,
         `orientation`, `spawntimesecs`, `curhealth`, `MovementType`)
    SELECT 14999991, p.`map`, p.`spawn_mask`, 1, p.`x`, p.`y`, p.`z`, p.`o`, 300, 1, 0
    FROM `mod_dungeon_quest_guide_spawn_plan` AS p
    ORDER BY p.`slot`;

    DROP TEMPORARY TABLE IF EXISTS `mod_dungeon_quest_guide_spawn_plan`;
    COMMIT;
END$$
DELIMITER ;
CALL `mod_dungeon_quests_reconcile_v1`();
DROP PROCEDURE IF EXISTS `mod_dungeon_quests_reconcile_v1`;
