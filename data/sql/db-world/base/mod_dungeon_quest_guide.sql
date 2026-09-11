-- PASS 2: one reusable guide and ONE Deadmines entrance test spawn.
-- Targets the supplied live schema: creature.id (NOT upstream's newer id1).
-- Entry checked against local custom modules and the upstream base world dump.
-- The live database must still be checked; collisions are never overwritten.
-- Run with worldserver stopped and no concurrent world-data imports.
SET @DQ_ENTRY := 14999991;
SET @DQ_CREATE := NOT EXISTS (SELECT 1 FROM `creature_template` WHERE `entry` = @DQ_ENTRY)
    AND NOT EXISTS (SELECT 1 FROM `creature_template_model` WHERE `CreatureID` = @DQ_ENTRY)
    AND NOT EXISTS (SELECT 1 FROM `creature` WHERE `id` = @DQ_ENTRY);

INSERT INTO `creature_template`
    (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`,
     `unit_class`, `unit_flags`, `type`, `flags_extra`, `ScriptName`)
SELECT @DQ_ENTRY, 'Dungeon Quest Guide', 'Dungeon Quests', 80, 80, 35, 1,
    1, 2, 7, 2, 'npc_dungeon_quest_guide'
WHERE @DQ_CREATE;

-- Existing WotLK model: Spirit Healer (creature 6491), display 5233.
-- Only the appearance is reused; this guide has gossip, not spirit-healer flags.
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
SELECT @DQ_ENTRY, 0, 5233, 1, 1
WHERE @DQ_CREATE;

-- Re-running is safe. An occupied entry with a different identity is left alone.
SET @DQ_OWNED := EXISTS (SELECT 1 FROM `creature_template`
    WHERE `entry` = @DQ_ENTRY AND `ScriptName` = 'npc_dungeon_quest_guide'
        AND `name` = 'Dungeon Quest Guide' AND `subname` = 'Dungeon Quests')
    AND EXISTS (SELECT 1 FROM `creature_template_model`
        WHERE `CreatureID` = @DQ_ENTRY AND `Idx` = 0 AND `CreatureDisplayID` = 5233);

-- Exact core areatrigger_teleport entry 78 destination: DeadMines Entrance.
-- Using the known arrival point avoids inventing an unverified terrain position.
-- guid is allocated by creature.AUTO_INCREMENT, not a fixed global spawn ID.
INSERT INTO `creature`
    (`id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`,
     `orientation`, `spawntimesecs`, `curhealth`, `MovementType`)
SELECT @DQ_ENTRY, 36, 1, 1, -16.4, -383.07, 61.78, 1.86, 300, 1, 0
WHERE @DQ_OWNED AND NOT EXISTS (SELECT 1 FROM `creature` WHERE `id` = @DQ_ENTRY AND `map` = 36);

-- No creature_queststarter / creature_questender rows are needed.
SELECT IF(@DQ_OWNED, 'Dungeon Quest Guide installed; verify map 36 spawn.',
    'Dungeon Quest Guide NOT installed: entry collision or incomplete template; inspect before retrying.') AS `DungeonQuestsResult`;
