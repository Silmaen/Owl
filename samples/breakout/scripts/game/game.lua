-- The rules of a Breakout level, on the `Game` entity of each level scene (one scene per level, gap G14).
--
-- - Score, lives and level live in `gamestate` (`breakout_score`, `breakout_lives`, `breakout_level`), kept across
--   the levels; `breakout_bricks` counts the bricks left in this one.
-- - Every brick broken: a burst of sparks in its colour with a flash of light (`prefabs/brick_burst.owlprefab`) and maybe
--   a falling power-up (`prefabs/power_wide.owlprefab`, `prefabs/power_slow.owlprefab`), read from the
--   `breakout_events` queue the bricks fill; each is spawned where the brick was and destroyed when done.
-- - Power-ups caught with the paddle: wide paddle (`breakout_wide`) or slow ball (`breakout_speed_factor`), for a time.
-- - No brick left: the scene of the `next_scene` property (the next level, the victory after the last one), kept in
--   `breakout_scene` for the restart of the pause window (the menus set it for the first level played); the level is
--   marked cleared, with its best score, for the level select. No life left: game over.
-- - The `pause` action (Esc, P, Start) stops the scene clock (`time.set_paused`: bodies, sparks and trail freeze) and
--   opens the pause window of `huds/play.owlhud`, which shows the controls (never shown during play).

properties = {
    { name = "level", type = "int", default = 1 },
    { name = "next_scene", type = "string", default = "scenes/victory.owl" },
    { name = "paddle_y", type = "float", default = -6.8 },
    { name = "paddle_half_height", type = "float", default = 0.18 },
    { name = "power_fall", type = "float", default = 3.0 },   -- units per second
    { name = "power_points", type = "int", default = 50 },
    { name = "slow_factor", type = "float", default = 0.65 },
    { name = "burst_time", type = "float", default = 0.9, min = 0.1 },   -- seconds a burst lives, its light fading
    { name = "burst_sparks", type = "int", default = 36, min = 1 },
    { name = "wipe_color", type = "color", default = { 0.45, 0.05, 0.55, 1 } },  -- the magenta of every Breakout screen change
    { name = "wide_time", type = "float", default = 15.0, min = 0 },
    { name = "slow_time", type = "float", default = 12.0, min = 0 },
    { name = "clear_delay", type = "float", default = 1.6, min = 0 },
    { name = "over_delay", type = "float", default = 1.4, min = 0 },
}

local FIRST_LEVEL = "scenes/level_1.owl"

local PADDLE_Y, PADDLE_HALF_HEIGHT, POWER_FALL, POWER_POINTS, SLOW_FACTOR    -- set from the properties in on_create
local POWER_HALF_W, POWER_HALF_H = 0.6, 0.26
local BURST_PREFAB = "prefabs/brick_burst.owlprefab"
local POWER_PREFABS = { wide = "prefabs/power_wide.owlprefab", slow = "prefabs/power_slow.owlprefab" }

local hud = {}
local bursts = {}             -- { id, clock, intensity }
local powers = {}             -- { id, kind, x, y }
local paused = false
local ending = nil            -- nil, "clear" or "over"
local end_clock = 0.0
local wide_clock, slow_clock = 0.0, 0.0
local message_clock = 0.0
local shown = {}

local function set_text(key, text)
    if hud[key] ~= 0 and shown[key] ~= text then
        ui.set_text(hud[key], text)
        shown[key] = text
    end
end

local function show_message(text, seconds)
    set_text("message", text)
    ui.set_visible(hud.message, true)
    message_clock = seconds or 0.0
end

