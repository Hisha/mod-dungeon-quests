-- ---------------------------------------------------------------------------
-- mod-dungeon-quests: manual override table for dungeon quest discovery.
--
-- Placed under data/sql/db-world/base/ so the AzerothCore DB updater applies
-- it when the module is installed under modules/ (world database updates
-- enabled). Re-running is safe (CREATE TABLE IF NOT EXISTS).
--
-- Rows are optional. Automatic discovery works without this table; overrides
-- only tune the derived results.
--
--   action = 1  force include: add quest_id to map_id even if automatic
--               discovery did not associate them.
--   action = 0  force exclude: remove quest_id from map_id even if automatic
--               discovery associated them.
--
-- Invalid rows (unknown map id, unknown quest id, or an action other than
-- 0/1) are logged and ignored at load time rather than crashing the server.
-- ---------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS `mod_dungeon_quest_override` (
    `map_id` SMALLINT UNSIGNED NOT NULL COMMENT 'Dungeon instance map id',
    `quest_id` INT UNSIGNED NOT NULL COMMENT 'Quest id',
    `action` TINYINT NOT NULL DEFAULT 1 COMMENT '1 = force include, 0 = force exclude',
    PRIMARY KEY (`map_id`, `quest_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Dungeon quest discovery manual overrides (mod-dungeon-quests)';

-- Intentionally no INSERT, UPDATE, REPLACE, DELETE or TRUNCATE of override rows.
-- Repeated execution only ensures the table exists; administrator contents survive.
