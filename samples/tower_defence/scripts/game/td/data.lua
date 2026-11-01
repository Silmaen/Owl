-- The constant tables of the rules: tower and power-up names and texts, badges, enemy kinds, sizes.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data

-- Names, badges (frames of `textures/icons.png`) and texts of the towers, their levels and power-ups.
C.KINDS = { "archer", "cannon", "frost" }

-- The name and the blurb of each tower, from `tables/towers.owltable` (`economy.read_towers`).
C.NAMES = {}

C.BLURBS = {}

C.ICON = { archer = 0, cannon = 1, frost = 2, sell = 3, upgrade = 4, locked = 5, pierce = 6, double = 7,
               fire = 8, shock = 9, freeze = 10, aura = 11 }

C.LEVEL_TEXT = {
    archer = { "", "Level 2: heavier arrows, longer range.", "Level 3: faster, heavier volleys." },
    cannon = { "", "Level 2: heavier shells.", "Level 3: wider and heavier bursts." },
    frost = { "", "Level 2: colder, slower enemies.", "Level 3: the chill lasts longer and bites." },
}

C.POWERS = { archer = { "pierce", "double" }, cannon = { "fire", "shock" }, frost = { "freeze", "aura" } }

C.POWER_NAMES = { pierce = "Piercing arrows", double = "Double shot", fire = "Fire shells", shock = "Shock wave",
                      freeze = "Full freeze", aura = "Vulnerability aura" }

C.POWER_TEXT = {
    pierce = "Each arrow also hits the two enemies behind its target.",
    double = "Two targets at each volley.",
    fire = "The ground burns where a shell lands: damage over time.",
    shock = "The enemies hit are thrown back along the road.",
    freeze = "Every third hit stops the enemy for a moment.",
    aura = "Enemies in range take a quarter more damage from every tower.",
}

C.UNLOCK_SETTING = "tower_done_01"

-- What winning each level the first time unlocks, as the tally says it.
C.UNLOCKS = { "the second power-up of each tower", "Rain of arrows", "the Royal treasury",
                  "the Fletchers' guild: archers cost 10% less", "Shield wall",
                  "the Watchtowers: +5% range for every tower", "the Harvest tithe: +10% gold per kill",
                  "the Night watch: +10% range at night", "the Royal guard: 3 more lives",
                  "the Keep walls, with 3 stars on all ten levels" }

C.UNLOCK_NAME = "Greenmarch Ford"

C.PROJECTILE = { archer = "prefabs/projectile_arrow.owlprefab", cannon = "prefabs/projectile_ball.owlprefab",
                     fire = "prefabs/projectile_fireball.owlprefab", frost = "prefabs/projectile_shard.owlprefab" }

C.SHOT_SOUND = { archer = "sounds/game/arrow.wav", cannon = "sounds/game/cannon.wav",
                     frost = "sounds/game/frost.wav" }

-- The subtitles of the sounds that tell something (`ui.show_subtitle`, shown while the `subtitles` setting is on).
C.CAPTIONS = {
    ["sounds/game/horn.wav"] = "[A war horn sounds]",
    ["sounds/game/leak.wav"] = "[Blows on the keep gate]",
    ["sounds/game/defeat.wav"] = "[The bells of the keep toll]",
    ["sounds/game/coins.wav"] = "[Coins jingle]",
}

C.SHOT_HEIGHT = { 1.15, 1.6, 2.05 }   -- height of the weapon over the pad, by tower level

C.PAD_TOP = 0.21                      -- height of a pad over the grass

C.GRASS_TOP = 0.4                     -- height of the grass over the base of a tile

-- Sprites: the foot stands 34 pixels under the centre of a 96-pixel cell (scaled with the sprite).
C.FOOT_SHARE = 34.0 / 96.0

C.TOWARD_VIEW = 0.25                  -- billboards come this far toward the camera, off the ground they stand on

-- The Horde, from `tables/enemies.owltable` (`economy.read_enemies`): the kinds in the order of the table, the name of
-- each and how to beat it (shown the first time it marches in a level), the height of its health bar, its shadow.
C.FOE_KINDS = {}

C.FOES = {}

C.BAR_HEIGHT = {}

C.SHADOW_SIZE = {}

-- Upgrades bought with the stars on the upgrades screen: the tier of each family (`tower_up_<family>` settings).
C.UPGRADE_FAMILIES = { "archer", "cannon", "frost", "rain", "shield", "treasury" }

C.DIRECTIONS = 8

C.ATTACK_ROW, C.DEATH_ROW = 4, 6       -- rows of the enemy sheets after the 4 of the walk

-- Pixels of the wheel around its centre, on the screen: the ring, the badges on it, the scroll with the info below.
C.WHEEL = { ring = 196, badge = 58, radius = 64, cost = 19, cost_drop = 40, plaque_w = 330, plaque_h = 113,
                plaque_drop = 168, title = 22, text = 15, line = 18, margin = 16 }

-- Angles of the badges (degrees, counter-clockwise from the right) for 1 to 4 choices.
C.WHEEL_ANGLES = { { 90 }, { 150, 30 }, { 150, 30, 270 }, { 135, 45, 315, 225 } }

C.WRAP = 38   -- characters of a line of the info

return td.data
