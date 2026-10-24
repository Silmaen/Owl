-- The rules of a Tower defence level, on the `Game` entity of each level scene.
--
-- - The level gives its waves, its gold and its lives as properties (`waves`: one string per wave, groups
--   "kind count gap" separated by ";"); the pads are the `Pad` entities, the road is the `Waypoint NN` entities in
--   the order of their numbers, from the gate to the keep. A new level is a duplicated scene: move the tiles, the
--   pads and the waypoints in Owl Nest, edit the waves.
-- - The values of the towers and of the enemies are properties of this script, the same for every level unless a
--   level overrides one in the inspector. A pad on high ground gives its tower `height_bonus` more range per step.
-- - Towers go from level 1 to 3, then take one power-up of two; the second power-up of each tower is unlocked by
--   winning the first level (`tower_done_01`).
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
    { name = "sky_elevation", type = "float", default = 58.0, min = 1, max = 90 },
    { name = "sky_night_exposure", type = "float", default = 6.0 },
    { name = "start_gold", type = "int", default = 240, min = 0 },
    { name = "lives", type = "int", default = 20, min = 1 },
    { name = "map_width", type = "int", default = 18, min = 1 },
    { name = "map_depth", type = "int", default = 14, min = 1 },
    { name = "first_wave_delay", type = "float", default = 25.0, min = 0 },
    { name = "wave_delay", type = "float", default = 18.0, min = 0 },
    { name = "early_bonus", type = "float", default = 2.0, min = 0 },
    { name = "hp_growth", type = "float", default = 0.06, min = 0 },
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
    { name = "goblin_hp", type = "float", default = 28.0 },
    { name = "goblin_speed", type = "float", default = 1.6 },
    { name = "goblin_gold", type = "int", default = 7 },
    { name = "grunt_hp", type = "float", default = 75.0 },
    { name = "grunt_speed", type = "float", default = 1.0 },
    { name = "grunt_gold", type = "int", default = 12 },
    { name = "ogre_hp", type = "float", default = 220.0 },
    { name = "ogre_speed", type = "float", default = 0.65 },
    { name = "ogre_gold", type = "int", default = 30 },
    { name = "ogre_arrow", type = "float", default = 0.4, min = 0, max = 1 },
    { name = "arrow_speed", type = "float", default = 9.0, min = 0.1 },
    { name = "ball_flight", type = "float", default = 0.8, min = 0.1 },
    { name = "camera_pitch", type = "float", default = -0.75, min = -1.5, max = -0.2 },
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
    archer = { "", "Level 2: longer range.", "Level 3: faster volleys." },
    cannon = { "", "Level 2: heavier shells.", "Level 3: wider bursts." },
    frost = { "", "Level 2: colder, slower enemies.", "Level 3: the chill lasts longer." },
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
local UNLOCK_NAME = "Greenmarch Ford"
local PROJECTILE = { archer = "prefabs/projectile_arrow.owlprefab", cannon = "prefabs/projectile_ball.owlprefab",
                     fire = "prefabs/projectile_fireball.owlprefab", frost = "prefabs/projectile_shard.owlprefab" }
local SHOT_SOUND = { archer = "sounds/game/arrow.wav", cannon = "sounds/game/cannon.wav",
                     frost = "sounds/game/frost.wav" }
local SHOT_HEIGHT = { 1.15, 1.6, 2.05 }   -- height of the weapon over the pad, by tower level
local PAD_TOP = 0.21                      -- height of a pad over the grass
local GRASS_TOP = 0.4                     -- height of the grass over the base of a tile
-- Sprites: the foot stands 22 pixels under the centre of a 64-pixel cell (scaled with the sprite).
local FOOT_SHARE = 22.0 / 64.0
local sprite_lift = 22.0 / 46.0
local TOWARD_VIEW = 0.25                  -- billboards come this far toward the camera, off the ground they stand on
local BAR_HEIGHT = { goblin = 0.62, grunt = 0.86, ogre = 1.12 }
local SHADOW_SIZE = { goblin = 0.45, grunt = 0.6, ogre = 0.9 }
local DIRECTIONS = 8

local camera_id, cursor_id, ring_id, sky_id, ambient_id = 0, 0, 0, 0, 0
local choice_ids = {}
local torch_ids = {}
local hud = {}
local shown = {}

