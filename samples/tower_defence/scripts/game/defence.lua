-- The rules of a Tower defence level, on the `Game` entity of each level scene.
--
-- - This script holds the properties and the hooks the engine calls (`on_create`, `on_update`, the HUD callbacks);
--   the rules live in the modules of `scripts/game/td/`, one per subject (`towers`, `enemies`, `waves`, `effects`,
--   `wheel`, `briefing`, `tutorial`...), sharing the level state (`td.state`) and the constants (`td.data`).
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
--   against damage, fire burns through), goblin sappers (3 lives at the keep, a blast hurting the Horde when killed),
--   orc berserkers (faster as they bleed) and the chieftain (armoured, rallies the Horde, half slowed and never frozen,
--   10 lives). The first one of a kind a level brings (`new_enemies`) shows its name and how to beat it.
-- - Waves are timed: a countdown before each one; calling it early (Space, pad Y, the HUD button) pays
--   `early_bonus` gold per second left. Every enemy reaching the keep costs a life. The waves of `night_waves` are
--   fought at night: between two waves the sky turns to dusk or to dawn, announced, and the torches are lit by night.
-- - Day and night change the towers and the Horde (`time_effects`: "subject phase stat factor", the subject a tower or
--   enemy kind, `towers` or `horde`, the phase `day`, `night` or `any`); the level `modifiers` (fog, blizzard, blood
--   moon...) and the unlocks add effects of the same form. The info scroll of a tower, the first sight of an enemy and
--   the pause window show them; the HUD shows a sun or a moon.
-- - A briefing card opens the level (its name, modifiers, new enemies and nights); a click, Enter or pad A starts it.
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
    { name = "night_waves", type = "string", default = "" },
    { name = "modifiers", type = "strings", default = {} },
    { name = "new_enemies", type = "string", default = "" },
    { name = "briefing", type = "bool", default = true },
    { name = "time_effects", type = "strings", default = {
        "archer day damage 1.1", "archer night range 0.85",
        "cannon day splash 1.15", "cannon night reload 1.15",
        "frost day time 0.8", "frost night time 1.4",
        "goblin day speed 0.9", "goblin night speed 1.15",
        "grunt day taken 0.9", "grunt night speed 1.1",
        "ogre day speed 0.85", "ogre night hp 1.15",
        "wolf day taken 1.15", "wolf night speed 1.2",
        "troll day regen 0", "troll night regen 1.5",
        "ogre_mage day ward 0.8", "ogre_mage night ward 1.5",
        "sapper day speed 0.9", "sapper night speed 1.2",
        "berserker day taken 1.1", "berserker night speed 1.15",
        "chieftain day taken 1.1", "chieftain night taken 0.85",
    } },
    { name = "start_hour", type = "float", default = 9.5, min = 0, max = 24 },
    { name = "night_hour", type = "float", default = 21.5, min = 0, max = 24 },
    { name = "morning_hour", type = "float", default = 8.0, min = 0, max = 24 },
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
    { name = "berserker_hp", type = "float", default = 120.0 },
    { name = "berserker_speed", type = "float", default = 1.05 },
    { name = "berserker_gold", type = "int", default = 16 },
    { name = "berserker_rage", type = "float", default = 0.9, min = 0 },
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

-- The rules live in the modules of `scripts/game/td/`, required in `on_create` (Owl Nest reads the
-- properties by running this chunk alone): `td` is their namespace, `S` the state, `C` the constants.
local td, S, C
local UI, Eco, Cam, World, Waves, Fx, Vis, Foes, Towers, Spells, Wheel, Input, Brief

