-- Title screen of Tower defence: Play starts the first map, How to play shows the rules and the controls in a window
-- (the only place besides the pause window that shows them), Settings, Quit. The board on the right reads the progress
-- of the map (`tower_done_01` and `tower_best_01` settings written by defence.lua). Every change of screen is a wipe in
-- the night blue of the game (`scene.transition_to` with a colour).

properties = {
    { name = "first_map", type = "string", default = "scenes/map_01.owl" },
    { name = "map_name", type = "string", default = "Green Ford" },
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
    set("MapLabel", "Map 1: " .. map_name)
    local done = settings.get("tower_done_01", false)
    set("StateLabel", done and "Defended" or "Not defended yet")
    set("BestLabel", done and ("Most lives kept: " .. settings.get("tower_best_01", 0)) or "8 waves to hold")
    local play_id = scene.find_entity("PlayButton")
    if play_id ~= 0 and done then ui.set_text(play_id, "Play again") end
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