function on_create()
    PADDLE_Y = paddle_y
    PADDLE_HALF_HEIGHT = paddle_half_height
    POWER_FALL = power_fall
    POWER_POINTS = power_points
    SLOW_FACTOR = slow_factor
    if not ui.is_transition_active() then ui.transition_fade_in(0.4) end
    hud.score = ui.find("ScoreText")
    hud.level = ui.find("LevelText")
    hud.lives = ui.find("LivesText")
    hud.message = ui.find("MessageText")
    hud.pause = ui.find("PauseWindow")
    hud.resume = ui.find("ResumeButton")

    if gamestate.get("breakout_lives", 0) <= 0 then gamestate.set("breakout_lives", 3) end
    gamestate.set("breakout_score", gamestate.get("breakout_score", 0))
    gamestate.set("breakout_level", level)
    gamestate.set("breakout_level_score", gamestate.get("breakout_score", 0))
    gamestate.set("breakout_bricks", #scene.find_entities("Brick"))
    gamestate.set("breakout_events", "")
    gamestate.set("breakout_serves", 0)
    gamestate.set("breakout_bursts", 0)
    gamestate.set("breakout_paused", false)
    gamestate.set("breakout_frozen", false)
    gamestate.set("breakout_wide", false)
    gamestate.set("breakout_speed_factor", 1.0)
    if hud.pause ~= 0 then ui.set_visible(hud.pause, false) end
    show_message("Level " .. level .. "\nGet ready", 2.0)
end

-- Bursts and power-ups ----------------------------------------------------------------------------------------------

local function burst(x, y, r, g, b)
    local id = scene.spawn_prefab(BURST_PREFAB, x, y, 0.3)
    if id == 0 then return end
    entity.set_tint(id, r, g, b, 1.0)
    entity.set_light_color(id, r, g, b)
    entity.particles_emit(id, burst_sparks)
    bursts[#bursts + 1] = { id = id, clock = 0.0, intensity = entity.get_light_intensity(id) or 1.0 }
    gamestate.set("breakout_bursts", gamestate.get("breakout_bursts", 0) + 1)
end

local function drop_power(kind, x, y)
    local id = scene.spawn_prefab(POWER_PREFABS[kind], x, y, 0.2)
    if id ~= 0 then powers[#powers + 1] = { id = id, kind = kind, x = x, y = y } end
end

local function read_events()
    local queue = gamestate.get("breakout_events", "")
    if queue == "" then return end
    gamestate.set("breakout_events", "")
    for kind, x, y, r, g, b in string.gmatch(queue, "(%a+) (%S+) (%S+) (%S+) (%S+) (%S+);") do
        x, y = tonumber(x), tonumber(y)
        if kind == "break" then
            burst(x, y, tonumber(r), tonumber(g), tonumber(b))
        elseif POWER_PREFABS[kind] then
            drop_power(kind, x, y)
        end
    end
end

-- The light of a burst fades out with its sparks, then the burst goes.
local function update_bursts(dt)
    for i = #bursts, 1, -1 do
        local item = bursts[i]
        item.clock = item.clock + dt
        local t = math.min(1.0, item.clock / burst_time)
        entity.set_light_intensity(item.id, item.intensity * (1.0 - t) * (1.0 - t))
        if t >= 1.0 then
            scene.destroy_entity(item.id)
            table.remove(bursts, i)
        end
    end
end

local function catch(kind)
    sound.play("sounds/game/power_up.wav")
    gamestate.set("breakout_score", gamestate.get("breakout_score", 0) + POWER_POINTS)
    if kind == "wide" then
        gamestate.set("breakout_wide", true)
        wide_clock = wide_time
        show_message("Wide paddle!", 1.2)
    else
        gamestate.set("breakout_speed_factor", SLOW_FACTOR)
        slow_clock = slow_time
        show_message("Slow ball!", 1.2)
    end
end

local function update_power_ups(dt)
    local px = gamestate.get("breakout_paddle_x", 0.0)
    local half = gamestate.get("breakout_paddle_half", 1.2)
    for i = #powers, 1, -1 do
        local item = powers[i]
        item.y = item.y - POWER_FALL * dt
        transform.set_position(item.id, item.x, item.y, 0.2)
        local touches = math.abs(item.x - px) < half + POWER_HALF_W and
                math.abs(item.y - PADDLE_Y) < PADDLE_HALF_HEIGHT + POWER_HALF_H
        local caught = touches and gamestate.get("breakout_ball_state", "") ~= "out"
        if caught or item.y < -9.0 then
            scene.destroy_entity(item.id)
            table.remove(powers, i)
            if caught then catch(item.kind) end
        end
    end
end

local function update_timers(dt)
    if wide_clock > 0.0 then
        wide_clock = wide_clock - dt
        if wide_clock <= 0.0 then gamestate.set("breakout_wide", false) end
    end
    if slow_clock > 0.0 then
        slow_clock = slow_clock - dt
        if slow_clock <= 0.0 then gamestate.set("breakout_speed_factor", 1.0) end
    end
    if message_clock > 0.0 then
        message_clock = message_clock - dt
        if message_clock <= 0.0 then ui.set_visible(hud.message, false) end
    end
end

-- Pause -------------------------------------------------------------------------------------------------------------

local function set_paused(value)
    paused = value
    gamestate.set("breakout_paused", value)
    time.set_paused(value)
    if hud.pause ~= 0 then ui.set_visible(hud.pause, value) end
    if value and hud.pause ~= 0 then
        ui.raise_window(hud.pause)
        ui.set_visible(hud.message, false)
        message_clock = 0.0
    end
    if value and hud.resume ~= 0 then ui.set_focus(hud.resume) end
end

function on_pause_resume()
    set_paused(false)
end

function on_pause_restart()
    set_paused(false)
    ending = "restart"
    gamestate.set("breakout_score", gamestate.get("breakout_level_score", 0))
    scene.transition_to(gamestate.get("breakout_scene", FIRST_LEVEL), "wipe_up", 0.3, wipe_color.r, wipe_color.g, wipe_color.b)
end

function on_pause_quit()
    set_paused(false)
    ending = "quit"
    gamestate.clear()
    scene.transition_to("scenes/title.owl", "wipe_up", 0.3, wipe_color.r, wipe_color.g, wipe_color.b)
end

-- Frame -------------------------------------------------------------------------------------------------------------

local function update_hud()
    set_text("score", string.format("SCORE %06d", gamestate.get("breakout_score", 0)))
    set_text("level", "LEVEL " .. level)
    set_text("lives", "LIVES " .. math.max(0, gamestate.get("breakout_lives", 0)))
end

-- A cleared level is marked for the level select, with the best score made in it (`breakout_done_N`,
-- `breakout_best_N` settings, saved at once).
local function save_progress()
    local points = gamestate.get("breakout_score", 0) - gamestate.get("breakout_level_score", 0)
    settings.set("breakout_done_" .. level, true)
    if points > settings.get("breakout_best_" .. level, 0) then
        settings.set("breakout_best_" .. level, points)
    end
    settings.save()
    gamestate.set("breakout_level_points", points)
end

local function check_end(dt)
    if ending == nil then
        if gamestate.get("breakout_bricks", 1) <= 0 then
            ending = "clear"
            gamestate.set("breakout_frozen", true)
            sound.play("sounds/game/level_clear.wav")
            show_message("Level clear!")
            save_progress()
        elseif gamestate.get("breakout_lives", 1) <= 0 then
            ending = "over"
            show_message("Game over")
        end
        return
    end
    end_clock = end_clock + dt
    if ending == "clear" and end_clock >= clear_delay then
        ending = "leaving"
        local target = (next_scene ~= nil and next_scene ~= "") and next_scene or "scenes/victory.owl"
        gamestate.set("breakout_scene", target)
        scene.transition_to(target, "wipe_up", 0.4, wipe_color.r, wipe_color.g, wipe_color.b)
    elseif ending == "over" and end_clock >= over_delay then
        ending = "leaving"
        scene.transition_to("scenes/game_over.owl", "wipe_up", 0.4, wipe_color.r, wipe_color.g, wipe_color.b)
    end
end

function on_update(dt)
    if ending == nil and input.is_action_just_pressed("pause") then
        set_paused(not paused)
    end
    update_hud()
    if paused then return end
    read_events()
    update_bursts(dt)
    update_power_ups(dt)
    update_timers(dt)
    check_end(dt)
end
