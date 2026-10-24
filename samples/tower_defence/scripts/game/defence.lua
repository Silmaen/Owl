-- The rules of a Tower defence level, on the `Game` entity of each level scene.
--
-- - The level gives its waves, its gold and its lives as properties (`waves`: one string per wave, groups
--   "kind count gap" separated by ";"); the pads are the `Pad` entities, the road is the `Waypoint NN` entities in
--   the order of their numbers, from the gate to the keep. A new level is a duplicated scene: move the tiles, the
--   pads and the waypoints in Owl Nest, edit the waves.
-- - The values of the towers and of the enemies are properties of this script, the same for every level unless a
--   level overrides one in the inspector. A pad on high ground gives its tower `height_bonus` more range per step.
-- - Towers go from level 1 to 3, then take one power-up of two; the second power-up of each tower is unlocked by
--   winning the first level (`tower_done_01`). The upgrades bought with the stars (`tower_up_<family>` settings, the
--   upgrades screen) raise the towers, the spells and the treasury of every level.
-- - The Horde: goblins, grunts, ogres, wolf riders, trolls (they heal), ogre mages (they ward the enemies around them
--   against damage, fire burns through), goblin sappers (3 lives at the keep, a blast hurting the Horde when killed)
--   and the chieftain (armoured, rallies the Horde, half slowed and never frozen, 10 lives). The first one of a kind in
--   a level shows its name and how to beat it.
-- - Waves are timed: a countdown before each one; calling it early (Space, pad Y, the HUD button) pays
--   `early_bonus` gold per second left. Every enemy reaching the keep costs a life. From the wave `night_from` the
--   sky turns to dusk then night and the torches are lit.
-- - Enemies, towers, projectiles, effects and health bars are prefabs spawned at runtime (`scene.spawn_prefab`); the
--   enemies are billboards whose frame (8 directions x 4 walk frames) follows their heading and the view.
-- - A click on a pad or a tower opens its build ring (badges over it, the range ring of the choice, the info panel
--   of the HUD); the right button, Backspace or pad B closes it. The keyboard and the gamepad move a cursor from pad
--   to pad (Q / E, Tab, the D-pad), Enter or pad A opens the ring and picks, 1 to 3 pick a badge, X sells.
-- - The camera pans (WASD, arrows, left stick, mouse on an edge), turns by 45 degrees (Z / C, pad LB / LT) and
--   zooms (wheel, R / V, right stick), always looking down at the same angle.
-- - F or pad RB doubles the speed, Escape, P or Start pauses: the pause window is the only place that shows the
--   controls during a level.
-- - The state is mirrored in `gamestate` (`tower_gold`, `tower_lives`, `tower_wave`, ...) for the scenarios; the
--   level done is the `tower_done_NN` setting with its stars, the end stats go to the victory scene as
--   `tower_result_*`.

properties = {
    { name = "level_number", type = "int", default = 1, min = 1 },
    { name = "level_name", type = "string", default = "Greenmarch Ford" },
    { name = "scene_path", type = "string", default = "scenes/level_01.owl" },
    { name = "next_scene", type = "string", default = "scenes/victory.owl" },
    { name = "waves", type = "strings", default = { "goblin 6 1.0" } },
    { name = "start_towers", type = "strings", default = {} },
    { name = "night_from", type = "int", default = 0, min = 0 },
    { name = "start_hour", type = "float", default = 9.5, min = 0, max = 24 },
    { name = "night_hour", type = "float", default = 21.5, min = 0, max = 24 },
    { name = "start_gold", type = "int", default = 240, min = 0 },
    { name = "lives", type = "int", default = 20, min = 1 },
    { name = "map_width", type = "int", default = 18, min = 1 },
    { name = "map_depth", type = "int", default = 14, min = 1 },
    { name = "first_wave_delay", type = "float", default = 25.0, min = 0 },
    { name = "wave_delay", type = "float", default = 18.0, min = 0 },
    { name = "early_bonus", type = "float", default = 2.0, min = 0 },
    { name = "hp_growth", type = "float", default = 0.18, min = 0 },
    { name = "sell_share", type = "float", default = 0.5, min = 0, max = 1 },
    { name = "height_step", type = "float", default = 0.4, min = 0.01 },
    { name = "height_bonus", type = "float", default = 0.15, min = 0 },
    { name = "upgrade_2_share", type = "float", default = 0.8, min = 0 },
    { name = "upgrade_3_share", type = "float", default = 1.2, min = 0 },
    { name = "power_share", type = "float", default = 1.5, min = 0 },
    { name = "archer_cost", type = "int", default = 70 },
    { name = "archer_range", type = "float", default = 3.0 },
    { name = "archer_damage", type = "float", default = 9.0 },
    { name = "archer_reload", type = "float", default = 0.65 },
    { name = "cannon_cost", type = "int", default = 110 },
    { name = "cannon_range", type = "float", default = 2.8 },
    { name = "cannon_damage", type = "float", default = 24.0 },
    { name = "cannon_reload", type = "float", default = 1.7 },
    { name = "cannon_splash", type = "float", default = 1.1 },
    { name = "frost_cost", type = "int", default = 90 },
    { name = "frost_range", type = "float", default = 2.6 },
    { name = "frost_damage", type = "float", default = 3.0 },
    { name = "frost_reload", type = "float", default = 1.0 },
    { name = "frost_slow", type = "float", default = 0.5, min = 0.05, max = 1 },
    { name = "frost_time", type = "float", default = 1.8, min = 0 },
    { name = "fire_time", type = "float", default = 3.0, min = 0 },
    { name = "fire_damage", type = "float", default = 7.0, min = 0 },
    { name = "shock_push", type = "float", default = 0.8, min = 0 },
    { name = "freeze_time", type = "float", default = 1.0, min = 0 },
    { name = "aura_damage", type = "float", default = 1.25, min = 1 },
    { name = "rain_damage", type = "float", default = 70.0, min = 0 },
    { name = "rain_radius", type = "float", default = 1.7, min = 0 },
    { name = "rain_cooldown", type = "float", default = 45.0, min = 0 },
    { name = "shield_time", type = "float", default = 5.0, min = 0 },
    { name = "shield_radius", type = "float", default = 1.8, min = 0 },
    { name = "shield_cooldown", type = "float", default = 60.0, min = 0 },
    { name = "treasury_gold", type = "int", default = 60, min = 0 },
    { name = "walls_lives", type = "int", default = 5, min = 0 },
    { name = "goblin_hp", type = "float", default = 34.0 },
    { name = "goblin_speed", type = "float", default = 1.6 },
    { name = "goblin_gold", type = "int", default = 9 },
    { name = "grunt_hp", type = "float", default = 92.0 },
    { name = "grunt_speed", type = "float", default = 1.0 },
    { name = "grunt_gold", type = "int", default = 15 },
    { name = "ogre_hp", type = "float", default = 300.0 },
    { name = "ogre_speed", type = "float", default = 0.65 },
    { name = "ogre_gold", type = "int", default = 36 },
    { name = "ogre_arrow", type = "float", default = 0.4, min = 0, max = 1 },
    { name = "wolf_hp", type = "float", default = 50.0 },
    { name = "wolf_speed", type = "float", default = 2.1 },
    { name = "wolf_gold", type = "int", default = 9 },
    { name = "troll_hp", type = "float", default = 130.0 },
    { name = "troll_speed", type = "float", default = 1.05 },
    { name = "troll_gold", type = "int", default = 18 },
    { name = "troll_regen", type = "float", default = 7.0, min = 0 },
    { name = "mage_hp", type = "float", default = 160.0 },
    { name = "mage_speed", type = "float", default = 0.8 },
    { name = "mage_gold", type = "int", default = 26 },
    { name = "mage_ward", type = "float", default = 45.0, min = 0 },
    { name = "mage_ward_every", type = "float", default = 6.0, min = 0.5 },
    { name = "mage_ward_radius", type = "float", default = 1.6, min = 0 },
    { name = "sapper_hp", type = "float", default = 42.0 },
    { name = "sapper_speed", type = "float", default = 1.75 },
    { name = "sapper_gold", type = "int", default = 12 },
    { name = "sapper_lives", type = "int", default = 3, min = 1 },
    { name = "sapper_blast", type = "float", default = 40.0, min = 0 },
    { name = "sapper_blast_radius", type = "float", default = 1.2, min = 0 },
    { name = "chieftain_hp", type = "float", default = 2000.0 },
    { name = "chieftain_speed", type = "float", default = 0.5 },
    { name = "chieftain_gold", type = "int", default = 200 },
    { name = "chieftain_lives", type = "int", default = 10, min = 1 },
    { name = "chieftain_arrow", type = "float", default = 0.5, min = 0, max = 1 },
    { name = "chieftain_rally", type = "float", default = 1.25, min = 1 },
    { name = "chieftain_rally_radius", type = "float", default = 2.0, min = 0 },
    { name = "arrow_speed", type = "float", default = 9.0, min = 0.1 },
    { name = "ball_flight", type = "float", default = 0.8, min = 0.1 },
    { name = "camera_pitch", type = "float", default = -0.75, min = -1.5, max = -0.2 },
    { name = "camera_fov", type = "float", default = 0.62, min = 0.1, max = 2.5 },
    { name = "weapon_yaw", type = "float", default = 0.0 },
    { name = "camera_distance", type = "float", default = 19.0, min = 2 },
    { name = "zoom_min", type = "float", default = 8.0, min = 1 },
    { name = "zoom_max", type = "float", default = 26.0, min = 1 },
    { name = "pan_speed", type = "float", default = 7.0, min = 0 },
    { name = "edge_pixels", type = "float", default = 14.0, min = 0 },
    { name = "end_delay", type = "float", default = 2.5, min = 0 },
    { name = "wipe_color", type = "color", default = { 0.1, 0.14, 0.24, 1 } },
}

-- Names, badges (frames of `textures/icons.png`) and texts of the towers, their levels and power-ups.
local KINDS = { "archer", "cannon", "frost" }
local NAMES = { archer = "Archer tower", cannon = "Cannon tower", frost = "Frost tower" }
local ICON = { archer = 0, cannon = 1, frost = 2, sell = 3, upgrade = 4, locked = 5, pierce = 6, double = 7,
               fire = 8, shock = 9, freeze = 10, aura = 11 }
local BLURBS = {
    archer = "Fast arrows at one enemy. Ogres shrug most of them off.",
    cannon = "Slow shells bursting on a group, aimed where it will be.",
    frost = "Chills an enemy: slower for a while, a little damage.",
}
local LEVEL_TEXT = {
    archer = { "", "Level 2: heavier arrows, longer range.", "Level 3: faster, heavier volleys." },
    cannon = { "", "Level 2: heavier shells.", "Level 3: wider and heavier bursts." },
    frost = { "", "Level 2: colder, slower enemies.", "Level 3: the chill lasts longer and bites." },
}
local POWERS = { archer = { "pierce", "double" }, cannon = { "fire", "shock" }, frost = { "freeze", "aura" } }
local POWER_NAMES = { pierce = "Piercing arrows", double = "Double shot", fire = "Fire shells", shock = "Shock wave",
                      freeze = "Full freeze", aura = "Vulnerability aura" }
