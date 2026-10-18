-- The rules of a Breakout level, on the `Game` entity of each level scene (one scene per level, gap G14).
--
-- - Score, lives and level live in `gamestate` (`breakout_score`, `breakout_lives`, `breakout_level`), kept across
--   the levels; `breakout_bricks` counts the bricks left in this one.
-- - Every brick broken: a burst and maybe a power-up, read from the `breakout_events` queue the bricks fill. The bursts
--   and the falling power-ups are authored entities shown and hidden from pools (gap G1): `BreakFx1..`,
--   `PowerWide1..`, `PowerSlow1..`.
-- - Power-ups caught with the paddle: wide paddle (`breakout_wide`) or slow ball (`breakout_speed_factor`), for a time.
-- - No brick left: the scene of the `next_scene` property (the next level, the victory after the last one), kept in
--   `breakout_scene` for the restart of the pause window. No life left: game over. The `pause` action (Esc, P, Start)
--   stops the ball and the paddle (gap G8: Lua stops the bodies by hand) and opens the pause window of
--   `huds/play.owlhud`.

properties = {
    { name = "level", type = "int", default = 1 },
    { name = "next_scene", type = "string", default = "scenes/victory.owl" },
}

local FIRST_LEVEL = "scenes/level_1.owl"

local PADDLE_Y = -6.8
local PADDLE_HALF_HEIGHT = 0.18
local POWER_HALF_W, POWER_HALF_H = 0.6, 0.26
local POWER_FALL = 3.0        -- units per second
local POWER_POINTS = 50
local WIDE_TIME, SLOW_TIME = 15.0, 12.0
local SLOW_FACTOR = 0.65
local FX_TIME = 0.3
local PARK_Y = -30.0
local CLEAR_DELAY, OVER_DELAY = 1.6, 1.4

local hud = {}
local pools = { wide = {}, slow = {}, fx = {} }
local paused = false
local ending = nil            -- nil, "clear" or "over"
local end_clock = 0.0
local wide_clock, slow_clock = 0.0, 0.0
local message_clock = 0.0
local shown = {}

