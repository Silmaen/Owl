-- Title screen of Breakout: Play starts a fresh run at level 1, Level select opens the list of levels, How to play
-- shows the rules and the controls in a window, Settings, Quit. The score bar reads the best score and the levels
-- cleared from the settings (`breakout_best`, `breakout_done_N`); the halo of the neon title flickers now and then.
-- Every change of screen is a magenta wipe (`scene.transition_to` with a colour).

properties = {
    { name = "wipe_color", type = "color", default = { 0.45, 0.05, 0.55, 1 } },
}

local LEVEL_COUNT = 5

local leaving = false
local window_id, back_id, how_to_id, glow_id = 0, 0, 0, 0
local flicker_clock, flicker_left, flicker_count = 3.0, 0.0, 0

local function leave(path, jingle)
    if leaving then return end
    leaving = true
    if jingle then sound.play("sounds/ui/start.wav") end
    scene.transition_to(path, "wipe_up", 0.3, wipe_color.r, wipe_color.g, wipe_color.b)
end

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("wipe_up", 0.4, wipe_color.r, wipe_color.g, wipe_color.b)
    end
    window_id = scene.find_entity("HowToWindow")
    back_id = scene.find_entity("HowToBackButton")
    how_to_id = scene.find_entity("HowToButton")
    glow_id = scene.find_entity("TitleGlow")
    local cleared = 0
    for i = 1, LEVEL_COUNT do
        if settings.get("breakout_done_" .. i, false) then cleared = cleared + 1 end
    end
    gamestate.set("breakout_cleared", cleared)
    local bar_id = scene.find_entity("ScoreBar")
    if bar_id ~= 0 then
        ui.set_text(bar_id, string.format("HI-SCORE %06d     CLEARED %d/%d", settings.get("breakout_best", 0), cleared,
                LEVEL_COUNT))
    end
end

function on_update(dt)
    -- The neon flickers: off for a few frames every few seconds.
    flicker_clock = flicker_clock - dt
    if flicker_clock <= 0 then
        flicker_left = 0.12
        flicker_count = flicker_count + 1
        flicker_clock = 2.0 + (flicker_count % 3) * 0.9
        if glow_id ~= 0 then ui.set_visible(glow_id, false) end
    end
    if flicker_left > 0 then
        flicker_left = flicker_left - dt
        if flicker_left <= 0 and glow_id ~= 0 then ui.set_visible(glow_id, true) end
    end
    if gamestate.get("breakout_how_to", false) and input.is_action_just_pressed("ui_cancel") then
        on_how_to_closed()
    end
end

function on_play_clicked()
    gamestate.clear()
    gamestate.set("breakout_score", 0)
    gamestate.set("breakout_lives", 3)
    gamestate.set("breakout_level", 1)
    gamestate.set("breakout_scene", "scenes/level_1.owl")
    leave("scenes/level_1.owl", true)
end

function on_levels_clicked()
    leave("scenes/level_select.owl", false)
end

function on_how_to_clicked()
    if leaving or window_id == 0 then return end
    ui.set_visible(window_id, true)
    ui.raise_window(window_id)
    ui.set_focus(back_id)
    gamestate.set("breakout_how_to", true)
end

function on_how_to_closed()
    if window_id == 0 then return end
    ui.set_visible(window_id, false)
    ui.set_focus(how_to_id)
    gamestate.set("breakout_how_to", false)
end

function on_settings_clicked()
    leave("scenes/settings.owl", false)
end

function on_quit_clicked()
    scene.quit()
end