local POWER_TEXT = {
    pierce = "Each arrow also hits the two enemies behind its target.",
    double = "Two targets at each volley.",
    fire = "The ground burns where a shell lands: damage over time.",
    shock = "The enemies hit are thrown back along the road.",
    freeze = "Every third hit stops the enemy for a moment.",
    aura = "Enemies in range take a quarter more damage from every tower.",
}
local UNLOCK_SETTING = "tower_done_01"
-- What winning each level the first time unlocks, as the tally says it.
local UNLOCKS = { "the second power-up of each tower", "Rain of arrows", "the Royal treasury", "Shield wall",
                  "the Keep walls, with 3 stars on all five levels" }
local UNLOCK_NAME = "Greenmarch Ford"
local PROJECTILE = { archer = "prefabs/projectile_arrow.owlprefab", cannon = "prefabs/projectile_ball.owlprefab",
                     fire = "prefabs/projectile_fireball.owlprefab", frost = "prefabs/projectile_shard.owlprefab" }
local SHOT_SOUND = { archer = "sounds/game/arrow.wav", cannon = "sounds/game/cannon.wav",
                     frost = "sounds/game/frost.wav" }
local SHOT_HEIGHT = { 1.15, 1.6, 2.05 }   -- height of the weapon over the pad, by tower level
local PAD_TOP = 0.21                      -- height of a pad over the grass
local GRASS_TOP = 0.4                     -- height of the grass over the base of a tile
-- Sprites: the foot stands 34 pixels under the centre of a 96-pixel cell (scaled with the sprite).
local FOOT_SHARE = 34.0 / 96.0
local sprite_lift = 0.5
local TOWARD_VIEW = 0.25                  -- billboards come this far toward the camera, off the ground they stand on
local BAR_HEIGHT = { goblin = 0.62, grunt = 0.86, ogre = 1.12, wolf = 0.82, troll = 0.98, ogre_mage = 1.1,
                     sapper = 0.66, chieftain = 1.5 }
local SHADOW_SIZE = { goblin = 0.45, grunt = 0.6, ogre = 0.9, wolf = 0.8, troll = 0.6, ogre_mage = 0.85, sapper = 0.45,
                      chieftain = 1.1 }
-- The name of each kind of the Horde and how to beat it, shown the first time it marches in a level.
local FOES = {
    goblin = { "Goblins", "quick and frail: archers" },
    grunt = { "Orc grunts", "sturdy: any tower" },
    ogre = { "Ogres", "arrows glance off them: cannons and frost" },
    wolf = { "Wolf riders", "very fast: frost towers slow them" },
    troll = { "Trolls", "they heal: burst them with cannons, fire stops the healing" },
    ogre_mage = { "Ogre mages", "they ward the Horde around them: kill them first, fire burns through wards" },
    sapper = { "Goblin sappers", "a keg blows 3 lives at the keep: shoot them early, their blast hurts the Horde" },
    chieftain = { "The orc chieftain", "armoured, rallies the Horde, 10 lives: cannons and the vulnerability aura" },
}
-- Upgrades bought with the stars on the upgrades screen: the tier of each family (`tower_up_<family>` settings).
local UPGRADE_FAMILIES = { "archer", "cannon", "frost", "rain", "shield", "treasury" }
local bought = {}
local foes_seen = {}
local DIRECTIONS = 8

local camera_id, cursor_id, ring_id, sky_id = 0, 0, 0, 0
local choice_ids = {}
local wheel_ids = { ring = 0, plaque = 0, title = 0, text = 0, costs = {} }
local wheel_shown = {}      -- the text each world text of the wheel shows
local banner_ids = {}       -- the waving banners: { id, yaw }
local spells = {}           -- the spells the campaign unlocked: rain of arrows, shield wall
local torch_ids = {}
local hud = {}
local shown = {}

local stats = {}            -- tower stats by kind, enemy stats by kind, read from the properties
local pads = {}             -- list of { id, x, y, z, level, tower }
local pad_by_id = {}
local route = nil           -- road A: the waypoints, with the distance walked at each one
local routes = {}           -- road A, and road B of a level with two gates