local function pool(prefix)
    local items = {}
    local i = 1
    while true do
        local id = scene.find_entity(prefix .. i)
        if id == 0 then break end
        items[#items + 1] = { id = id, active = false, x = 0.0, y = 0.0, clock = 0.0 }
        i = i + 1
    end
    return items
end

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
    ui.transition_fade_in(0.4)
    hud.score = ui.find("ScoreText")
    hud.level = ui.find("LevelText")
    hud.lives = ui.find("LivesText")
    hud.message = ui.find("MessageText")
    hud.pause = ui.find("PauseWindow")
    hud.resume = ui.find("ResumeButton")
    pools.wide = pool("PowerWide")
    pools.slow = pool("PowerSlow")
    pools.fx = pool("BreakFx")

    if gamestate.get("breakout_lives", 0) <= 0 then gamestate.set("breakout_lives", 3) end
    gamestate.set("breakout_score", gamestate.get("breakout_score", 0))
    gamestate.set("breakout_level", level)
    gamestate.set("breakout_level_score", gamestate.get("breakout_score", 0))
    gamestate.set("breakout_bricks", #scene.find_entities("Brick"))
    gamestate.set("breakout_events", "")
    gamestate.set("breakout_serves", 0)
    gamestate.set("breakout_paused", false)
    gamestate.set("breakout_frozen", false)
    gamestate.set("breakout_wide", false)
    gamestate.set("breakout_speed_factor", 1.0)
    if hud.pause ~= 0 then ui.set_visible(hud.pause, false) end
    show_message("Level " .. level .. "\nSpace, A or click to serve", 2.5)
end

-- Bursts and power-ups ----------------------------------------------------------------------------------------------

local function spawn(items, x, y)
    for _, item in ipairs(items) do
        if not item.active then
            item.active, item.x, item.y, item.clock = true, x, y, 0.0
            transform.set_position(item.id, x, y, 0.2)
            ui.set_visible(item.id, true)
            return item
        end
    end
    return nil
end

local function retire(item)
    item.active = false
    ui.set_visible(item.id, false)
    transform.set_position(item.id, 0.0, PARK_Y, 0.0)
end

local function read_events()
    local queue = gamestate.get("breakout_events", "")
    if queue == "" then return end
    gamestate.set("breakout_events", "")
    for kind, x, y, r, g, b in string.gmatch(queue, "(%a+) (%S+) (%S+) (%S+) (%S+) (%S+);") do
        x, y = tonumber(x), tonumber(y)
        if kind == "break" then
            local fx = spawn(pools.fx, x, y)
            if fx then entity.set_tint(fx.id, tonumber(r), tonumber(g), tonumber(b), 1.0) end
        elseif kind == "wide" or kind == "slow" then
            spawn(pools[kind], x, y)
        end
    end
end

local function update_fx(dt)
    for _, fx in ipairs(pools.fx) do
        if fx.active then
            fx.clock = fx.clock + dt
            local t = math.min(1.0, fx.clock / FX_TIME)
            transform.set_scale(fx.id, 1.5 * (1.0 + 0.6 * t), 0.5 * (1.0 + 0.6 * t), 1.0)
            local r, g, b = entity.get_tint(fx.id)
            entity.set_tint(fx.id, r, g, b, 1.0 - t)
            if t >= 1.0 then retire(fx) end
        end
    end
end

local function catch(kind)
    sound.play("sounds/game/power_up.wav")
    gamestate.set("breakout_score", gamestate.get("breakout_score", 0) + POWER_POINTS)
    if kind == "wide" then
        gamestate.set("breakout_wide", true)
        wide_clock = WIDE_TIME
        show_message("Wide paddle!", 1.2)
    else
        gamestate.set("breakout_speed_factor", SLOW_FACTOR)
        slow_clock = SLOW_TIME
        show_message("Slow ball!", 1.2)
    end
end

local function update_power_ups(dt)
    local px = gamestate.get("breakout_paddle_x", 0.0)
    local half = gamestate.get("breakout_paddle_half", 1.2)
    for kind, items in pairs({ wide = pools.wide, slow = pools.slow }) do
        for _, item in ipairs(items) do
            if item.active then
                item.y = item.y - POWER_FALL * dt
                transform.set_position(item.id, item.x, item.y, 0.2)
                local touches = math.abs(item.x - px) < half + POWER_HALF_W and
                        math.abs(item.y - PADDLE_Y) < PADDLE_HALF_HEIGHT + POWER_HALF_H
                if touches and gamestate.get("breakout_ball_state", "") ~= "out" then
                    retire(item)
                    catch(kind)
                elseif item.y < -9.0 then
                    retire(item)
                end
            end
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
    if hud.pause ~= 0 then ui.set_visible(hud.pause, value) end
    if value and hud.resume ~= 0 then ui.set_focus(hud.resume) end
end

function on_pause_resume()
    set_paused(false)
end

function on_pause_restart()
    set_paused(false)
    ending = "restart"
    gamestate.set("breakout_score", gamestate.get("breakout_level_score", 0))
    scene.transition_to(gamestate.get("breakout_scene", FIRST_LEVEL), "fade_out", 0.3)
end

function on_pause_quit()
    set_paused(false)
    ending = "quit"
    gamestate.clear()
    scene.transition_to("scenes/title.owl", "fade_out", 0.3)
end

-- Frame -------------------------------------------------------------------------------------------------------------

local function update_hud()
    set_text("score", "Score " .. gamestate.get("breakout_score", 0))
    set_text("level", "Level " .. level)
    set_text("lives", "Lives " .. math.max(0, gamestate.get("breakout_lives", 0)))
end

local function check_end(dt)
    if ending == nil then
        if gamestate.get("breakout_bricks", 1) <= 0 then
            ending = "clear"
            gamestate.set("breakout_frozen", true)
            sound.play("sounds/game/level_clear.wav")
            show_message("Level clear!")
        elseif gamestate.get("breakout_lives", 1) <= 0 then
            ending = "over"
            show_message("Game over")
        end
        return
    end
    end_clock = end_clock + dt
    if ending == "clear" and end_clock >= CLEAR_DELAY then
        ending = "leaving"
        local target = (next_scene ~= nil and next_scene ~= "") and next_scene or "scenes/victory.owl"
        gamestate.set("breakout_scene", target)
        scene.transition_to(target, "fade_out", 0.4)
    elseif ending == "over" and end_clock >= OVER_DELAY then
        ending = "leaving"
        scene.transition_to("scenes/game_over.owl", "fade_out", 0.4)
    end
end

function on_update(dt)
    if ending == nil and input.is_action_just_pressed("pause") then
        set_paused(not paused)
    end
    update_hud()
    if paused then return end
    read_events()
    update_fx(dt)
    update_power_ups(dt)
    update_timers(dt)
    check_end(dt)
end
