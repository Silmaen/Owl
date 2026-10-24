-- Title screen of Tower defence: Play opens the world map, How to play shows the rules and the controls in a window
-- (the only place besides the pause window that shows them), Settings, Quit. The board on the right reads the progress
-- of the campaign (`tower_done_NN` and `tower_stars_NN` settings written by defence.lua). Every change of screen is a wipe in
-- the night blue of the game (`scene.transition_to` with a colour).

properties = {
    { name = "first_map", type = "string", default = "scenes/world_map.owl" },
    { name = "level_count", type = "int", default = 3, min = 1 },
    { name = "wipe_color", type = "color", default = { 0.1, 0.14, 0.24, 1 } },
}

local leaving = false
local window_id, back_id, how_to_id = 0, 0, 0

local function leave(path, jingle)
    if leaving then return end
    leaving = true
    if jingle then sound.play("sounds/ui/start.wav") end
    scene.transition_to(path, "wipe_down", 0.35, wipe_color.r, wipe_color.g, wipe_color.b)
end

local function set(name, text)
    local id = scene.find_entity(name)
    if id ~= 0 then ui.set_text(id, text) end
end

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("wipe_down", 0.4, wipe_color.r, wipe_color.g, wipe_color.b)
    end
    time.set_scale(1.0)
    time.set_paused(false)
    window_id = scene.find_entity("HowToWindow")
    back_id = scene.find_entity("HowToBackButton")
    how_to_id = scene.find_entity("HowToButton")
    local done, stars = 0, 0
    for index = 1, level_count do
        if settings.get(string.format("tower_done_%02d", index), false) then done = done + 1 end
        stars = stars + settings.get(string.format("tower_stars_%02d", index), 0)
    end
    set("MapLabel", "The campaign of the keep")
    set("StateLabel", string.format("Levels defended: %d / %d", done, level_count))
    set("BestLabel", string.format("Stars won: %d / %d", stars, level_count * 3))
    local play_id = scene.find_entity("PlayButton")
    if play_id ~= 0 and done > 0 then ui.set_text(play_id, "Continue") end
    gamestate.set("tower_title", true)
end

function on_update(dt)
    if gamestate.get("tower_how_to", false) and input.is_action_just_pressed("ui_cancel") then
        on_how_to_closed()
    end
end

function on_play_clicked()
    gamestate.clear()
    leave(first_map, true)
end

function on_how_to_clicked()
    if leaving or window_id == 0 then return end
    ui.set_visible(window_id, true)
    ui.raise_window(window_id)
    ui.set_focus(back_id)
    gamestate.set("tower_how_to", true)
end

function on_how_to_closed()
    if window_id == 0 then return end
    ui.set_visible(window_id, false)
    ui.set_focus(how_to_id)
    gamestate.set("tower_how_to", false)
end

function on_settings_clicked()
    leave("scenes/settings.owl", false)
end

function on_quit_clicked()
    scene.quit()
end