function on_create()
    td = require("scripts.game.td.core")
    require("scripts.game.td.data")
    require("scripts.game.td.state")
    require("scripts.game.td.effects")
    require("scripts.game.td.briefing")
    require("scripts.game.td.ui")
    require("scripts.game.td.economy")
    require("scripts.game.td.camera")
    require("scripts.game.td.world")
    require("scripts.game.td.waves")
    require("scripts.game.td.visuals")
    require("scripts.game.td.enemies")
    require("scripts.game.td.towers")
    require("scripts.game.td.spells")
    require("scripts.game.td.wheel")
    require("scripts.game.td.input")
    S, C = td.state, td.data
    UI, Eco, Cam, World, Waves, Fx = td.ui, td.economy, td.camera, td.world, td.waves, td.effects
    Vis, Foes, Towers, Spells, Wheel, Input, Brief = td.visuals, td.enemies, td.towers, td.spells, td.wheel, td.input,
            td.briefing
    if not ui.is_transition_active() then
        ui.transition_play("wipe_down", 0.4, wipe_color.r, wipe_color.g, wipe_color.b)
    end
    time.set_scale(1.0)
    time.set_paused(false)
    Eco.read_stats()
    S.camera_id = scene.find_entity("MainCamera")
    S.cursor_id = scene.find_entity("Cursor")
    S.ring_id = scene.find_entity("RangeRing")
    S.sky_id = scene.find_entity("Sky")
    S.choice_ids = scene.find_entities("BuildChoice")
    for _, name in ipairs({ "KeepFlag", "GateBanner" }) do
        for _, id in ipairs(scene.find_entities(name)) do
            local _, yaw = transform.get_rotation(id)
            S.banner_ids[#S.banner_ids + 1] = { id = id, yaw = yaw }
        end
    end
    S.wheel_ids = { ring = scene.find_entity("Wheel"), plaque = scene.find_entity("WheelPlaque"),
                  title = scene.find_entity("WheelTitle"), text = scene.find_entity("WheelText"),
                  costs = scene.find_entities("ChoiceCost") }
    for _, id in ipairs(scene.find_entities("Torch")) do S.torch_ids[#S.torch_ids + 1] = { light = id } end
    local flames, glows = scene.find_entities("TorchFlame"), scene.find_entities("TorchGlow")
    for index, torch in ipairs(S.torch_ids) do
        torch.flame = flames[index] or 0
        torch.glow = glows[index] or 0
    end
    for _, name in ipairs({ "gold", "lives", "wave", "message", "call", "speed", "pause", "resume", "rain", "shield",
                            "rain_label", "shield_label", "phase", "phase_icon", "effects", "briefing",
                            "briefing_title", "briefing_text", "briefing_button" }) do
        S.hud[name] = 0
    end
    S.hud.gold = ui.find("GoldText")
    S.hud.lives = ui.find("LivesText")
    S.hud.wave = ui.find("WaveText")
    S.hud.message = ui.find("MessageText")
    S.hud.call = ui.find("CallButton")
    S.hud.rain = ui.find("RainButton")
    S.hud.shield = ui.find("ShieldButton")
    S.hud.rain_label, S.hud.shield_label = S.hud.rain, S.hud.shield
    S.hud.phase, S.hud.phase_icon, S.hud.effects = ui.find("PhaseText"), ui.find("PhaseIcon"), ui.find("EffectsText")
    S.hud.briefing, S.hud.briefing_title = ui.find("BriefingCard"), ui.find("BriefingTitle")
    S.hud.briefing_text, S.hud.briefing_button = ui.find("BriefingText"), ui.find("BriefingButton")
    S.hud.speed = ui.find("SpeedButton")
    S.hud.pause = ui.find("PauseWindow")
    S.hud.resume = ui.find("ResumeButton")
    UI.show(S.hud.pause, false)
    UI.show(S.hud.message, false)
    UI.show(S.cursor_id, false)
    UI.show(S.ring_id, false)
    World.find_pads()
    World.find_route()
    S.view.x, S.view.z = (map_width - 1) * 0.5, (map_depth - 1) * 0.5
    S.view.distance = camera_distance
    Cam.place_camera()
    -- What the campaign unlocked: the spells, more gold, more lives (`tower_done_NN`, `tower_stars_NN` settings).
    S.spells.rain = { unlocked = settings.get("tower_done_02", false) == true, clock = 0.0,
                    cooldown = rain_cooldown * (Eco.tier("rain", 1) and 0.8 or 1.0),
                    damage = rain_damage * (Eco.tier("rain", 2) and 1.3 or 1.0) * (Eco.tier("rain", 4) and 1.3 or 1.0),
                    radius = rain_radius * (Eco.tier("rain", 3) and 1.25 or 1.0), title = "Rain of arrows", casts = 0 }
    S.spells.shield = { unlocked = settings.get("tower_done_05", false) == true, clock = 0.0,
                      cooldown = shield_cooldown * (Eco.tier("shield", 1) and 0.8 or 1.0)
                              * (Eco.tier("shield", 4) and 0.8 or 1.0),
                      time = shield_time + (Eco.tier("shield", 2) and 2.0 or 0.0),
                      radius = shield_radius * (Eco.tier("shield", 3) and 1.25 or 1.0), title = "Shield wall",
                      casts = 0 }
    UI.show(S.hud.rain, S.spells.rain.unlocked)
    UI.show(S.hud.shield, S.spells.shield.unlocked)
    local bonus_gold = settings.get("tower_done_03", false) == true and treasury_gold or 0
    local all_stars = true
    for number = 1, 10 do
        if settings.get(string.format("tower_stars_%02d", number), 0) < 3 then all_stars = false end
    end
    local guard = settings.get("tower_done_09", false) == true and 3 or 0
    local chest = (Eco.tier("treasury", 1) and 40 or 0) + (Eco.tier("treasury", 4) and 60 or 0)
    local masons = Eco.tier("treasury", 3) and 2 or 0
    S.gold, S.lives_left = start_gold + bonus_gold + chest, lives + masons + guard + (all_stars and walls_lives or 0)
    gamestate.set("tower_start_gold", S.gold)
    S.wave_count = #waves
    S.countdown = first_wave_delay
    S.hours, S.hours_goal = start_hour, start_hour
    if Fx.nights[1] then S.hours, S.hours_goal = night_hour, night_hour end
    Fx.night = Fx.nights[1] == true
    gamestate.set("tower_phase", Fx.night and "night" or "day")
    gamestate.set("tower_phase_changes", 0)
    Waves.mirror_effects()
    if S.sky_id ~= 0 then
        entity.set_day_night_enabled(S.sky_id, false)
        entity.set_time_of_day(S.sky_id, S.hours % 24.0)
    end
    gamestate.set("tower_won", false)
    gamestate.set("tower_lost", false)
    gamestate.set("tower_map", level_number)
    gamestate.set("tower_scene", scene_path)
    if not briefing then UI.message(level_name .. "\n" .. S.wave_count .. " waves", 2.5) end
    UI.mirror()
end

function on_rain_clicked()
    Spells.cast("rain")
end

function on_shield_clicked()
    Spells.cast("shield")
end

local function finish(won)
    S.ending = won and "won" or "lost"
    S.end_clock = end_delay
    Wheel.close_ring()
    time.set_scale(1.0)
    S.fast = false
    local kept = math.max(0, S.lives_left)
    gamestate.set(won and "tower_won" or "tower_lost", true)
    gamestate.set("tower_result_won", won)
    gamestate.set("tower_result_kills", S.kills)
    gamestate.set("tower_result_lives", kept)
    gamestate.set("tower_result_max_lives", lives)
    gamestate.set("tower_result_gold", S.earned)
    gamestate.set("tower_result_time", math.floor(S.clock))
    gamestate.set("tower_result_towers", S.built)
    gamestate.set("tower_result_wave", S.wave)
    gamestate.set("tower_result_waves", S.wave_count)
    gamestate.set("tower_result_stars", won and Eco.stars_for(kept) or 0)
    gamestate.set("tower_result_level", level_name)
    if won then
        local done = string.format("tower_done_%02d", level_number)
        local best = string.format("tower_best_%02d", level_number)
        local stars = string.format("tower_stars_%02d", level_number)
        local first = not settings.get(done, false)
        gamestate.set("tower_result_unlock", first)
        gamestate.set("tower_result_unlock_text", first and (C.UNLOCKS[level_number] or "") or "")
        if first then gamestate.set("tower_map_reveal", level_number + 1) end
        settings.set(done, true)
        gamestate.set("tower_done", true)
        if kept > settings.get(best, 0) then settings.set(best, kept) end
        local before = settings.get(stars, 0)
        gamestate.set("tower_result_stars_gained", math.max(0, Eco.stars_for(kept) - before))
        if Eco.stars_for(kept) > before then settings.set(stars, Eco.stars_for(kept)) end
        settings.save()
        UI.message("The keep holds!", end_delay)
    else
        UI.play("sounds/game/defeat.wav")
        UI.message("The keep has fallen", end_delay)
    end
end

local function check_end()
    if S.ending ~= nil then return end
    if S.lives_left <= 0 then
        finish(false)
    elseif S.wave >= S.wave_count and S.spawning == nil and #S.enemies == 0 and S.route ~= nil then
        finish(true)
    end
end

function on_briefing_closed()
    Brief.close_briefing()
end

local function set_paused(value)
    S.paused = value
    time.set_paused(value)
    S.travel_at_pause = nil
    UI.show(S.hud.pause, value)
    if value then
        if S.hud.effects ~= 0 then ui.set_text(S.hud.effects, Brief.effects_legend()) end
        Wheel.close_ring()
        ui.raise_window(S.hud.pause)
        if S.hud.resume ~= 0 then ui.set_focus(S.hud.resume) end
    end
    UI.mirror()
end

function on_pause_resume()
    set_paused(false)
end

function on_pause_restart()
    set_paused(false)
    S.ending = "restart"
    time.set_scale(1.0)
    scene.transition_to(scene_path, "wipe_down", 0.3, wipe_color.r, wipe_color.g, wipe_color.b)
end

function on_pause_quit()
    set_paused(false)
    S.ending = "quit"
    time.set_scale(1.0)
    scene.transition_to("scenes/world_map.owl", "wipe_down", 0.3, wipe_color.r, wipe_color.g, wipe_color.b)
end

function on_menu_clicked()
    if S.ending == nil then set_paused(not S.paused) end
end

function on_call_clicked()
    Waves.call_early()
end

function on_speed_clicked()
    if S.ending ~= nil or S.paused then return end
    S.fast = not S.fast
    time.set_scale(S.fast and 2.0 or 1.0)
end

function on_update(dt)
    local real_dt = time.real_delta()
    if not S.started then
        S.started = true
        Towers.raise_start_towers()
        if briefing then Brief.open_briefing() end
    end
    if Brief.open then
        if input.is_action_just_pressed("ui_accept") or input.is_action_just_pressed("confirm")
                or input.is_action_just_pressed("primary") or input.is_action_just_pressed("call_wave") then
            Brief.close_briefing()
        end
        Waves.update_sky(0.0)
        UI.update_hud()
        UI.mirror()
        return
    end
    if S.ending == "restart" or S.ending == "quit" then return end
    if S.ending == nil and input.is_action_just_pressed("pause") then set_paused(not S.paused) end
    if S.message_clock > 0.0 then
        S.message_clock = S.message_clock - real_dt
        if S.message_clock <= 0.0 then UI.show(S.hud.message, false) end
    end
    if S.ending ~= nil then
        S.end_clock = S.end_clock - real_dt
        if S.end_clock <= 0.0 and S.ending ~= "leaving" then
            local target = S.ending == "won" and next_scene or "scenes/game_over.owl"
            S.ending = "leaving"
            scene.transition_to(target, "wipe_down", 0.5, wipe_color.r, wipe_color.g, wipe_color.b)
        end
    end
    -- The world runs on the scene clock: while paused `dt` is 0 and nothing moves (`tower_paused_travel` checks it).
    S.clock = S.clock + dt
    if S.ending == nil and not S.paused then
        Input.handle_input()
        Waves.update_waves(dt)
    end
    if not S.paused then Cam.update_camera(real_dt) end
    Waves.update_sky(dt)
    World.wave_banners()
    if S.route ~= nil then Foes.update_enemies(dt) end
    Towers.update_towers(dt)
    Towers.update_projectiles(dt)
    Vis.update_effects(dt)
    Spells.update_spells(dt)
    Foes.update_corpses(dt)
    Vis.update_sparks(dt)
    if S.ending == nil then check_end() end
    if S.paused then
        -- Measured from the end of the frame the pause was asked in: the clock stops from the next one.
        S.travel_at_pause = S.travel_at_pause or S.travel
        S.paused_travel = S.travel - S.travel_at_pause
    else
        Input.update_cursor()
        Input.hover_badge()
        UI.update_hud()
    end
    UI.mirror()
end