local enemies, towers, projectiles, effects = {}, {}, {}, {}
local corpses, sparks = {}, {}
local aim                   -- turns the weapons of a tower (defined with the towers' placement)
local MAX_SPARKS = 90
local ATTACK_ROW, DEATH_ROW = 4, 6       -- rows of the enemy sheets after the 4 of the walk
local gold, lives_left = 0, 0
local wave, wave_count = 0, 0
local countdown, spawning = 0.0, nil    -- seconds before the next wave; the queue of the wave being sent
local spawn_clock = 0.0
local spawned = 0
local paused, fast = false, false
local ending, end_clock = nil, 0.0
local clock = 0.0
local message_clock = 0.0
local kills, leaks, built, earned, bonus_total, upgrades = 0, 0, 0, 0, 0, 0
local travel, paused_travel, travel_at_pause = 0.0, 0.0, nil
local last_shot_sound = {}
local hours, hours_goal = 9.5, 9.5
local night_lit = nil

local ring = nil            -- { pad, choices = { {kind, id, locked}, ... } } while the ring is open
local hover = 0             -- index of the highlighted choice
local cursor_pad = nil
local pad_mode = false
local last_mouse_x, last_mouse_y = nil, nil
local mouse_moved = false

local view = { x = 9, z = 7, yaw = 0.0, yaw_goal = 0.0, distance = 15.0, eye = { 0.0, 0.0, 0.0 } }
local basis = {}            -- camera axes of the frame: right, up, forward, toward

-- Helpers -------------------------------------------------------------------------------------------------------------

local function set_text(key, text)
    if hud[key] ~= nil and hud[key] ~= 0 and shown[key] ~= text then
        ui.set_text(hud[key], text)
        shown[key] = text
    end
end

local function show(id, visible)
    if id ~= nil and id ~= 0 then ui.set_visible(id, visible) end
end

local function message(text, seconds)
    set_text("message", text)
    show(hud.message, true)
    message_clock = seconds or 2.0
end

local function play(path)
    sound.play(path)
end

local function unlocked_second_power()
    return settings.get(UNLOCK_SETTING, false) == true
end

local function step_cost(kind, level)
    local base = stats[kind].cost
    if level == 2 then return math.floor(base * upgrade_2_share + 0.5) end
    if level == 3 then return math.floor(base * upgrade_3_share + 0.5) end
    return math.floor(base * power_share + 0.5)
end

local function refund_of(tower)
    return math.floor(tower.spent * sell_share + 0.5)
end

local function mirror()
    gamestate.set("tower_gold", gold)
    gamestate.set("tower_lives", math.max(0, lives_left))
    gamestate.set("tower_wave", wave)
    gamestate.set("tower_waves", wave_count)
    gamestate.set("tower_countdown", math.ceil(countdown))
    gamestate.set("tower_towers", #towers)
    gamestate.set("tower_kills", kills)
    gamestate.set("tower_leaks", leaks)
    gamestate.set("tower_spawned", spawned)
    gamestate.set("tower_alive", #enemies)
    gamestate.set("tower_ring", ring == nil and 0 or (ring.pad.tower ~= nil and 2 or 1))
    gamestate.set("tower_hover", hover)
    gamestate.set("tower_paused", paused)
    gamestate.set("tower_speed", time.get_scale())
    gamestate.set("tower_travel", math.floor(travel * 100) / 100)
    gamestate.set("tower_paused_travel", paused_travel)
    gamestate.set("tower_bonus", bonus_total)
    gamestate.set("tower_cursor", cursor_pad and cursor_pad.index or 0)
    gamestate.set("tower_yaw", math.floor(view.yaw_goal * 180 / math.pi + 0.5))
    gamestate.set("tower_hours", math.floor(hours * 10) / 10)
    gamestate.set("tower_night", night_lit == true)
    gamestate.set("tower_upgrades", upgrades)
    local top, power = 0, ""
    for _, tower in ipairs(towers) do
        if tower.level > top then top = tower.level end
        if tower.power ~= nil then power = tower.power end
    end
    gamestate.set("tower_top_level", top)
    gamestate.set("tower_power", power)
    local first = enemies[1]
    gamestate.set("tower_enemy_frame", first and first.frame or -1)
    gamestate.set("tower_enemy_direction", first and first.frame % DIRECTIONS or -1)
end

-- Camera --------------------------------------------------------------------------------------------------------------

local function update_basis()
    local cy, sy = math.cos(view.yaw), math.sin(view.yaw)
    local cp, sp = math.cos(camera_pitch), math.sin(camera_pitch)
    basis.right = { cy, 0.0, -sy }
    basis.up = { sp * sy, cp, sp * cy }
    basis.forward = { -cp * sy, sp, -cp * cy }
    basis.toward = { sy, cy }       -- toward the camera on the ground plane
end

local function place_camera()
    if camera_id == 0 then return end
    update_basis()
    local f = basis.forward
    local target_y = GRASS_TOP
    view.eye = { view.x - f[1] * view.distance, target_y - f[2] * view.distance, view.z - f[3] * view.distance }
    transform.set_position(camera_id, view.eye[1], view.eye[2], view.eye[3])
    transform.set_rotation(camera_id, camera_pitch, view.yaw, 0.0)
end

-- The centre of a billboard whose foot (22 pixels under the cell centre) stands at a world point.
local function billboard_at(x, y, z, lift)
    local up, f = basis.up, basis.forward
    lift = lift or sprite_lift
    return x + up[1] * lift - f[1] * TOWARD_VIEW, y + up[2] * lift - f[2] * TOWARD_VIEW,
            z + up[3] * lift - f[3] * TOWARD_VIEW
end

-- Set up --------------------------------------------------------------------------------------------------------------

local function tier(family, level)
    return (bought[family] or 0) >= level
end

-- The values of the towers and the enemies, the tiers bought with the stars applied to the towers.
local function read_stats()
    for _, family in ipairs(UPGRADE_FAMILIES) do
        bought[family] = settings.get("tower_up_" .. family, 0)
    end
    stats.archer = { cost = archer_cost, range = archer_range, damage = archer_damage, reload = archer_reload }
    stats.cannon = { cost = cannon_cost, range = cannon_range, damage = cannon_damage, reload = cannon_reload,
                     splash = cannon_splash }
    stats.frost = { cost = frost_cost, range = frost_range, damage = frost_damage, reload = frost_reload,
                    slow = frost_slow, time = frost_time }
    local a, c, f = stats.archer, stats.cannon, stats.frost
    if tier("archer", 1) then a.damage = a.damage * 1.15 end
    if tier("archer", 2) then a.range = a.range * 1.12 end
    if tier("archer", 3) then a.reload = a.reload * 0.85 end
    if tier("cannon", 1) then c.cost = math.floor(c.cost * 0.85 + 0.5) end
    if tier("cannon", 2) then c.splash = c.splash * 1.2 end
    if tier("cannon", 3) then c.damage = c.damage * 1.2 end
    if tier("frost", 1) then f.time = f.time * 1.3 end
    if tier("frost", 2) then f.slow = f.slow * 0.85 end
    if tier("frost", 3) then f.range = f.range * 1.12 end
    stats.goblin = { hp = goblin_hp, speed = goblin_speed, gold = goblin_gold, arrow = 1.0 }
    stats.grunt = { hp = grunt_hp, speed = grunt_speed, gold = grunt_gold, arrow = 1.0 }
    stats.ogre = { hp = ogre_hp, speed = ogre_speed, gold = ogre_gold, arrow = ogre_arrow }
    stats.wolf = { hp = wolf_hp, speed = wolf_speed, gold = wolf_gold, arrow = 1.0 }
    stats.troll = { hp = troll_hp, speed = troll_speed, gold = troll_gold, arrow = 1.0, regen = troll_regen }
    stats.ogre_mage = { hp = mage_hp, speed = mage_speed, gold = mage_gold, arrow = 1.0, ward = mage_ward }
    stats.sapper = { hp = sapper_hp, speed = sapper_speed, gold = sapper_gold, arrow = 1.0, lives = sapper_lives,
                     blast = sapper_blast }
    stats.chieftain = { hp = chieftain_hp, speed = chieftain_speed, gold = chieftain_gold, arrow = chieftain_arrow,
                        lives = chieftain_lives, boss = true, scale = 1.35 }
    local bounty = tier("treasury", 2) and 1.1 or 1.0
    for _, kind in ipairs({ "goblin", "grunt", "ogre", "wolf", "troll", "ogre_mage", "sapper", "chieftain" }) do
        stats[kind].gold = math.floor(stats[kind].gold * bounty + 0.5)
    end
end

-- The stats of a tower at its level and power-up, its range grown by the height of its pad.
local function tower_stats(tower)
    local base = stats[tower.kind]
    local s = { range = base.range, damage = base.damage, reload = base.reload, splash = base.splash,
                slow = base.slow, time = base.time }
    if tower.level >= 2 then
        if tower.kind == "archer" then s.range, s.damage = s.range * 1.15, s.damage * 1.5
        elseif tower.kind == "cannon" then s.damage = s.damage * 1.5
        else s.slow, s.damage = s.slow * 0.8, s.damage * 1.5 end
    end
    if tower.level >= 3 then
        if tower.kind == "archer" then s.reload, s.damage = s.reload * 0.65, s.damage * 1.2
        elseif tower.kind == "cannon" then s.splash, s.damage = s.splash * 1.35, s.damage * 1.2
        else s.time, s.damage = s.time * 1.45, s.damage * 1.3 end
    end
    s.range = s.range * (1.0 + height_bonus * tower.pad.level)
    return s
end

-- The pads, numbered row by row from the north-west (the numbers of `start_towers` and of the scenarios).
local function find_pads()
    for _, id in ipairs(scene.find_entities("Pad")) do
        local x, y, z = transform.get_position(id)
        local level = math.max(0, math.floor((y - GRASS_TOP) / height_step + 0.5))
        local pad = { id = id, x = x, y = y, z = z, level = level, tower = nil }
        pads[#pads + 1] = pad
        pad_by_id[id] = pad
    end
    table.sort(pads, function(a, b)
        if math.abs(a.z - b.z) > 0.01 then return a.z < b.z end
        return a.x < b.x
    end)
    for index, pad in ipairs(pads) do pad.index = index end
end

-- The roads: `Waypoint NN` (road A), then `Waypoint B NN` (road B, a level with two gates), in the order of their
-- numbers, with the distance walked at each point.
local function read_road(prefix)
    local points = {}
    for number = 1, 99 do
        local id = scene.find_entity(string.format("Waypoint %s%02d", prefix, number))
        if id ~= 0 then
            local x, y, z = transform.get_position(id)
            points[#points + 1] = { x = x, y = y, z = z, at = 0.0 }
        end
    end
    if #points < 2 then return nil end
    local road = { points = points, total = 0.0 }
    for i = 2, #points do
        local a, b = points[i - 1], points[i]
        local dx, dy, dz = b.x - a.x, b.y - a.y, b.z - a.z
        road.total = road.total + math.sqrt(dx * dx + dy * dy + dz * dz)
        b.at = road.total
    end
    return road
end

local function find_route()
    routes[1] = read_road("")
    routes[2] = read_road("B ")
    route = routes[1]
    if route == nil then
        log.warn("Tower defence: the level needs at least two Waypoint NN entities.")
        return
    end
    gamestate.set("tower_route", #route.points)
    gamestate.set("tower_roads", routes[2] ~= nil and 2 or 1)
end

-- The point of a road (1 or 2) at a distance from its gate and the direction of the road there (on the ground plane).
local function along(distance, road)
    local points = (routes[road or 1] or route).points
    local index = 1
    while index < #points - 1 and points[index + 1].at < distance do index = index + 1 end
    local a, b = points[index], points[index + 1]
    local length = b.at - a.at
    local t = length > 0 and math.max(0.0, math.min(1.0, (distance - a.at) / length)) or 0.0
    local dx, dz = b.x - a.x, b.z - a.z
    local flat = math.sqrt(dx * dx + dz * dz)
    if flat > 0 then dx, dz = dx / flat, dz / flat end
    return a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, dx, dz
end

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("wipe_down", 0.4, wipe_color.r, wipe_color.g, wipe_color.b)
    end
    time.set_scale(1.0)
    time.set_paused(false)
    read_stats()
    camera_id = scene.find_entity("MainCamera")
    cursor_id = scene.find_entity("Cursor")
    ring_id = scene.find_entity("RangeRing")
    sky_id = scene.find_entity("Sky")
    choice_ids = scene.find_entities("BuildChoice")
    for _, name in ipairs({ "KeepFlag", "GateBanner" }) do
        for _, id in ipairs(scene.find_entities(name)) do
            local _, yaw = transform.get_rotation(id)
            banner_ids[#banner_ids + 1] = { id = id, yaw = yaw }
        end
    end
    wheel_ids = { ring = scene.find_entity("Wheel"), plaque = scene.find_entity("WheelPlaque"),
                  title = scene.find_entity("WheelTitle"), text = scene.find_entity("WheelText"),
                  costs = scene.find_entities("ChoiceCost") }
    for _, id in ipairs(scene.find_entities("Torch")) do torch_ids[#torch_ids + 1] = { light = id } end
    local flames, glows = scene.find_entities("TorchFlame"), scene.find_entities("TorchGlow")
    for index, torch in ipairs(torch_ids) do
        torch.flame = flames[index] or 0
        torch.glow = glows[index] or 0
    end
    for _, name in ipairs({ "gold", "lives", "wave", "message", "call", "speed", "pause", "resume", "rain", "shield",
                            "rain_label", "shield_label" }) do
        hud[name] = 0
    end
    hud.gold = ui.find("GoldText")
    hud.lives = ui.find("LivesText")
    hud.wave = ui.find("WaveText")
    hud.message = ui.find("MessageText")
    hud.call = ui.find("CallButton")
    hud.rain = ui.find("RainButton")
    hud.shield = ui.find("ShieldButton")
    hud.rain_label, hud.shield_label = hud.rain, hud.shield
    hud.speed = ui.find("SpeedButton")
    hud.pause = ui.find("PauseWindow")
    hud.resume = ui.find("ResumeButton")
    show(hud.pause, false)
    show(hud.message, false)
    show(cursor_id, false)
    show(ring_id, false)
    find_pads()
    find_route()
    view.x, view.z = (map_width - 1) * 0.5, (map_depth - 1) * 0.5
    view.distance = camera_distance
    place_camera()
    -- What the campaign unlocked: the spells, more gold, more lives (`tower_done_NN`, `tower_stars_NN` settings).
    spells.rain = { unlocked = settings.get("tower_done_02", false) == true, clock = 0.0,
                    cooldown = rain_cooldown * (tier("rain", 1) and 0.8 or 1.0),
                    damage = rain_damage * (tier("rain", 2) and 1.3 or 1.0),
                    radius = rain_radius * (tier("rain", 3) and 1.25 or 1.0), title = "Rain of arrows", casts = 0 }
    spells.shield = { unlocked = settings.get("tower_done_04", false) == true, clock = 0.0,
                      cooldown = shield_cooldown * (tier("shield", 1) and 0.8 or 1.0),
                      time = shield_time + (tier("shield", 2) and 2.0 or 0.0),
                      radius = shield_radius * (tier("shield", 3) and 1.25 or 1.0), title = "Shield wall", casts = 0 }
    show(hud.rain, spells.rain.unlocked)
    show(hud.shield, spells.shield.unlocked)
    local bonus_gold = settings.get("tower_done_03", false) == true and treasury_gold or 0
    local all_stars = true
    for number = 1, 5 do
        if settings.get(string.format("tower_stars_%02d", number), 0) < 3 then all_stars = false end
    end
    local chest = tier("treasury", 1) and 40 or 0
    local masons = tier("treasury", 3) and 2 or 0
    gold, lives_left = start_gold + bonus_gold + chest, lives + masons + (all_stars and walls_lives or 0)
    gamestate.set("tower_start_gold", gold)
    wave_count = #waves
    countdown = first_wave_delay
    hours, hours_goal = start_hour, start_hour
    if sky_id ~= 0 then
        entity.set_day_night_enabled(sky_id, false)
        entity.set_time_of_day(sky_id, hours)
    end
    gamestate.set("tower_won", false)
    gamestate.set("tower_lost", false)
    gamestate.set("tower_map", level_number)
    gamestate.set("tower_scene", scene_path)
    message(level_name .. "\n" .. wave_count .. " waves", 2.5)
    mirror()
end

-- Day and night -------------------------------------------------------------------------------------------------------

-- The banners of the keep and of the gate wave in the wind.
local function wave_banners()
    for index, banner in ipairs(banner_ids) do
        local sway = math.sin(clock * 2.3 + index * 1.7)
        transform.set_rotation(banner.id, 0.04 * sway, banner.yaw + 0.22 * sway, 0.03 * math.sin(clock * 3.1 + index))
    end
end

local function update_sky(dt)
    if sky_id == 0 then return end
    if math.abs(hours_goal - hours) > 0.001 then
        local step = math.max(0.25, math.abs(hours_goal - hours)) * dt * 0.35
        if hours < hours_goal then hours = math.min(hours_goal, hours + step) else hours = math.max(hours_goal, hours - step) end
        entity.set_time_of_day(sky_id, hours)
    end
    local night = hours >= 19.0 or hours < 6.0
    if night ~= night_lit then
        night_lit = night
        for _, torch in ipairs(torch_ids) do
            entity.set_light_enabled(torch.light, night)
            if torch.glow ~= 0 then entity.set_light_enabled(torch.glow, night) end
            show(torch.flame, night)
        end
    end
end

-- Waves ---------------------------------------------------------------------------------------------------------------

local function parse_wave(text)
    local queue = {}
    for group in string.gmatch(text or "", "[^;]+") do
        local kind, count, gap, letter = string.match(group, "^%s*([%a_]+)%s+(%d+)%s+([%d%.]+)%s*(%a?)%s*$")
        if kind ~= nil and stats[kind] ~= nil and stats[kind].hp ~= nil then
            for i = 1, tonumber(count) do
                local road = letter == "b" and 2 or (letter == "a" and 1 or (#queue % 2) + 1)
                if routes[road] == nil then road = 1 end
                queue[#queue + 1] = { kind = kind, road = road,
                                      gap = (i == 1 and #queue > 0) and tonumber(gap) * 2 or tonumber(gap) }
            end
        else
            log.warn("Tower defence: wave group '" .. group .. "' is not 'kind count gap [road]'.")
        end
    end
    return queue
end

local function start_wave()
    wave = wave + 1
    spawning = parse_wave(waves[wave])
    spawn_clock = 0.0
    countdown = 0.0
    if night_from > 0 and wave >= night_from then
        hours_goal = night_hour
    else
        hours_goal = start_hour + (wave - 1) * 0.6
    end
    play("sounds/game/horn.wav")
    message("Wave " .. wave .. " / " .. wave_count .. (night_from > 0 and wave == night_from and "\nNightfall" or ""), 1.8)
end

local function call_early()
    if ending ~= nil or spawning ~= nil or wave >= wave_count or route == nil then return end
    local bonus = math.floor(countdown * early_bonus)
    if bonus > 0 then
        gold = gold + bonus
        earned = earned + bonus
        bonus_total = bonus_total + bonus
        play("sounds/game/coins.wav")
    end
    start_wave()
    if bonus > 0 then message("Wave " .. wave .. " / " .. wave_count .. "\n+" .. bonus .. " gold", 1.8) end
end

local function spawn_enemy(kind, road)
    local base = stats[kind]
    local x, y, z = along(0.0, road)
    local id = scene.spawn_prefab("prefabs/enemy_" .. kind .. ".owlprefab", billboard_at(x, y, z))
    if id == 0 then return end
    local _, size = transform.get_scale(id)
    local lift = size * FOOT_SHARE
    local bar = scene.spawn_prefab("prefabs/health_bar.owlprefab", x, y + 1.0, z)
    show(bar, false)
    local shadow = scene.spawn_prefab("prefabs/shadow.owlprefab", x, y + 0.03, z)
    if shadow ~= 0 then
        transform.set_rotation(shadow, -math.pi / 2, 0.0, 0.0)
        transform.set_scale(shadow, SHADOW_SIZE[kind], SHADOW_SIZE[kind], 1.0)
    end
    local hp = base.hp * (1.0 + hp_growth * (wave - 1))
    spawned = spawned + 1
    if not foes_seen[kind] then
        foes_seen[kind] = true
        local foe = FOES[kind]
        if foe ~= nil and level_number > 1 and kind ~= "goblin" and kind ~= "grunt" then
            message(foe[1] .. "!\n" .. foe[2], 3.5)
        end
        gamestate.set("tower_seen_" .. kind, true)
    end
    if road == 2 then gamestate.set("tower_spawned_b", (gamestate.get("tower_spawned_b", 0)) + 1) end
    local side = ((spawned * 37) % 7) / 6.0 - 0.5
    enemies[#enemies + 1] = {
        id = id, bar = bar, shadow = shadow, kind = kind, hp = hp, max = hp, speed = base.speed, gold = base.gold,
        arrow = base.arrow, regen = base.regen or 0.0, road = road or 1, distance = 0.0, side = side * 0.28,
        slow = 0.0, slow_factor = 1.0, frozen = 0.0, frost_hits = 0, burn = 0.0, x = x, y = y, z = z, dx = 1.0,
        dz = 0.0, walk = spawned * 0.37, frame = -1, lift = lift, lives = base.lives or 1, boss = base.boss == true,
        ward = 0.0, ward_clock = base.ward and 1.5 or nil, ward_give = base.ward, blast = base.blast,
    }
end

local function update_waves(dt)
    if route == nil then return end
    if spawning ~= nil then
        spawn_clock = spawn_clock - dt
        while spawning ~= nil and spawn_clock <= 0.0 do
            local next_one = table.remove(spawning, 1)
            if next_one == nil then
                spawning = nil
                if wave < wave_count then countdown = wave_delay end
            else
                spawn_enemy(next_one.kind, next_one.road)
                spawn_clock = spawn_clock + (spawning[1] and spawning[1].gap or 0.0)
                if #spawning == 0 then
                    spawning = nil
                    if wave < wave_count then countdown = wave_delay end
                end
            end
        end
    elseif wave < wave_count then
        countdown = countdown - dt
        if countdown <= 0.0 then start_wave() end
    end
end

-- Effects -------------------------------------------------------------------------------------------------------------

local function spawn_puff(x, y, z, r, g, b, size, duration)
    local id = scene.spawn_prefab("prefabs/puff.owlprefab", billboard_at(x, y, z, 0.2))
    if id == 0 then return end
    entity.set_tint(id, r, g, b, 0.85)
    effects[#effects + 1] = { id = id, kind = "puff", t = 0.0, duration = duration, size = size, r = r, g = g, b = b }
end

local function spawn_ring(x, y, z, radius, r, g, b)
    local id = scene.spawn_prefab("prefabs/shock_ring.owlprefab", x, y + 0.05, z)
    if id == 0 then return end
    transform.set_rotation(id, -math.pi / 2, 0.0, 0.0)
    effects[#effects + 1] = { id = id, kind = "ring", t = 0.0, duration = 0.4, size = radius * 2.0, r = r, g = g, b = b }
end

local function spawn_fire(x, y, z, radius)
    local id = scene.spawn_prefab("prefabs/fire.owlprefab", billboard_at(x, y, z, 0.28))
    if id == 0 then return end
    effects[#effects + 1] = { id = id, kind = "fire", t = 0.0, duration = fire_time, x = x, y = y, z = z,
                              radius = radius }
end

-- Enemies -------------------------------------------------------------------------------------------------------------

-- Sparks: a pool of small glowing sprites for trails and impacts, reused rather than spawned each time.
local function emit(x, y, z, count, speed, r, g, b, size, life, rise)
    for _ = 1, count do
        local spark = nil
        for _, candidate in ipairs(sparks) do
            if not candidate.active then
                spark = candidate
                break
            end
        end
        if spark == nil and #sparks < MAX_SPARKS then
            local id = scene.spawn_prefab("prefabs/spark.owlprefab", x, y, z)
            if id ~= 0 then
                spark = { id = id }
                sparks[#sparks + 1] = spark
            end
        end
        if spark == nil then return end
        local a, e = math.random() * 2.0 * math.pi, math.random() * 0.8 + 0.2
        spark.active, spark.t, spark.life, spark.size = true, 0.0, life * (0.7 + math.random() * 0.6), size
        spark.x, spark.y, spark.z = x, y, z
        spark.vx, spark.vy, spark.vz = math.cos(a) * speed * e, (rise or 0.0) + speed * (math.random() - 0.3), math.sin(a) * speed * e
        spark.r, spark.g, spark.b = r, g, b
        show(spark.id, true)
        entity.set_tint(spark.id, r, g, b, 1.0)
    end
end

local function update_sparks(dt)
    for _, spark in ipairs(sparks) do
        if spark.active then
            spark.t = spark.t + dt
            local t = spark.t / spark.life
            if t >= 1.0 then
                spark.active = false
                show(spark.id, false)
            else
                spark.vy = spark.vy - 3.0 * dt
                spark.x, spark.y, spark.z = spark.x + spark.vx * dt, spark.y + spark.vy * dt, spark.z + spark.vz * dt
                transform.set_position(spark.id, spark.x, spark.y, spark.z)
                local size = spark.size * (1.0 - 0.6 * t)
                transform.set_scale(spark.id, size, size, 1.0)
                entity.set_tint(spark.id, spark.r, spark.g, spark.b, 1.0 - t)
            end
        end
    end
end

local function remove_enemy(index, dying)
    local foe = enemies[index]
    if foe.bar ~= 0 then scene.destroy_entity(foe.bar) end
    if foe.shadow ~= 0 then scene.destroy_entity(foe.shadow) end
    if dying then
        corpses[#corpses + 1] = { id = foe.id, direction = foe.frame % DIRECTIONS, t = 0.0 }
    else
        scene.destroy_entity(foe.id)
    end
    foe.dead = true
    table.remove(enemies, index)
end

local blast

-- Damage to an enemy: a ward takes it first unless it burns (`through_ward`); a kill pays and may blow a keg.
local function hurt(foe, amount, through_ward)
    if foe.dead then return end
    if foe.vulnerable then amount = amount * aura_damage end
    if foe.ward > 0.0 and not through_ward then
        local taken = math.min(foe.ward, amount)
        foe.ward = foe.ward - taken
        amount = amount - taken
    end
    foe.hp = foe.hp - amount
    if foe.hp <= 0 then
        gold = gold + foe.gold
        earned = earned + foe.gold
        kills = kills + 1
        play("sounds/game/pop.wav")
        emit(foe.x, foe.y + 0.35, foe.z, 6, 1.4, 0.75, 0.15, 0.1, 0.12, 0.45, 1.2)
        for index, other in ipairs(enemies) do
            if other == foe then
                remove_enemy(index, true)
                break
            end
        end
        if foe.blast ~= nil then blast(foe) end
        return
    end
    foe.flash = 0.09
    if foe.bar ~= 0 then
        local share = foe.hp / foe.max
        show(foe.bar, true)
        transform.set_scale(foe.bar, 0.5 * share, 0.07, 1.0)
        entity.set_tint(foe.bar, 1.0 - share * 0.8, 0.25 + share * 0.65, 0.2, 1.0)
    end
end

-- A sapper killed on the road: its keg bursts and hurts the Horde around it.
function blast(foe)
    play("sounds/game/cannon.wav")
    spawn_ring(foe.x, foe.y, foe.z, sapper_blast_radius, 1.0, 0.6, 0.2)
    emit(foe.x, foe.y + 0.3, foe.z, 14, 2.2, 1.0, 0.55, 0.15, 0.14, 0.5, 1.4)
    local hit = {}
    for _, other in ipairs(enemies) do
        local dx, dz = other.x - foe.x, other.z - foe.z
        if dx * dx + dz * dz <= sapper_blast_radius * sapper_blast_radius then hit[#hit + 1] = other end
    end
    for _, other in ipairs(hit) do hurt(other, foe.blast) end
    gamestate.set("tower_blasts", gamestate.get("tower_blasts", 0) + 1)
end

-- An ogre mage wards the Horde around it (itself included) every few seconds.
local function cast_ward(mage)
    for _, other in ipairs(enemies) do
        local dx, dz = other.x - mage.x, other.z - mage.z
        if dx * dx + dz * dz <= mage_ward_radius * mage_ward_radius then
            other.ward = math.max(other.ward, mage.ward_give)
        end
    end
    spawn_ring(mage.x, mage.y, mage.z, mage_ward_radius, 0.55, 0.75, 1.0)
    gamestate.set("tower_wards", gamestate.get("tower_wards", 0) + 1)
end

-- The speed factor of an enemy near the chieftain (its war cry).
local function rally_of(foe)
    for _, other in ipairs(enemies) do
        if other.boss and other ~= foe then
            local dx, dz = other.x - foe.x, other.z - foe.z
            if dx * dx + dz * dz <= chieftain_rally_radius * chieftain_rally_radius then return chieftain_rally end
        end
    end
    return 1.0
end

-- The fallen: the three frames of the fall, then a fade.
local function update_corpses(dt)
    local index = 1
    while index <= #corpses do
        local corpse = corpses[index]
        corpse.t = corpse.t + dt
        local row = DEATH_ROW + math.min(2, math.floor(corpse.t / 0.12))
        isometric.set_sprite_frame(corpse.id, row * DIRECTIONS + corpse.direction)
        if corpse.t > 0.45 then entity.set_tint(corpse.id, 1.0, 1.0, 1.0, math.max(0.0, 1.0 - (corpse.t - 0.45) / 0.5)) end
        if corpse.t > 0.95 then
            scene.destroy_entity(corpse.id)
            table.remove(corpses, index)
        else
            index = index + 1
        end
    end
end

-- The column of a sheet for a heading on the ground plane: 0 toward the camera, then clockwise seen from above.
local function direction_of(dx, dz)
    local right = dx * basis.right[1] + dz * basis.right[3]
    local toward = dx * basis.toward[1] + dz * basis.toward[2]
    local angle = math.atan(right, toward)
    return math.floor(angle / (2.0 * math.pi / DIRECTIONS) + 0.5) % DIRECTIONS
end

local function update_enemies(dt)
    local index = 1
    while index <= #enemies do
        local foe = enemies[index]
        local speed = foe.speed * rally_of(foe)
        if foe.ward_clock ~= nil then
            foe.ward_clock = foe.ward_clock - dt
            if foe.ward_clock <= 0.0 then
                foe.ward_clock = mage_ward_every
                cast_ward(foe)
            end
        end
        if foe.frozen > 0.0 then
            foe.frozen = foe.frozen - dt
            speed = 0.0
        elseif foe.slow > 0.0 then
            foe.slow = foe.slow - dt
            speed = speed * foe.slow_factor
        end
        if foe.burn > 0.0 then
            foe.burn = foe.burn - dt
            hurt(foe, fire_damage * dt, true)
        elseif foe.regen > 0.0 and foe.hp < foe.max then
            foe.hp = math.min(foe.max, foe.hp + foe.regen * dt)
        end
        if not foe.dead then
            local step = speed * dt
            foe.distance = foe.distance + step
            travel = travel + step
            local length = routes[foe.road].total
            if foe.distance >= length then
                -- At the gate of the keep: two blows, then the life is lost.
                foe.distance = length
                foe.attack = (foe.attack or 0.0) + dt
                local row = ATTACK_ROW + (math.floor(foe.attack / 0.18) % 2)
                isometric.set_sprite_frame(foe.id, row * DIRECTIONS + direction_of(foe.dx, foe.dz))
                if foe.attack >= 0.72 then
                    lives_left = lives_left - foe.lives
                    leaks = leaks + 1
                    play("sounds/game/leak.wav")
                    emit(foe.x, foe.y + 0.5, foe.z, 5, 1.2, 0.9, 0.85, 0.7, 0.14, 0.4, 0.6)
                    message("The keep is hit! " .. math.max(0, lives_left) .. " lives left", 1.5)
                    remove_enemy(index, false)
                else
                    index = index + 1
                end
            else
                local x, y, z, dx, dz = along(foe.distance, foe.road)
                -- Walk beside the middle of the road, on the right of the heading.
                x, z = x - dz * foe.side, z + dx * foe.side
                foe.x, foe.y, foe.z, foe.dx, foe.dz = x, y, z, dx, dz
                foe.walk = foe.walk + step * 3.2
                local frame = (math.floor(foe.walk) % 4) * DIRECTIONS + direction_of(dx, dz)
                if speed == 0.0 then frame = direction_of(dx, dz) end
                if frame ~= foe.frame then
                    foe.frame = frame
                    isometric.set_sprite_frame(foe.id, frame)
                end
                transform.set_position(foe.id, billboard_at(x, y, z, foe.lift))
                if foe.bar ~= 0 then transform.set_position(foe.bar, billboard_at(x, y, z, BAR_HEIGHT[foe.kind])) end
                if foe.shadow ~= 0 then transform.set_position(foe.shadow, x, y + 0.03, z) end
                local tint = foe.frozen > 0.0 and "frozen" or (foe.slow > 0.0 and "slow" or (foe.burn > 0.0 and "burn"
                        or (foe.ward > 0.0 and "ward" or "plain")))
                if (foe.flash or 0.0) > 0.0 then
                    foe.flash = foe.flash - dt
                    tint = "flash"
                end
                if foe.tinted ~= tint then
                    foe.tinted = tint
                    if tint == "flash" then entity.set_tint(foe.id, 1.0, 0.4, 0.35, 1.0)
                    elseif tint == "frozen" then entity.set_tint(foe.id, 0.75, 0.95, 1.0, 1.0)
                    elseif tint == "slow" then entity.set_tint(foe.id, 0.65, 0.82, 1.0, 1.0)
                    elseif tint == "burn" then entity.set_tint(foe.id, 1.0, 0.7, 0.5, 1.0)
                    elseif tint == "ward" then entity.set_tint(foe.id, 0.8, 0.75, 1.0, 1.0)
                    else entity.set_tint(foe.id, 1.0, 1.0, 1.0, 1.0) end
                end
                index = index + 1
            end
        end
    end
end

-- Towers and projectiles ----------------------------------------------------------------------------------------------

local function close_ring()
    ring = nil
    hover = 0
    show(ring_id, false)
    for _, id in ipairs(choice_ids) do show(id, false) end
    for _, id in ipairs(wheel_ids.costs or {}) do show(id, false) end
    for _, id in ipairs({ wheel_ids.ring, wheel_ids.plaque, wheel_ids.title, wheel_ids.text }) do show(id, false) end
end

local function in_range(tower, foe, range)
    local dx, dz = foe.x - tower.x, foe.z - tower.z
    return dx * dx + dz * dz <= range * range
end

-- The enemies in range, the one nearest the keep first.
local function targets_of(tower, count)
    local range = tower.stats.range
    local found = {}
    for _, foe in ipairs(enemies) do
        if in_range(tower, foe, range) then found[#found + 1] = foe end
    end
    table.sort(found, function(a, b) return a.distance > b.distance end)
    local out = {}
    for i = 1, math.min(count, #found) do out[i] = found[i] end
    return out
end

local function fire(tower, foe)
    local kind = tower.kind
    local key = (kind == "cannon" and tower.power == "fire") and "fire" or kind
    local sy = tower.y + SHOT_HEIGHT[tower.level]
    local id = scene.spawn_prefab(PROJECTILE[key], tower.x, sy, tower.z)
    if id == 0 then return end
    local shot = { id = id, kind = kind, power = tower.power, tower = tower, x = tower.x, y = sy, z = tower.z,
                   target = foe, t = 0.0, sx = tower.x, sy = sy, sz = tower.z, tx = foe.x, ty = foe.y, tz = foe.z }
    if kind == "cannon" then
        -- Aim where the enemy will be when the shell lands.
        local factor = foe.frozen > 0.0 and 0.0 or (foe.slow > 0.0 and foe.slow_factor or 1.0)
        local lead = foe.distance + foe.speed * factor * ball_flight
        shot.tx, shot.ty, shot.tz = along(math.min(lead, routes[foe.road].total), foe.road)
    end
    projectiles[#projectiles + 1] = shot
    if clock - (last_shot_sound[kind] or -1.0) > 0.12 then
        last_shot_sound[kind] = clock
        play(SHOT_SOUND[kind])
    end
end

local function update_towers(dt)
    for _, foe in ipairs(enemies) do foe.vulnerable = false end
    for _, tower in ipairs(towers) do
        if tower.power == "aura" then
            for _, foe in ipairs(enemies) do
                if in_range(tower, foe, tower.stats.range) then foe.vulnerable = true end
            end
        end
    end
    for _, tower in ipairs(towers) do
        aim(tower, dt)
        tower.cooldown = tower.cooldown - dt
        if tower.cooldown <= 0.0 then
            local found = targets_of(tower, tower.power == "double" and 2 or 1)
            if #found > 0 then
                for _, foe in ipairs(found) do fire(tower, foe) end
                tower.aim_x, tower.aim_z = found[1].x, found[1].z
                tower.kick = 0.15
                tower.cooldown = tower.stats.reload
            else
                tower.cooldown = 0.0
            end
        end
    end
end

local function land(shot)
    local s = shot.tower.stats
    if shot.kind == "cannon" then
        play("sounds/game/boom.wav")
        spawn_puff(shot.tx, shot.ty, shot.tz, 1.0, 0.7, 0.35, 1.6, 0.4)
        emit(shot.tx, shot.ty + 0.1, shot.tz, 10, 2.2, 0.45, 0.36, 0.26, 0.11, 0.6, 1.5)
        emit(shot.tx, shot.ty + 0.15, shot.tz, 6, 1.6, 1.0, 0.7, 0.25, 0.1, 0.35, 1.0)
        local radius = s.splash
        local hit = {}
        for _, foe in ipairs(enemies) do
            local dx, dz = foe.x - shot.tx, foe.z - shot.tz
            if dx * dx + dz * dz <= radius * radius then hit[#hit + 1] = foe end
        end
        for _, foe in ipairs(hit) do
            if shot.power == "shock" then foe.distance = math.max(0.0, foe.distance - shock_push) end
            hurt(foe, s.damage)
        end
        if shot.power == "fire" then spawn_fire(shot.tx, shot.ty, shot.tz, radius * 0.8) end
        if shot.power == "shock" then spawn_ring(shot.tx, shot.ty, shot.tz, radius, 1.0, 0.85, 0.6) end
        return
    end
    local foe = shot.target
    if foe == nil or foe.dead then return end
    if shot.kind == "archer" then
        play("sounds/game/hit.wav")
        emit(foe.x, foe.y + 0.35, foe.z, 3, 1.0, 0.95, 0.85, 0.6, 0.07, 0.25, 0.6)
        if shot.power == "pierce" then
            local behind = {}
            for _, other in ipairs(enemies) do
                if other ~= foe and other.distance < foe.distance and other.distance > foe.distance - 1.4 then
                    behind[#behind + 1] = other
                end
            end
            table.sort(behind, function(a, b) return a.distance > b.distance end)
            for i = 1, math.min(2, #behind) do hurt(behind[i], s.damage * behind[i].arrow) end
        end
        hurt(foe, s.damage * foe.arrow)
    else
        foe.slow = s.time
        foe.slow_factor = foe.boss and (1.0 - (1.0 - s.slow) * 0.5) or s.slow
        if shot.power == "freeze" and not foe.boss then
            foe.frost_hits = foe.frost_hits + 1
            if foe.frost_hits % 3 == 0 then foe.frozen = freeze_time end
        end
        spawn_puff(foe.x, foe.y + 0.3, foe.z, 0.6, 0.85, 1.0, 0.7, 0.3)
        emit(foe.x, foe.y + 0.35, foe.z, 5, 1.2, 0.75, 0.92, 1.0, 0.08, 0.4, 0.4)
        hurt(foe, s.damage)
    end
end

-- The angle on the screen of a world direction, to turn an arrow along its flight.
local function screen_roll(dx, dy, dz)
    local r, u = basis.right, basis.up
    return math.atan(dx * u[1] + dy * u[2] + dz * u[3], dx * r[1] + dy * r[2] + dz * r[3])
end

local function update_projectiles(dt)
    local index = 1
    while index <= #projectiles do
        local shot = projectiles[index]
        local arrived = false
        if shot.kind == "cannon" then
            shot.t = shot.t + dt / ball_flight
            local t = math.min(1.0, shot.t)
            shot.x = shot.sx + (shot.tx - shot.sx) * t
            shot.z = shot.sz + (shot.tz - shot.sz) * t
            shot.y = shot.sy + (shot.ty + 0.2 - shot.sy) * t + 2.5 * t * (1.0 - t)
            arrived = t >= 1.0
        else
            if shot.target ~= nil and not shot.target.dead then
                shot.tx, shot.ty, shot.tz = shot.target.x, shot.target.y + 0.3, shot.target.z
            end
            local dx, dy, dz = shot.tx - shot.x, shot.ty - shot.y, shot.tz - shot.z
            local distance = math.sqrt(dx * dx + dy * dy + dz * dz)
            local step = arrow_speed * dt
            if distance <= step or distance < 0.05 then
                arrived = true
            else
                shot.x, shot.y, shot.z = shot.x + dx / distance * step, shot.y + dy / distance * step,
                        shot.z + dz / distance * step
                if shot.kind == "archer" then transform.set_rotation(shot.id, 0.0, 0.0, screen_roll(dx, dy, dz)) end
            end
        end
        shot.trail = (shot.trail or 0.0) - dt
        if shot.trail <= 0.0 and not arrived then
            shot.trail = 0.035
            if shot.power == "fire" then
                emit(shot.x, shot.y, shot.z, 1, 0.25, 1.0, 0.55, 0.15, 0.16, 0.35, 0.3)
            elseif shot.kind == "frost" then
                emit(shot.x, shot.y, shot.z, 1, 0.15, 0.7, 0.9, 1.0, 0.09, 0.3, 0.0)
            elseif shot.kind == "cannon" then
                emit(shot.x, shot.y, shot.z, 1, 0.1, 0.55, 0.55, 0.55, 0.12, 0.4, 0.2)
            else
                emit(shot.x, shot.y, shot.z, 1, 0.05, 0.95, 0.92, 0.8, 0.05, 0.18, 0.0)
            end
        end
        if arrived then
            land(shot)
            scene.destroy_entity(shot.id)
            table.remove(projectiles, index)
        else
            transform.set_position(shot.id, shot.x, shot.y, shot.z)
            index = index + 1
        end
    end
end

local function update_effects(dt)
    local index = 1
    while index <= #effects do
        local effect = effects[index]
        effect.t = effect.t + dt
        local t = math.min(1.0, effect.t / effect.duration)
        if effect.kind == "puff" then
            local size = effect.size * (0.5 + 0.7 * t)
            transform.set_scale(effect.id, size, size, 1.0)
            entity.set_tint(effect.id, effect.r, effect.g, effect.b, 0.85 * (1.0 - t))
        elseif effect.kind == "ring" then
            local size = effect.size * (0.3 + 0.9 * t)
            transform.set_scale(effect.id, size, size, 1.0)
            entity.set_tint(effect.id, effect.r, effect.g, effect.b, 0.9 * (1.0 - t))
        elseif effect.kind == "fire" then
            for _, foe in ipairs(enemies) do
                local dx, dz = foe.x - effect.x, foe.z - effect.z
                if dx * dx + dz * dz <= effect.radius * effect.radius then foe.burn = 0.5 end
            end
            local fade = t > 0.75 and (1.0 - t) * 4.0 or 1.0
            entity.set_tint(effect.id, 1.0, 1.0, 1.0, fade)
        end
        if t >= 1.0 then
            scene.destroy_entity(effect.id)
            table.remove(effects, index)
        else
            index = index + 1
        end
    end
end

-- Spells ------------------------------------------------------------------------------------------------------------

-- The enemy nearest the keep: the spells fall on it and around it.
local function front_enemy()
    local best, share = nil, -1.0
    for _, foe in ipairs(enemies) do
        local progress = foe.distance / math.max(0.01, routes[foe.road].total)
        if progress > share then best, share = foe, progress end
    end
    return best
end

local function cast(name)
    local spell = spells[name]
    if spell == nil or not spell.unlocked or spell.clock > 0.0 or ending ~= nil or paused then return end
    local target = front_enemy()
    if target == nil then
        message("No enemy to strike", 1.0)
        return
    end
    spell.clock = spell.cooldown
    spell.casts = spell.casts + 1
    local hit = {}
    for _, foe in ipairs(enemies) do
        local dx, dz = foe.x - target.x, foe.z - target.z
        if dx * dx + dz * dz <= spell.radius * spell.radius then hit[#hit + 1] = foe end
    end
    if name == "rain" then
        play("sounds/game/arrow.wav")
        for k = 1, 14 do
            local a, r = k * 2.4, (k % 5) * spell.radius * 0.2
            emit(target.x + math.cos(a) * r, target.y + 2.2, target.z + math.sin(a) * r, 1, 0.2, 0.95, 0.9, 0.75, 0.1, 0.5, -4.0)
        end
        spawn_ring(target.x, target.y, target.z, spell.radius, 0.95, 0.9, 0.7)
        for _, foe in ipairs(hit) do hurt(foe, spell.damage) end
    else
        play("sounds/game/build.wav")
        spawn_ring(target.x, target.y, target.z, spell.radius, 1.0, 0.85, 0.3)
        emit(target.x, target.y + 0.4, target.z, 12, 1.6, 1.0, 0.85, 0.35, 0.12, 0.6, 0.8)
        for _, foe in ipairs(hit) do foe.frozen = math.max(foe.frozen, foe.boss and spell.time * 0.5 or spell.time) end
    end
    gamestate.set("tower_" .. name .. "_casts", spell.casts)
end

function on_rain_clicked()
    cast("rain")
end

function on_shield_clicked()
    cast("shield")
end

local function update_spells(dt)
    for name, spell in pairs(spells) do
        spell.clock = math.max(0.0, spell.clock - dt)
        local id = hud[name]
        if id ~= 0 and spell.unlocked then
            ui.set_button_enabled(id, spell.clock <= 0.0 and #enemies > 0 and ending == nil)
            set_text(name .. "_label", spell.clock > 0.0 and string.format("%s %d s", spell.title, math.ceil(spell.clock))
                    or spell.title)
        end
    end
end

-- The build wheel -----------------------------------------------------------------------------------------------------

-- Pixels of the wheel around its centre, on the screen: the ring, the badges on it, the scroll with the info below.
local WHEEL = { ring = 196, badge = 58, radius = 64, cost = 19, cost_drop = 40, plaque_w = 330, plaque_h = 113,
                plaque_drop = 168, title = 22, text = 15, line = 18, margin = 16 }
-- Angles of the badges (degrees, counter-clockwise from the right) for 1 to 4 choices.
local WHEEL_ANGLES = { { 90 }, { 150, 30 }, { 150, 30, 270 }, { 135, 45, 315, 225 } }
local WRAP = 38   -- characters of a line of the info

-- The pixel of a world point and the world size of a pixel there, through the camera.
local function project(x, y, z)
    local width, height = scene.get_viewport_size()
    if width == nil or width <= 0 then width, height = 1280, 720 end
    local dx, dy, dz = x - view.eye[1], y - view.eye[2], z - view.eye[3]
    local f, r, u = basis.forward, basis.right, basis.up
    local depth = math.max(0.1, dx * f[1] + dy * f[2] + dz * f[3])
    local half = math.tan(camera_fov * 0.5)
    local aspect = width / height
    local sx = (dx * r[1] + dy * r[2] + dz * r[3]) / (depth * half * aspect)
    local sy = (dx * u[1] + dy * u[2] + dz * u[3]) / (depth * half)
    return (sx + 1.0) * 0.5 * width, (1.0 - sy) * 0.5 * height, 2.0 * depth * half / height, width, height
end

-- A world point moved by pixels on the screen (right, up) at the depth of a point.
local function screen_offset(x, y, z, px, py, unit, behind)
    local r, u, f = basis.right, basis.up, basis.forward
    behind = behind or 0.0
    return x + (r[1] * px + u[1] * py) * unit + f[1] * behind, y + (r[2] * px + u[2] * py) * unit + f[2] * behind,
            z + (r[3] * px + u[3] * py) * unit + f[3] * behind
end

local function wrap(text)
    local lines, current = {}, ""
    for word in string.gmatch(text, "%S+") do
        if #current > 0 and #current + 1 + #word > WRAP then
            lines[#lines + 1] = current
            current = word
        else
            current = #current > 0 and (current .. " " .. word) or word
        end
    end
    if #current > 0 then lines[#lines + 1] = current end
    return table.concat(lines, "\n")
end

local function show_range(pad, range)
    if ring_id == 0 or range == nil then
        show(ring_id, false)
        return
    end
    transform.set_position(ring_id, pad.x, pad.y + PAD_TOP + 0.03, pad.z)
    transform.set_scale(ring_id, range * 2.0, range * 2.0, 1.0)
    show(ring_id, true)
end

-- The range a choice of the wheel would give.
local function range_of_choice(pad, choice)
    if choice == nil or choice.kind == "sell" then
        return pad.tower ~= nil and pad.tower.stats.range or nil
    end
    if pad.tower == nil then
        return tower_stats({ kind = choice.kind, level = 1, pad = pad }).range
    end
    local level = choice.kind == "upgrade" and pad.tower.level + 1 or pad.tower.level
    return tower_stats({ kind = pad.tower.kind, level = level, pad = pad }).range
end

local function choice_price(pad, choice)
    if choice.kind == "sell" then return 0 end
    if pad.tower == nil then return stats[choice.kind].cost end
    if choice.kind == "upgrade" then return step_cost(pad.tower.kind, pad.tower.level + 1) end
    return step_cost(pad.tower.kind, 4)
end

local function height_text(pad)
    if pad.level <= 0 then return "" end
    return string.format(" High ground: +%d %% range.", math.floor(height_bonus * pad.level * 100 + 0.5))
end

local function set_world_text(id, text)
    if id ~= 0 and wheel_shown[id] ~= text then
        ui.set_text(id, text)
        wheel_shown[id] = text
    end
end

-- The title and the text of the scroll for the choice under the cursor (or the pad itself).
local function info_of(pad, choice)
    local tower = pad.tower
    if choice == nil then
        if tower == nil then
            return "Build pad", "Choose a tower." .. height_text(pad)
        end
        return NAMES[tower.kind] .. " - level " .. tower.level,
                (tower.power and (POWER_NAMES[tower.power] .. ". ") or "") .. "Choose a badge." .. height_text(pad)
    end
    local kind = choice.kind
    local price = choice_price(pad, choice)
    local short = gold < price and ("Not enough gold: " .. price - gold .. " more.") or nil
    if kind == "sell" then
        return "Sell for " .. refund_of(tower) .. " gold", "Half of what this tower cost."
    elseif kind == "upgrade" then
        return NAMES[tower.kind] .. " to level " .. (tower.level + 1), short or LEVEL_TEXT[tower.kind][tower.level + 1]
    elseif POWER_NAMES[kind] ~= nil then
        if choice.locked then return POWER_NAMES[kind], "Unlocked by winning " .. UNLOCK_NAME .. "." end
        return POWER_NAMES[kind], short or POWER_TEXT[kind]
    end
    return NAMES[kind], (short or BLURBS[kind]) .. height_text(pad)
end

-- Place the wheel beside its pad on the screen, kept inside the view clear of the HUD, at a constant pixel size.
local function place_wheel()
    if ring == nil then return end
    local pad = ring.pad
    local lift = pad.tower ~= nil and SHOT_HEIGHT[pad.tower.level] * 0.6 or 0.6
    local ax, ay, az = pad.x, pad.y + lift, pad.z
    local px, py, unit, width, height = project(ax, ay, az)
    local half_w = math.max(WHEEL.ring, WHEEL.plaque_w) * 0.5
    local low, high = WHEEL.plaque_drop + WHEEL.plaque_h * 0.5, WHEEL.ring * 0.5
    local cx = math.max(WHEEL.margin + half_w, math.min(width - WHEEL.margin - half_w, px))
    local cy = math.max(height * 0.2 + high, math.min(height * 0.89 - low, py))
    local cwx, cwy, cwz = screen_offset(ax, ay, az, cx - px, py - cy, unit)
    ring.unit = unit
    transform.set_position(wheel_ids.ring, screen_offset(cwx, cwy, cwz, 0, 0, unit, 0.2))
    transform.set_scale(wheel_ids.ring, WHEEL.ring * unit, WHEEL.ring * unit, 1.0)
    local angles = WHEEL_ANGLES[#ring.choices] or WHEEL_ANGLES[4]
    for index, choice in ipairs(ring.choices) do
        local a = math.rad(angles[index])
        local bx, by = math.cos(a) * WHEEL.radius, math.sin(a) * WHEEL.radius
        transform.set_position(choice.id, screen_offset(cwx, cwy, cwz, bx, by, unit))
        local size = (index == hover and 1.18 or 1.0) * WHEEL.badge * unit
        transform.set_scale(choice.id, size, size, 1.0)
        local cost = wheel_ids.costs[index]
        if cost ~= nil then
            local price = choice_price(pad, choice)
            local text = choice.kind == "sell" and ("+" .. refund_of(pad.tower)) or (choice.locked and "" or tostring(price))
            set_world_text(cost, text)
            transform.set_position(cost, screen_offset(cwx, cwy, cwz, bx, by - WHEEL.cost_drop, unit, -0.1))
            transform.set_scale(cost, WHEEL.cost * unit, WHEEL.cost * unit, 1.0)
        end
    end
    local title, text = info_of(pad, ring.choices[hover])
    set_world_text(wheel_ids.title, title)
    set_world_text(wheel_ids.text, wrap(text))
    local top = -WHEEL.plaque_drop + WHEEL.plaque_h * 0.5
    transform.set_position(wheel_ids.plaque, screen_offset(cwx, cwy, cwz, 0, -WHEEL.plaque_drop, unit, 0.2))
    transform.set_scale(wheel_ids.plaque, WHEEL.plaque_w * unit, WHEEL.plaque_h * unit, 1.0)
    transform.set_position(wheel_ids.title, screen_offset(cwx, cwy, cwz, 0, top - 32, unit, -0.1))
    transform.set_scale(wheel_ids.title, WHEEL.title * unit, WHEEL.title * unit, 1.0)
    transform.set_position(wheel_ids.text, screen_offset(cwx, cwy, cwz, 0, top - 62, unit, -0.1))
    transform.set_scale(wheel_ids.text, WHEEL.text * unit, WHEEL.text * unit, 1.0)
    show_range(pad, range_of_choice(pad, ring.choices[hover]))
end

local function refresh_badges()
    if ring == nil then return end
    for _, choice in ipairs(ring.choices) do
        local afford = choice.kind == "sell" or (not choice.locked and gold >= choice_price(ring.pad, choice))
        local base = afford and 1.0 or 0.5
        entity.set_tint(choice.id, base, base, base * (afford and 1.0 or 0.9), 1.0)
    end
end

local function open_ring(pad)
    close_ring()
    ring = { pad = pad, choices = {} }
    local kinds = {}
    local tower = pad.tower
    if tower == nil then
        kinds = { { "archer" }, { "cannon" }, { "frost" } }
    elseif tower.level < 3 then
        kinds = { { "upgrade" }, { "sell" } }
    elseif tower.power == nil then
        local powers = POWERS[tower.kind]
        kinds = { { powers[1] }, { powers[2], not unlocked_second_power() }, { "sell" } }
    else
        kinds = { { "sell" } }
    end
    for index, entry in ipairs(kinds) do
        local id = choice_ids[index]
        if id ~= nil then
            local kind, locked = entry[1], entry[2] == true
            isometric.set_sprite_frame(id, locked and ICON.locked or ICON[kind])
            show(id, true)
            show(wheel_ids.costs[index], true)
            ring.choices[#ring.choices + 1] = { kind = kind, id = id, locked = locked }
        end
    end
    for _, id in ipairs({ wheel_ids.ring, wheel_ids.plaque, wheel_ids.title, wheel_ids.text }) do show(id, true) end
    hover = pad_mode and 1 or 0
    play("sounds/ui/click.wav")
    refresh_badges()
    place_wheel()
end

local function tower_prefab(tower)
    local key = tower.power or tostring(tower.level)
    return "prefabs/tower_" .. tower.kind .. "_" .. key .. ".owlprefab"
end

-- Spawn the prefab of a tower at its level and keep its weapons (the meshes tagged `TowerWeapon`), which turn to
-- their target and recoil at each shot.
local function place_tower(tower)
    if tower.id ~= nil and tower.id ~= 0 then scene.destroy_entity(tower.id) end
    local before = {}
    for _, id in ipairs(scene.find_entities("TowerWeapon")) do before[id] = true end
    tower.id = scene.spawn_prefab(tower_prefab(tower), tower.x, tower.y + PAD_TOP, tower.z)
    tower.stats = tower_stats(tower)
    tower.weapons = {}
    for _, id in ipairs(scene.find_entities("TowerWeapon")) do
        if not before[id] then
            local x, y, z = transform.get_position(id)
            local sx = transform.get_scale(id)
            tower.weapons[#tower.weapons + 1] = { id = id, x = x, y = y, z = z, scale = sx }
        end
    end
    tower.kick = 0.0
end

-- Turn the weapons of a tower toward its target, recoiling after a shot (`tower.kick`); the crystals pulse instead.
function aim(tower, dt)
    tower.kick = math.max(0.0, (tower.kick or 0.0) - dt)
    local push = tower.kick / 0.15
    if tower.aim_x ~= nil then
        tower.yaw = math.atan(tower.aim_x - tower.x, tower.aim_z - tower.z) + weapon_yaw
    end
    local yaw = tower.yaw or 0.0
    for _, weapon in ipairs(tower.weapons or {}) do
        if tower.kind == "frost" then
            local size = weapon.scale * (1.0 + 0.25 * push)
            transform.set_scale(weapon.id, size, size, size)
        else
            transform.set_rotation(weapon.id, 0.0, yaw, 0.0)
            local back = 0.12 * push
            transform.set_position(weapon.id, weapon.x - math.sin(yaw) * back, weapon.y, weapon.z - math.cos(yaw) * back)
        end
    end
end

local function build(pad, kind)
    if pad.tower ~= nil then return end
    local price = stats[kind].cost
    if gold < price then
        message("Not enough gold", 1.0)
        return
    end
    gold = gold - price
    local tower = { kind = kind, level = 1, pad = pad, x = pad.x, y = pad.y, z = pad.z, cooldown = 0.2, spent = price }
    place_tower(tower)
    if tower.id == 0 then
        gold = gold + price
        return
    end
    pad.tower = tower
    towers[#towers + 1] = tower
    built = built + 1
    play("sounds/game/build.wav")
    close_ring()
end

local function improve(pad, choice)
    local tower = pad.tower
    if tower == nil then return end
    if choice.locked then
        message("Unlocked by winning " .. UNLOCK_NAME, 1.4)
        return
    end
    local price = choice_price(pad, choice)
    if gold < price then
        message("Not enough gold", 1.0)
        return
    end
    gold = gold - price
    tower.spent = tower.spent + price
    if choice.kind == "upgrade" then
        tower.level = tower.level + 1
    else
        tower.power = choice.kind
    end
    place_tower(tower)
    upgrades = upgrades + 1
    play("sounds/game/build.wav")
    close_ring()
end

local function sell(pad)
    local tower = pad.tower
    if tower == nil then return end
    gold = gold + refund_of(tower)
    scene.destroy_entity(tower.id)
    for index, other in ipairs(towers) do
        if other == tower then
            table.remove(towers, index)
            break
        end
    end
    pad.tower = nil
    play("sounds/game/sell.wav")
    close_ring()
end

local function choose(index)
    if ring == nil then return end
    local choice = ring.choices[index]
    if choice == nil then return end
    if choice.kind == "sell" then
        sell(ring.pad)
    elseif ring.pad.tower == nil then
        build(ring.pad, choice.kind)
    else
        improve(ring.pad, choice)
    end
end

-- The towers standing when the level starts (`start_towers`: "pad kind level [power]", pads counted from 1).
local function raise_start_towers()
    for _, text in ipairs(start_towers or {}) do
        local index, kind, level, power = string.match(text, "^%s*(%d+)%s+(%a+)%s+(%d)%s*(%a*)%s*$")
        local pad = pads[tonumber(index or 0)]
        if pad ~= nil and pad.tower == nil and stats[kind] ~= nil and stats[kind].cost ~= nil then
            local tower = { kind = kind, level = math.max(1, math.min(3, tonumber(level))), pad = pad, x = pad.x,
                            y = pad.y, z = pad.z, cooldown = 0.2, spent = stats[kind].cost }
            if power ~= "" and tower.level == 3 then tower.power = power end
            place_tower(tower)
            pad.tower = tower
            towers[#towers + 1] = tower
        else
            log.warn("Tower defence: start tower '" .. text .. "' is not 'pad kind level [power]' on a free pad.")
        end
    end
end

-- Input ---------------------------------------------------------------------------------------------------------------

local function pad_of_tower(id)
    for _, tower in ipairs(towers) do
        if tower.id == id then return tower.pad end
    end
    return nil
end

local function pad_under_mouse()
    local pad = pad_of_tower(scene.pick_entity(nil, nil, "Tower"))
    if pad == nil then pad = pad_by_id[scene.pick_entity(nil, nil, "Pad")] end
    return pad
end

local function click()
    if ring ~= nil then
        local badge = scene.pick_entity(nil, nil, "BuildChoice")
        for index, choice in ipairs(ring.choices) do
            if choice.id == badge then
                choose(index)
                return
            end
        end
    end
    local pad = pad_under_mouse()
    if pad ~= nil and (ring == nil or ring.pad ~= pad) then
        cursor_pad = pad
        open_ring(pad)
    else
        close_ring()
    end
end

local function hover_badge()
    if ring == nil or pad_mode then return end
    local badge = scene.pick_entity(nil, nil, "BuildChoice")
    local found = 0
    for index, choice in ipairs(ring.choices) do
        if choice.id == badge then found = index end
    end
    if found ~= hover then
        hover = found
        place_wheel()
    end
end

-- The pads from the left of the screen to the right (then from the back to the front).
local function pads_on_screen()
    local order = {}
    for _, pad in ipairs(pads) do order[#order + 1] = pad end
    local r, t = basis.right, basis.toward
    table.sort(order, function(a, b)
        local sa, sb = a.x * r[1] + a.z * r[3], b.x * r[1] + b.z * r[3]
        if math.abs(sa - sb) > 0.01 then return sa < sb end
        return a.x * t[1] + a.z * t[2] < b.x * t[1] + b.z * t[2]
    end)
    return order
end

local function step_cursor(direction)
    local order = pads_on_screen()
    if #order == 0 then return end
    local index = 0
    for i, pad in ipairs(order) do
        if pad == cursor_pad then index = i end
    end
    if index == 0 then
        index = direction > 0 and 1 or #order
    else
        index = (index - 1 + direction) % #order + 1
    end
    cursor_pad = order[index]
end

local function update_cursor()
    local mx, my = scene.get_viewport_mouse()
    if last_mouse_x ~= nil and (mx ~= last_mouse_x or my ~= last_mouse_y) then
        mouse_moved = true
        pad_mode = false
    end
    last_mouse_x, last_mouse_y = mx, my
    local shown_pad = nil
    if pad_mode then
        shown_pad = cursor_pad
    elseif mouse_moved and not ui.is_mouse_over_ui() then
        shown_pad = pad_under_mouse()
    end
    if shown_pad ~= nil and cursor_id ~= 0 then
        transform.set_position(cursor_id, shown_pad.x, shown_pad.y + PAD_TOP + 0.02, shown_pad.z)
        transform.set_scale(cursor_id, 1.05, 1.05, 1.0)
    end
    show(cursor_id, shown_pad ~= nil)
    gamestate.set("tower_hover_pad", shown_pad and shown_pad.index or 0)
end

local function handle_input()
    if input.is_action_just_pressed("fast_forward") then on_speed_clicked() end
    if input.is_action_just_pressed("call_wave") then call_early() end
    if input.is_action_just_pressed("rain") then cast("rain") end
    if input.is_action_just_pressed("shield") then cast("shield") end
    local world_free = not ui.is_world_blocked()
    if world_free and input.is_action_just_pressed("primary") and not ui.is_mouse_over_ui() then
        pad_mode = false
        click()
    end
    if input.is_action_just_pressed("secondary") or input.is_action_just_pressed("cancel") then close_ring() end
    local step = 0
    if input.is_action_just_pressed("next_pad") then step = 1 end
    if input.is_action_just_pressed("prev_pad") then step = -1 end
    if step ~= 0 then
        pad_mode = true
        if ring ~= nil and #ring.choices > 1 then
            hover = (math.max(hover, 1) - 1 + step) % #ring.choices + 1
            place_wheel()
        else
            close_ring()
            step_cursor(step)
        end
    end
    if input.is_action_just_pressed("confirm") and cursor_pad ~= nil then
        pad_mode = true
        if ring ~= nil and hover > 0 then
            choose(hover)
        else
            open_ring(cursor_pad)
        end
    end
    for index = 1, 3 do
        if input.is_action_just_pressed("build_" .. index) and ring ~= nil and ring.choices[index] ~= nil
                and ring.choices[index].kind ~= "sell" then
            choose(index)
        end
    end
    if input.is_action_just_pressed("sell") and ring ~= nil and ring.pad.tower ~= nil then sell(ring.pad) end
    if input.is_action_just_pressed("turn_left") then view.yaw_goal = view.yaw_goal + math.pi / 4 end
    if input.is_action_just_pressed("turn_right") then view.yaw_goal = view.yaw_goal - math.pi / 4 end
end

local function update_camera(real_dt)
    if camera_id == 0 then return end
    local right, down = input.get_action_vector("pan")
    local width, height = scene.get_viewport_size()
    if mouse_moved and width > 0 and edge_pixels > 0 and not ui.is_mouse_over_ui() then
        local mx, my = scene.get_viewport_mouse()
        if mx >= 0 and my >= 0 and mx < width and my < height then
            if mx < edge_pixels then right = -1 elseif mx > width - edge_pixels then right = 1 end
            if my < edge_pixels then down = -1 elseif my > height - edge_pixels then down = 1 end
        end
    end
    local _, wheel = input.get_mouse_wheel()
    local zoom = input.get_action_value("zoom") * real_dt * 12.0 - wheel * 1.2
    view.distance = math.max(zoom_min, math.min(zoom_max, view.distance + zoom))
    if math.abs(view.yaw_goal - view.yaw) > 1e-4 then
        local delta = view.yaw_goal - view.yaw
        local turn = math.min(1.0, real_dt * 8.0)
        view.yaw = math.abs(delta) < 0.002 and view.yaw_goal or view.yaw + delta * turn
    end
    update_basis()
    if pad_mode and cursor_pad ~= nil then
        -- The keyboard and gamepad cursor drags the view along when it leaves the middle of the screen.
        local dx, dz = cursor_pad.x - view.x, cursor_pad.z - view.z
        if math.abs(dx) > 3.5 or math.abs(dz) > 2.5 then
            view.x = view.x + dx * math.min(1.0, real_dt * 3.0)
            view.z = view.z + dz * math.min(1.0, real_dt * 3.0)
        end
    end
    if right ~= 0 or down ~= 0 then
        -- Screen right is the camera right on the ground, screen down is toward the camera.
        local step = pan_speed * real_dt * view.distance / camera_distance
        local r, t = basis.right, basis.toward
        view.x = view.x + (right * r[1] + down * t[1]) * step
        view.z = view.z + (right * r[3] + down * t[2]) * step
    end
    view.x = math.max(0.0, math.min(map_width - 1, view.x))
    view.z = math.max(0.0, math.min(map_depth - 1, view.z))
    place_camera()
end

-- HUD -----------------------------------------------------------------------------------------------------------------

local function update_hud()
    set_text("gold", "Gold " .. gold)
    set_text("lives", "Lives " .. math.max(0, lives_left))
    if spawning ~= nil then
        set_text("wave", "Wave " .. wave .. " / " .. wave_count .. " - incoming")
    elseif wave < wave_count then
        set_text("wave", "Wave " .. wave .. " / " .. wave_count .. " - next in " .. math.ceil(countdown) .. " s")
    else
        set_text("wave", "Wave " .. wave .. " / " .. wave_count .. " - last one")
    end
    local can_call = spawning == nil and wave < wave_count and route ~= nil and ending == nil
    if hud.call ~= 0 then
        ui.set_button_enabled(hud.call, can_call)
        set_text("call", can_call and ("Call wave +" .. math.floor(countdown * early_bonus)) or "Call wave")
    end
    set_text("speed", fast and "Speed x2" or "Speed x1")
    if ring ~= nil then
        refresh_badges()
        place_wheel()
    end
end

-- End of the level ----------------------------------------------------------------------------------------------------

local function stars_for(kept)
    if kept >= 18 then return 3 end
    if kept >= 10 then return 2 end
    return 1
end

local function finish(won)
    ending = won and "won" or "lost"
    end_clock = end_delay
    close_ring()
    time.set_scale(1.0)
    fast = false
    local kept = math.max(0, lives_left)
    gamestate.set(won and "tower_won" or "tower_lost", true)
    gamestate.set("tower_result_won", won)
    gamestate.set("tower_result_kills", kills)
    gamestate.set("tower_result_lives", kept)
    gamestate.set("tower_result_max_lives", lives)
    gamestate.set("tower_result_gold", earned)
    gamestate.set("tower_result_time", math.floor(clock))
    gamestate.set("tower_result_towers", built)
    gamestate.set("tower_result_wave", wave)
    gamestate.set("tower_result_waves", wave_count)
    gamestate.set("tower_result_stars", won and stars_for(kept) or 0)
    gamestate.set("tower_result_level", level_name)
    if won then
        local done = string.format("tower_done_%02d", level_number)
        local best = string.format("tower_best_%02d", level_number)
        local stars = string.format("tower_stars_%02d", level_number)
        local first = not settings.get(done, false)
        gamestate.set("tower_result_unlock", first)
        gamestate.set("tower_result_unlock_text", first and (UNLOCKS[level_number] or "") or "")
        if first then gamestate.set("tower_map_reveal", level_number + 1) end
        settings.set(done, true)
        gamestate.set("tower_done", true)
        if kept > settings.get(best, 0) then settings.set(best, kept) end
        local before = settings.get(stars, 0)
        gamestate.set("tower_result_stars_gained", math.max(0, stars_for(kept) - before))
        if stars_for(kept) > before then settings.set(stars, stars_for(kept)) end
        settings.save()
        message("The keep holds!", end_delay)
    else
        play("sounds/game/defeat.wav")
        message("The keep has fallen", end_delay)
    end
end

local function check_end()
    if ending ~= nil then return end
    if lives_left <= 0 then
        finish(false)
    elseif wave >= wave_count and spawning == nil and #enemies == 0 and route ~= nil then
        finish(true)
    end
end

-- Pause ---------------------------------------------------------------------------------------------------------------

local function set_paused(value)
    paused = value
    time.set_paused(value)
    travel_at_pause = nil
    show(hud.pause, value)
    if value then
        close_ring()
        ui.raise_window(hud.pause)
        if hud.resume ~= 0 then ui.set_focus(hud.resume) end
    end
    mirror()
end

function on_pause_resume()
    set_paused(false)
end

function on_pause_restart()
    set_paused(false)
    ending = "restart"
    time.set_scale(1.0)
    scene.transition_to(scene_path, "wipe_down", 0.3, wipe_color.r, wipe_color.g, wipe_color.b)
end

function on_pause_quit()
    set_paused(false)
    ending = "quit"
    time.set_scale(1.0)
    scene.transition_to("scenes/world_map.owl", "wipe_down", 0.3, wipe_color.r, wipe_color.g, wipe_color.b)
end

function on_menu_clicked()
    if ending == nil then set_paused(not paused) end
end

function on_call_clicked()
    call_early()
end

function on_speed_clicked()
    if ending ~= nil or paused then return end
    fast = not fast
    time.set_scale(fast and 2.0 or 1.0)
end

-- Frame ---------------------------------------------------------------------------------------------------------------

local started = false

function on_update(dt)
    local real_dt = time.real_delta()
    if not started then
        started = true
        raise_start_towers()
    end
    if ending == "restart" or ending == "quit" then return end
    if ending == nil and input.is_action_just_pressed("pause") then set_paused(not paused) end
    if message_clock > 0.0 then
        message_clock = message_clock - real_dt
        if message_clock <= 0.0 then show(hud.message, false) end
    end
    if ending ~= nil then
        end_clock = end_clock - real_dt
        if end_clock <= 0.0 and ending ~= "leaving" then
            local target = ending == "won" and next_scene or "scenes/game_over.owl"
            ending = "leaving"
            scene.transition_to(target, "wipe_down", 0.5, wipe_color.r, wipe_color.g, wipe_color.b)
        end
    end
    -- The world runs on the scene clock: while paused `dt` is 0 and nothing moves (`tower_paused_travel` checks it).
    clock = clock + dt
    if ending == nil and not paused then
        handle_input()
        update_waves(dt)
    end
    if not paused then update_camera(real_dt) end
    update_sky(dt)
    wave_banners()
    if route ~= nil then update_enemies(dt) end
    update_towers(dt)
    update_projectiles(dt)
    update_effects(dt)
    update_spells(dt)
    update_corpses(dt)
    update_sparks(dt)
    if ending == nil then check_end() end
    if paused then
        -- Measured from the end of the frame the pause was asked in: the clock stops from the next one.
        travel_at_pause = travel_at_pause or travel
        paused_travel = travel - travel_at_pause
    else
        update_cursor()
        hover_badge()
        update_hud()
    end
    mirror()
end
