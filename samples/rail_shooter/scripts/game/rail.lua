-- The rules of Ring Runner: the scene script of the level (sequence events and HUD buttons call it).
--
-- - The `RailCarrier` rides the `Rail` spline (`SplineFollower`) through an asteroid ring around a planet; it carries
--   the camera, the ship and the two reticles. The ship steers inside the play frame (`frame_x`, `frame_y`) and can
--   never leave it; the reticles show where the lasers go (mouse, or the aim stick around the nose of the ship).
-- - The modules of `scripts/game/rs/` hold the rules: `core` (the run state, the rail coordinates), `field` (the
--   rocks of the scene), `foes` (the waves of `waves`, a data table, their patterns, the boss), `guns` (the lasers).
-- - The hull recharges by `shield_rate` per second once no hit came for `shield_delay` seconds.
-- - A barrel roll (Q / E, pad LB / RB) dashes aside and turns the enemy shots away while it lasts.
-- - Three sequences of the carrier: the intro (skipped with Fire), the boss arrival and the end. The boss slows the
--   carrier down to `boss_speed`; destroying it wins the run, the hull at zero loses it.
-- - Score: rocks, enemies, the boss and the hull left; a bronze, silver or gold medal by `silver_score` and
--   `gold_score`. The results go to the victory scene as `rail_result_*` game-state keys; the state is mirrored in
--   `gamestate` (`rail_score`, `rail_hull`, `rail_phase`...) for the scenarios.
-- - Escape, P or Start pauses: the pause window is the only place of the level that shows the controls.

properties = {
    { name = "waves", type = "string", default = "tables/waves.owltable" },
    { name = "enemies", type = "string", default = "tables/enemies.owltable" },
    { name = "rail_speed", type = "float", default = 20.0, min = 0.0 },
    { name = "boss_speed", type = "float", default = 7.0, min = 0.0 },
    { name = "hull", type = "float", default = 100.0, min = 1.0 },
    { name = "frame_x", type = "float", default = 8.0, min = 1.0 },
    { name = "frame_y", type = "float", default = 4.5, min = 1.0 },
    { name = "steer_speed", type = "float", default = 13.0, min = 0.1 },
    { name = "fire_interval", type = "float", default = 0.14, min = 0.02 },
    { name = "bolt_speed", type = "float", default = 150.0, min = 1.0 },
    { name = "enemy_bolt_speed", type = "float", default = 42.0, min = 1.0 },
    { name = "aim_assist", type = "float", default = 0.08, min = 0.0 },
    { name = "aim_range_x", type = "float", default = 14.0, min = 0.0 },
    { name = "aim_range_y", type = "float", default = 9.0, min = 0.0 },
    { name = "roll_time", type = "float", default = 0.6, min = 0.1 },
    { name = "roll_cooldown", type = "float", default = 0.5, min = 0.0 },
    { name = "roll_dash", type = "float", default = 9.0, min = 0.0 },
    { name = "rock_damage", type = "float", default = 15.0, min = 0.0 },
    { name = "bolt_damage", type = "float", default = 6.0, min = 0.0 },
    { name = "ram_damage", type = "float", default = 12.0, min = 0.0 },
    { name = "shield_delay", type = "float", default = 4.0, min = 0.0 },
    { name = "shield_rate", type = "float", default = 1.0, min = 0.0 },
    { name = "boss_hold", type = "float", default = 60.0, min = 10.0 },
    { name = "rock_points", type = "int", default = 50, min = 0 },
    { name = "big_rock_points", type = "int", default = 150, min = 0 },
    { name = "big_rock", type = "float", default = 1.6, min = 0.1 },
    { name = "silver_score", type = "int", default = 22000, min = 0 },
    { name = "gold_score", type = "int", default = 30000, min = 0 },
}

local rs = require("scripts.game.rs.core")
local field = require("scripts.game.rs.field")
local foes = require("scripts.game.rs.foes")
local guns = require("scripts.game.rs.guns")
local S, C = rs.state, rs.cfg

local CAMERA_Z, CAMERA_Y = 10.0, 1.3
local intro_id, boss_seq_id, end_seq_id = 0, 0, 0
local pause_id, resume_id = 0, 0
local leaving = false

local function hud(tag)
    local id = scene.find_entity(tag)
    S.hud[tag] = id
    return id
end

local function set_text(tag, text)
    local id = S.hud[tag]
    if id ~= nil and id ~= 0 and S.shown[tag] ~= text then
        ui.set_text(id, text)
        S.shown[tag] = text
    end
end

local function show(tag, visible)
    local id = S.hud[tag]
    if id ~= nil and id ~= 0 then ui.set_visible(id, visible) end
end

local function message(text, seconds)
    set_text("Message", text)
    show("Message", true)
    S.message_clock = seconds
