-- mod-dungeon-quests: ruRU name and subtitle of the Dungeon Quest Guide (entry 14999991).
-- Re-running is safe: only this entry's ruRU row is replaced.
DELETE FROM `creature_template_locale` WHERE `entry` = 14999991 AND `locale` = 'ruRU';
INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`)
VALUES (14999991, 'ruRU', 'Проводник по подземельям', 'Координатор экспедиций', 0);