local stats = {}            -- tower stats by kind, enemy stats by kind, read from the properties
local pads = {}             -- list of { id, x, y, z, level, tower }
local pad_by_id = {}
local route = nil           -- the waypoints, with the distance walked at each one

local enemies, towers, projectiles, effects = {}, {}, {}, {}
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

local view = { x = 9, z = 7, yaw = 0.0, yaw_goal = 0.0, distance = 15.0 }
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
    transform.set_position(camera_id, view.x - f[1] * view.distance, target_y - f[2] * view.distance,
            view.z - f[3] * view.distance)
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

local function read_stats()
    stats.archer = { cost = archer_cost, range = archer_range, damage = archer_damage, reload = archer_reload }
    stats.cannon = { cost = cannon_cost, range = cannon_range, damage = cannon_damage, reload = cannon_reload,
                     splash = cannon_splash }
    stats.frost = { cost = frost_cost, range = frost_range, damage = frost_damage, reload = frost_reload,
                    slow = frost_slow, time = frost_time }
    stats.goblin = { hp = goblin_hp, speed = goblin_speed, gold = goblin_gold, arrow = 1.0 }
    stats.grunt = { hp = grunt_hp, speed = grunt_speed, gold = grunt_gold, arrow = 1.0 }
    stats.ogre = { hp = ogre_hp, speed = ogre_speed, gold = ogre_gold, arrow = ogre_arrow }
end

-- The stats of a tower at its level and power-up, its range grown by the height of its pad.
local function tower_stats(tower)
    local base = stats[tower.kind]
    local s = { range = base.range, damage = base.damage, reload = base.reload, splash = base.splash,
                slow = base.slow, time = base.time }
    if tower.level >= 2 then
        if tower.kind == "archer" then s.range = s.range * 1.2
        elseif tower.kind == "cannon" then s.damage = s.damage * 1.4
        else s.slow = s.slow * 0.8 end
    end
    if tower.level >= 3 then
        if tower.kind == "archer" then s.reload = s.reload * 0.7
        elseif tower.kind == "cannon" then s.splash = s.splash * 1.35
        else s.time = s.time * 1.45 end
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