end

local function set_reticles(visible)
    entity.set_hud_marker_enabled(S.reticle, visible)
    entity.set_hud_marker_enabled(S.reticle_near, visible)
end

local function hit_ratio()
    if S.shots == 0 then return 0 end
    return math.floor(S.hits * 100 / S.shots + 0.5)
end

local function mirror()
    gamestate.set("rail_phase", S.phase)
    gamestate.set("rail_distance", math.floor(S.s))
    gamestate.set("rail_score", S.score)
    gamestate.set("rail_hull", math.floor(S.hull + 0.5))
    gamestate.set("rail_shots", S.shots)
    gamestate.set("rail_hits", S.hits)
    gamestate.set("rail_kills", S.kills)
    gamestate.set("rail_rocks", S.rocks)
    gamestate.set("rail_rolls", S.rolls)
    gamestate.set("rail_deflected", S.deflected or 0)
    gamestate.set("rail_taken_rock", S.taken.rock or 0)
    gamestate.set("rail_taken_bolt", S.taken.bolt or 0)
    gamestate.set("rail_taken_ram", S.taken.ram or 0)
    gamestate.set("rail_enemies", #S.enemies)
    gamestate.set("rail_wave", S.next_wave - 1)
    gamestate.set("rail_ship_x", math.floor(S.ship_x * 100) / 100)
    gamestate.set("rail_ship_y", math.floor(S.ship_y * 100) / 100)
    gamestate.set("rail_paused", S.paused)
    gamestate.set("rail_boss_hp", S.boss ~= nil and math.max(0, S.boss.hp) or -1)
end

function on_create()
    for _, property in ipairs(properties) do C[property.name] = _G[property.name] end
    C.ship_radius, C.reticle_depth = 0.9, 40.0
    S.rail = scene.find_entity("Rail")
    S.carrier = scene.find_entity("RailCarrier")
    S.ship = scene.find_entity("Ship")
    S.camera = scene.find_entity("MainCamera")
    S.reticle = scene.find_entity("Reticle")
    S.reticle_near = scene.find_entity("ReticleNear")
    intro_id = scene.find_entity("IntroDirector")
    boss_seq_id = scene.find_entity("BossDirector")
    end_seq_id = scene.find_entity("EndDirector")
    S.hull, S.hull_max = C.hull, C.hull
    S.speed = C.rail_speed
    spline.set_speed(S.carrier, S.speed)
    for index = 1, 6 do
        S.bursts[index] = scene.find_entity("Burst" .. index)
        S.debris[index] = scene.find_entity("Debris" .. index)
        S.sparks[index] = scene.find_entity("Spark" .. index)
    end
    for _, tag in ipairs({ "ScoreText", "HitsText", "HullText", "HullBar", "BossGroup", "BossBar", "Message",
                           "DistanceBar" }) do
        hud(tag)
    end
    pause_id = hud("PauseWindow")
    resume_id = scene.find_entity("ResumeButton")
    field.load()
    foes.load(C.waves, C.enemies)
    set_reticles(false)
    gamestate.set("rail_won", false)
    gamestate.set("rail_lost", false)
    mirror()
    if not ui.is_transition_active() then ui.transition_play("fade", 0.6, 0.0, 0.02, 0.06) end
end

-- Event keys and ends of the sequences.
function on_intro_title()
    message("RING OF KAEL - CLEAR THE LANE", 2.5)
end

function on_intro_finished()
    if S.phase ~= "intro" then return end
    S.phase, S.phase_clock = "play", 0.0
    set_reticles(true)
end

function on_boss_alarm()
    rs.play("sounds/game/alarm.wav")
    message("WARNING - THE WARDEN", 3.0)
end

function on_boss_intro_finished()
    if S.phase == "boss_intro" then S.phase, S.phase_clock = "boss", 0.0 end
    set_reticles(S.phase == "boss")
end

function on_outro_fade()
    if leaving then return end
    leaving = true
    scene.transition_to("scenes/victory.owl", "fade", 0.8, 0.9, 0.95, 1.0)
end

function on_outro_finished()
    on_outro_fade()
end

local function start_boss()
    S.phase, S.phase_clock = "boss_intro", 0.0
    show("BossGroup", true)
    set_reticles(false)
    if boss_seq_id == 0 or not sequence.play(boss_seq_id) then on_boss_intro_finished() end
end

local function medal()
    if S.score >= C.gold_score then return 3 end
    if S.score >= C.silver_score then return 2 end
    return 1
end

local function win()
    S.phase, S.phase_clock = "outro", 0.0
    foes.clear()
    guns.clear()
    show("BossGroup", false)
    set_reticles(false)
    local bonus = math.floor(S.hull) * 30
    rs.add_score(bonus)
    gamestate.set("rail_won", true)
    gamestate.set("rail_result_score", S.score)
    gamestate.set("rail_result_bonus", bonus)
    gamestate.set("rail_result_hits", hit_ratio())
    gamestate.set("rail_result_kills", S.kills)
    gamestate.set("rail_result_foes", S.foes)
    gamestate.set("rail_result_rocks", S.rocks)
    gamestate.set("rail_result_hull", math.floor(S.hull + 0.5))
    gamestate.set("rail_result_time", math.floor(S.clock))
    gamestate.set("rail_result_medal", medal())
    message("SECTOR CLEARED", 3.0)
    if end_seq_id == 0 or not sequence.play(end_seq_id) then on_outro_fade() end
end

local function lose()
    S.phase, S.phase_clock = "dead", 0.0
    rs.burst("bursts", S.s, S.ship_x, S.ship_y, 160)
    rs.play("sounds/game/boss_explosion.wav")
    ui.set_visible(S.ship, false)
    set_reticles(false)
    gamestate.set("rail_lost", true)
    gamestate.set("rail_result_score", S.score)
    gamestate.set("rail_result_distance", math.floor(math.min(100, S.s * 100 / S.boss_distance)))
end

-- Steering inside the frame, the barrel roll, the bank and the camera following the ship.
local function steer(dt)
    local sx, sy = input.get_action_vector("steer")
    if settings.get("rail_invert_y", false) then sy = -sy end
    local target_vx, target_vy = sx * C.steer_speed, -sy * C.steer_speed
    local blend = math.min(1, dt * 8)
    S.vx = S.vx + (target_vx - S.vx) * blend
    S.vy = S.vy + (target_vy - S.vy) * blend
    S.roll_cooldown = math.max(0, S.roll_cooldown - dt)
    if S.roll_time <= 0 and S.roll_cooldown <= 0 then
        local dir = 0
        if input.is_action_just_pressed("roll_left") then dir = -1 end
        if input.is_action_just_pressed("roll_right") then dir = 1 end
        if dir ~= 0 then
            S.roll_time, S.roll_dir, S.rolls = C.roll_time, dir, S.rolls + 1
            rs.play("sounds/game/roll.wav")
        end
    end
    local dash = 0
    if S.roll_time > 0 then
        S.roll_time = math.max(0, S.roll_time - dt)
        if S.roll_time <= 0 then S.roll_cooldown = C.roll_cooldown end
        dash = S.roll_dir * C.roll_dash
    end
    S.ship_x = math.max(-C.frame_x, math.min(C.frame_x, S.ship_x + (S.vx + dash) * dt))
    S.ship_y = math.max(-C.frame_y, math.min(C.frame_y, S.ship_y + S.vy * dt))
end

-- The far reticle: under the mouse once it moved, else around the nose of the ship by the aim stick.
local function aim(dt)
    local ax, ay = input.get_action_vector("aim")
    local mx, my = scene.get_viewport_mouse()
    if mx ~= nil and (math.abs(mx - S.mouse_x) > 1 or math.abs(my - S.mouse_y) > 1) then
        if S.mouse_x >= 0 then S.mouse_aim = true end
        S.mouse_x, S.mouse_y = mx, my
    end
    if ax * ax + ay * ay > 0.04 then S.mouse_aim = false end
    local depth = CAMERA_Z + C.reticle_depth
    local tx, ty
    local w, h = scene.get_viewport_size()
    if S.mouse_aim and w ~= nil and w > 0 and h > 0 then
        local half_h = depth * math.tan(0.45)
        local cx, cy = S.ship_x * 0.4, CAMERA_Y + S.ship_y * 0.4
        tx = cx + (mx / w * 2 - 1) * half_h * w / h
        ty = cy + (1 - my / h * 2) * half_h
    else
        tx = S.ship_x * 1.6 + ax * C.aim_range_x
        ty = S.ship_y * 1.6 - ay * C.aim_range_y
    end
    local blend = math.min(1, dt * 14)
    S.aim_x = S.aim_x + (tx - S.aim_x) * blend
    S.aim_y = S.aim_y + (ty - S.aim_y) * blend
end

local function pose_ship(dt)
    local roll = -S.vx / C.steer_speed * 0.6
    if S.roll_time > 0 then roll = roll - S.roll_dir * (1 - S.roll_time / C.roll_time) * 2 * math.pi end
    local yaw = -(S.aim_x - S.ship_x) / C.reticle_depth * 0.6
    local pitch = (S.aim_y - S.ship_y) / C.reticle_depth * 0.6 + S.vy / C.steer_speed * 0.2
    local z = 0
    if S.phase == "outro" then z = -(S.phase_clock ^ 2) * 14 end
    transform.set_position(S.ship, S.ship_x, S.ship_y, z)
    transform.set_rotation(S.ship, pitch, yaw, roll)
    local shake = S.hurt_clock > 0 and math.sin(S.clock * 90) * S.hurt_clock * 0.5 or 0
    transform.set_position(S.camera, S.ship_x * 0.4 + shake, CAMERA_Y + S.ship_y * 0.4, CAMERA_Z)
    transform.set_position(S.reticle, S.aim_x, S.aim_y, -C.reticle_depth)
    local near = 18 / C.reticle_depth
    transform.set_position(S.reticle_near, S.ship_x + (S.aim_x - S.ship_x) * near,
            S.ship_y + (S.aim_y - S.ship_y) * near, -18)
    S.hurt_clock = math.max(0, S.hurt_clock - dt)
end

local function update_hud(dt)
    set_text("ScoreText", string.format("SCORE %06d", S.score))
    set_text("HitsText", string.format("HITS %d%%", hit_ratio()))
    set_text("HullText", string.format("HULL %d", math.floor(S.hull + 0.5)))
    if S.hud.HullBar ~= 0 then ui.set_progress(S.hud.HullBar, S.hull / S.hull_max) end
    if S.hud.DistanceBar ~= 0 then ui.set_progress(S.hud.DistanceBar, math.min(1, S.s / S.boss_distance)) end
    if S.boss ~= nil and S.hud.BossBar ~= 0 then ui.set_progress(S.hud.BossBar, math.max(0, S.boss.hp) / S.boss.max_hp) end
    if S.message_clock > 0 then
        S.message_clock = S.message_clock - dt
        if S.message_clock <= 0 then show("Message", false) end
    end
end

local function set_paused(paused)
    S.paused = paused
    time.set_paused(paused)
    if pause_id ~= 0 then
        ui.set_visible(pause_id, paused)
        if paused then
            ui.raise_window(pause_id)
            if resume_id ~= 0 then ui.set_focus(resume_id) end
        end
    end
end

function on_update(dt)
    if leaving then return end
    if input.is_action_just_pressed("pause") and S.phase ~= "dead" and S.phase ~= "outro" then
        set_paused(not S.paused)
    elseif S.paused and input.is_action_just_pressed("ui_cancel") then
        set_paused(false)
    end
    if S.paused then
        mirror()
        return
    end
    S.clock = S.clock + dt
    S.phase_clock = S.phase_clock + dt
    S.s = spline.get_distance(S.carrier) or S.s
    local wave_due = S.phase == "play" or S.phase == "intro"
    if S.phase == "intro" and input.is_action_just_pressed("fire") and intro_id ~= 0 then
        sequence.stop(intro_id)
    end
    if S.phase == "boss" or S.phase == "boss_intro" then
        S.speed = math.max(C.boss_speed, S.speed - dt * 6)
        spline.set_speed(S.carrier, S.speed)
    end
    if S.phase ~= "dead" then
        steer(dt)
        aim(dt)
        pose_ship(dt)
    end
    if (S.phase == "play" or S.phase == "boss") and input.is_action_pressed("fire") then
        S.fire_clock = S.fire_clock - dt
        if S.fire_clock <= 0 then
            S.fire_clock = C.fire_interval
            guns.fire()
        end
    else
        S.fire_clock = math.min(S.fire_clock, 0)
    end
    if wave_due then foes.spawn_due(start_boss) end
    field.update(dt)
    foes.update(dt, S.phase == "play" or S.phase == "boss")
    guns.update(dt)
    if S.boss_dead and (S.phase == "boss" or S.phase == "boss_intro") then win() end
    S.calm_clock = (S.calm_clock or 0) + dt
    if S.hurt_clock > 0 then S.calm_clock = 0 end
    if S.calm_clock > C.shield_delay and S.phase ~= "dead" then
        S.hull = math.min(S.hull_max, S.hull + C.shield_rate * dt)
    end
    if S.hull <= 0 and S.phase ~= "dead" and S.phase ~= "outro" then lose() end
    if S.phase == "dead" and S.phase_clock > 2.0 and not leaving then
        leaving = true
        scene.transition_to("scenes/game_over.owl", "fade", 0.8, 0.25, 0.02, 0.04)
    end
    update_hud(dt)
    mirror()
end

function on_resume_clicked()
    set_paused(false)
end

function on_restart_clicked()
    if leaving then return end
    leaving = true
    time.set_paused(false)
    scene.transition_to("scenes/level.owl", "fade", 0.4, 0.0, 0.02, 0.06)
end

function on_quit_clicked()
    if leaving then return end
    leaving = true
    time.set_paused(false)
    gamestate.clear()
    scene.transition_to("scenes/title.owl", "fade", 0.4, 0.0, 0.02, 0.06)
end