local function find_route()
    local points = {}
    for number = 1, 99 do
        local id = scene.find_entity(string.format("Waypoint %02d", number))
        if id ~= 0 then
            local x, y, z = transform.get_position(id)
            points[#points + 1] = { x = x, y = y, z = z, at = 0.0 }
        end
    end
    if #points < 2 then
        log.warn("Tower defence: the level needs at least two Waypoint NN entities.")
        return
    end
    route = { points = points, total = 0.0 }
    for i = 2, #points do
        local a, b = points[i - 1], points[i]
        local dx, dy, dz = b.x - a.x, b.y - a.y, b.z - a.z
        route.total = route.total + math.sqrt(dx * dx + dy * dy + dz * dz)
        b.at = route.total
    end
    gamestate.set("tower_route", #points)
end

-- The point of the road at a distance from the gate and the direction of the road there (on the ground plane).
local function along(distance)
    local points = route.points
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
    ambient_id = scene.find_entity("Ambient2D")
    choice_ids = scene.find_entities("BuildChoice")
    for _, id in ipairs(scene.find_entities("Torch")) do torch_ids[#torch_ids + 1] = { light = id } end
    local flames, glows = scene.find_entities("TorchFlame"), scene.find_entities("TorchGlow")
    for index, torch in ipairs(torch_ids) do
        torch.flame = flames[index] or 0
        torch.glow = glows[index] or 0
    end
    for _, name in ipairs({ "gold", "lives", "wave", "message", "info", "info_icon", "info_title", "info_text",
                            "call", "speed", "pause", "resume" }) do
        hud[name] = 0
    end
    hud.gold = ui.find("GoldText")
    hud.lives = ui.find("LivesText")
    hud.wave = ui.find("WaveText")
    hud.message = ui.find("MessageText")
    hud.info = ui.find("InfoPanel")
    hud.info_icon = ui.find("InfoIcon")
    hud.info_title = ui.find("InfoTitle")
    hud.info_text = ui.find("InfoText")
    hud.call = ui.find("CallButton")
    hud.speed = ui.find("SpeedButton")
    hud.pause = ui.find("PauseWindow")
    hud.resume = ui.find("ResumeButton")
    show(hud.pause, false)
    show(hud.info, false)
    show(hud.message, false)
    show(cursor_id, false)
    show(ring_id, false)
    for _, id in ipairs(choice_ids) do show(id, false) end
    find_pads()
    find_route()
    view.x, view.z = (map_width - 1) * 0.5, (map_depth - 1) * 0.5
    view.distance = camera_distance
    place_camera()
    gold, lives_left = start_gold, lives
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
    if ambient_id ~= 0 then
        -- Sprites follow the sky: a blue dimness at night, against the exposure the cycle adds to the camera then.
        local sun_y = math.sin((hours - 6.0) / 24.0 * 2.0 * math.pi) * math.sin(math.rad(sky_elevation))
        local t = math.max(0.0, math.min(1.0, (sun_y + 0.15) / 0.2))
        local night = 1.0 - t * t * (3.0 - 2.0 * t)
        local level = (1.0 - 0.55 * night) * 2.0 ^ (-sky_night_exposure * night)
        entity.set_light_intensity(ambient_id, level)
        entity.set_light_color(ambient_id, 1.0 - 0.35 * night, 1.0 - 0.25 * night, 1.0)
    end
end

-- Waves ---------------------------------------------------------------------------------------------------------------

local function parse_wave(text)
    local queue = {}
    for group in string.gmatch(text or "", "[^;]+") do
        local kind, count, gap = string.match(group, "^%s*(%a+)%s+(%d+)%s+([%d%.]+)%s*$")
        if kind ~= nil and stats[kind] ~= nil and stats[kind].hp ~= nil then
            for i = 1, tonumber(count) do
                queue[#queue + 1] = { kind = kind, gap = (i == 1 and #queue > 0) and tonumber(gap) * 2 or tonumber(gap) }
            end
        else
            log.warn("Tower defence: wave group '" .. group .. "' is not 'kind count gap'.")
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

local function spawn_enemy(kind)
    local base = stats[kind]
    local x, y, z = along(0.0)
    local id = scene.spawn_prefab("prefabs/enemy_" .. kind .. ".owlprefab", billboard_at(x, y, z))
    if id == 0 then return end
    local _, size = transform.get_scale(id)
    sprite_lift = size * FOOT_SHARE
    local bar = scene.spawn_prefab("prefabs/health_bar.owlprefab", x, y + 1.0, z)
    show(bar, false)
    local shadow = scene.spawn_prefab("prefabs/shadow.owlprefab", x, y + 0.03, z)
    if shadow ~= 0 then
        transform.set_rotation(shadow, -math.pi / 2, 0.0, 0.0)
        transform.set_scale(shadow, SHADOW_SIZE[kind], SHADOW_SIZE[kind], 1.0)
    end
    local hp = base.hp * (1.0 + hp_growth * (wave - 1))
    spawned = spawned + 1
    local side = ((spawned * 37) % 7) / 6.0 - 0.5
    enemies[#enemies + 1] = {
        id = id, bar = bar, shadow = shadow, kind = kind, hp = hp, max = hp, speed = base.speed, gold = base.gold,
        arrow = base.arrow, distance = 0.0, side = side * 0.28, slow = 0.0, slow_factor = 1.0, frozen = 0.0,
        frost_hits = 0, burn = 0.0, x = x, y = y, z = z, dx = 1.0, dz = 0.0, walk = spawned * 0.37, frame = -1,
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
                spawn_enemy(next_one.kind)
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

local function remove_enemy(index)
    local foe = enemies[index]
    scene.destroy_entity(foe.id)
    if foe.bar ~= 0 then scene.destroy_entity(foe.bar) end
    if foe.shadow ~= 0 then scene.destroy_entity(foe.shadow) end
    foe.dead = true
    table.remove(enemies, index)
end

local function hurt(foe, amount)
    if foe.dead then return end
    if foe.vulnerable then amount = amount * aura_damage end
    foe.hp = foe.hp - amount
    if foe.hp <= 0 then
        gold = gold + foe.gold
        earned = earned + foe.gold
        kills = kills + 1
        play("sounds/game/pop.wav")
        spawn_puff(foe.x, foe.y + 0.2, foe.z, 1.0, 0.95, 0.7, 0.7, 0.35)
        for index, other in ipairs(enemies) do
            if other == foe then
                remove_enemy(index)
                break
            end
        end
        return
    end
    if foe.bar ~= 0 then
        local share = foe.hp / foe.max
        show(foe.bar, true)
        transform.set_scale(foe.bar, 0.5 * share, 0.07, 1.0)
        entity.set_tint(foe.bar, 1.0 - share * 0.8, 0.25 + share * 0.65, 0.2, 1.0)
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
        local speed = foe.speed
        if foe.frozen > 0.0 then
            foe.frozen = foe.frozen - dt
            speed = 0.0
        elseif foe.slow > 0.0 then
            foe.slow = foe.slow - dt
            speed = speed * foe.slow_factor
        end
        if foe.burn > 0.0 then
            foe.burn = foe.burn - dt
            hurt(foe, fire_damage * dt)
        end
        if not foe.dead then
            local step = speed * dt
            foe.distance = foe.distance + step
            travel = travel + step
            if foe.distance >= route.total then
                lives_left = lives_left - 1
                leaks = leaks + 1
                play("sounds/game/leak.wav")
                message("The keep is hit! " .. math.max(0, lives_left) .. " lives left", 1.5)
                remove_enemy(index)
            else
                local x, y, z, dx, dz = along(foe.distance)
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
                transform.set_position(foe.id, billboard_at(x, y, z))
                if foe.bar ~= 0 then transform.set_position(foe.bar, billboard_at(x, y, z, BAR_HEIGHT[foe.kind])) end
                if foe.shadow ~= 0 then transform.set_position(foe.shadow, x, y + 0.03, z) end
                local tint = foe.frozen > 0.0 and "frozen" or (foe.slow > 0.0 and "slow" or (foe.burn > 0.0 and "burn" or "plain"))
                if foe.tinted ~= tint then
                    foe.tinted = tint
                    if tint == "frozen" then entity.set_tint(foe.id, 0.75, 0.95, 1.0, 1.0)
                    elseif tint == "slow" then entity.set_tint(foe.id, 0.65, 0.82, 1.0, 1.0)
                    elseif tint == "burn" then entity.set_tint(foe.id, 1.0, 0.7, 0.5, 1.0)
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
    show(hud.info, false)
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
        shot.tx, shot.ty, shot.tz = along(math.min(lead, route.total))
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
        tower.cooldown = tower.cooldown - dt
        if tower.cooldown <= 0.0 then
            local found = targets_of(tower, tower.power == "double" and 2 or 1)
            if #found > 0 then
                for _, foe in ipairs(found) do fire(tower, foe) end
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
        foe.slow_factor = s.slow
        if shot.power == "freeze" then
            foe.frost_hits = foe.frost_hits + 1
            if foe.frost_hits % 3 == 0 then foe.frozen = freeze_time end
        end
        spawn_puff(foe.x, foe.y + 0.3, foe.z, 0.6, 0.85, 1.0, 0.7, 0.3)
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

-- The build ring ------------------------------------------------------------------------------------------------------

-- The badges over a pad, spread along the screen.
local function badge_position(pad, slot, count)
    local height = pad.tower ~= nil and SHOT_HEIGHT[pad.tower.level] + 0.9 or 1.25
    local offset = (slot - (count + 1) * 0.5) * 0.62
    local r = basis.right
    return pad.x + r[1] * offset, pad.y + height, pad.z + r[3] * offset
end

local function show_range(pad, range)
    if ring_id == 0 or range == nil then
        show(ring_id, false)
        return
    end
    transform.set_position(ring_id, pad.x, pad.y + 0.03, pad.z)
    transform.set_scale(ring_id, range * 2.0, range * 2.0, 1.0)
    show(ring_id, true)
end

-- The range a choice of the ring would give.
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

local function refresh_info()
    if ring == nil then
        show(hud.info, false)
        return
    end
    local pad = ring.pad
    local choice = ring.choices[hover]
    show(hud.info, true)
    show_range(pad, range_of_choice(pad, choice))
    local tower = pad.tower
    if choice == nil then
        if tower == nil then
            ui.set_image_frame(hud.info_icon, ICON.archer)
            set_text("info_title", "Build pad")
            set_text("info_text", string.format("Archer %d, cannon %d, frost %d gold.%s", stats.archer.cost,
                    stats.cannon.cost, stats.frost.cost, height_text(pad)))
        else
            ui.set_image_frame(hud.info_icon, ICON[tower.power or tower.kind])
            set_text("info_title", NAMES[tower.kind] .. " - level " .. tower.level ..
                    (tower.power and (", " .. POWER_NAMES[tower.power]) or ""))
            set_text("info_text", "Point at a badge." .. height_text(pad))
        end
        return
    end
    local kind = choice.kind
    local price = choice_price(pad, choice)
    if kind == "sell" then
        ui.set_image_frame(hud.info_icon, ICON.sell)
        set_text("info_title", "Sell for " .. refund_of(tower) .. " gold")
        set_text("info_text", "Half of what this tower cost (X, pad X).")
    elseif kind == "upgrade" then
        ui.set_image_frame(hud.info_icon, ICON.upgrade)
        set_text("info_title", NAMES[tower.kind] .. " to level " .. (tower.level + 1) .. " - " .. price .. " gold")
        set_text("info_text", gold >= price and LEVEL_TEXT[tower.kind][tower.level + 1]
                or ("Not enough gold: " .. price - gold .. " more."))
    elseif POWER_NAMES[kind] ~= nil then
        ui.set_image_frame(hud.info_icon, choice.locked and ICON.locked or ICON[kind])
        set_text("info_title", POWER_NAMES[kind] .. " - " .. price .. " gold")
        if choice.locked then
            set_text("info_text", "Unlocked by winning " .. UNLOCK_NAME .. ".")
        else
            set_text("info_text", gold >= price and POWER_TEXT[kind] or ("Not enough gold: " .. price - gold .. " more."))
        end
    else
        ui.set_image_frame(hud.info_icon, ICON[kind])
        set_text("info_title", NAMES[kind] .. " - " .. price .. " gold")
        set_text("info_text", (gold >= price and BLURBS[kind] or ("Not enough gold: " .. price - gold .. " more."))
                .. height_text(pad))
    end
end

local function refresh_badges()
    if ring == nil then return end
    for index, choice in ipairs(ring.choices) do
        local lit = index == hover
        local afford = choice.kind == "sell" or (not choice.locked and gold >= choice_price(ring.pad, choice))
        local base = afford and 1.0 or 0.45
        local glow = lit and 1.0 or 0.85
        entity.set_tint(choice.id, base * glow, base * glow, base * glow * (afford and 1.0 or 0.9), 1.0)
        local size = lit and 0.68 or 0.55
        transform.set_scale(choice.id, size, size, 1.0)
    end
end

local function place_badges()
    if ring == nil then return end
    for index, choice in ipairs(ring.choices) do
        transform.set_position(choice.id, badge_position(ring.pad, index, #ring.choices))
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
            ring.choices[#ring.choices + 1] = { kind = kind, id = id, locked = locked }
        end
    end
    place_badges()
    hover = pad_mode and 1 or 0
    play("sounds/ui/click.wav")
    refresh_badges()
    refresh_info()
end

local function tower_prefab(tower)
    local key = tower.power or tostring(tower.level)
    return "prefabs/tower_" .. tower.kind .. "_" .. key .. ".owlprefab"
end

local function place_tower(tower)
    if tower.id ~= nil and tower.id ~= 0 then scene.destroy_entity(tower.id) end
    tower.id = scene.spawn_prefab(tower_prefab(tower), tower.x, tower.y + PAD_TOP, tower.z)
    tower.stats = tower_stats(tower)
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
        refresh_badges()
        refresh_info()
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
            refresh_badges()
            refresh_info()
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
        refresh_info()
        refresh_badges()
        place_badges()
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
        gamestate.set("tower_result_unlock", (not settings.get(done, false)) and level_number == 1)
        settings.set(done, true)
        gamestate.set("tower_done", true)
        if kept > settings.get(best, 0) then settings.set(best, kept) end
        if stars_for(kept) > settings.get(stars, 0) then settings.set(stars, stars_for(kept)) end
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
    scene.transition_to("scenes/title.owl", "wipe_down", 0.3, wipe_color.r, wipe_color.g, wipe_color.b)
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
    if route ~= nil then update_enemies(dt) end
    update_towers(dt)
    update_projectiles(dt)
    update_effects(dt)
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
