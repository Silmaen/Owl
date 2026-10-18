-- Title screen of Sokoban: Play opens the level select, Continue starts the first level not done yet (disabled when
-- there is none to continue), How to play shows the rules and the controls in a window, Settings, Quit. The warehouse
-- log on the right reads the progress (the `sokoban_done_NN` and `sokoban_best_NN` settings written by sokoban.lua).
-- Every change of screen is a wipe in the brown of the crates (`scene.transition_to` with a colour).

-- Every level, to count the progress and find the level to continue (a level names the next one in `next_scene`).
local LEVEL_SCENES = {
    "scenes/level_01.owl",
    "scenes/level_02.owl",
    "scenes/level_03.owl",
    "scenes/level_04.owl",
    "scenes/level_05.owl",
    "scenes/level_06.owl",
    "scenes/level_07.owl",
    "scenes/level_08.owl",
    "scenes/level_09.owl",
    "scenes/level_10.owl",
}
local LEVEL_NAMES = { "First push", "Two by two", "Side door", "Back room", "Cellar", "Four corners", "Pantry",
                      "Columns", "Loading bay", "Warehouse" }
local WIPE = { 0.2, 0.12, 0.05 }

local leaving = false
local next_level = 1
local window_id, back_id, how_to_id = 0, 0, 0

local function leave(path, jingle)
    if leaving then return end
    leaving = true
    if jingle then sound.play("sounds/ui/start.wav") end
    scene.transition_to(path, "wipe_left", 0.3, WIPE[1], WIPE[2], WIPE[3])
end

local function set(name, text)
    local id = scene.find_entity(name)
    if id ~= 0 then ui.set_text(id, text) end
    return id
end

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("wipe_left", 0.4, WIPE[1], WIPE[2], WIPE[3])
    end
    window_id = scene.find_entity("HowToWindow")
    back_id = scene.find_entity("HowToBackButton")
    how_to_id = scene.find_entity("HowToButton")
    local done, moves = 0, 0
    next_level = 0
    for i = 1, #LEVEL_SCENES do
        if settings.get(string.format("sokoban_done_%02d", i), false) then
            done = done + 1
            moves = moves + settings.get(string.format("sokoban_best_%02d", i), 0)
        elseif next_level == 0 then
            next_level = i
        end
    end
    set("ProgressLabel", "Levels done: " .. done .. " / " .. #LEVEL_SCENES)
    local bar_id = scene.find_entity("ProgressBar")
    if bar_id ~= 0 then ui.set_progress(bar_id, done / #LEVEL_SCENES) end
    if next_level == 0 then
        set("NextLabel", "Every level is done")
    else
        set("NextLabel", "Next: level " .. next_level .. ", " .. LEVEL_NAMES[next_level])
    end
    set("BestLabel", done == 0 and "No level done yet" or ("Fewest moves, levels done: " .. moves))
    local continue_id = scene.find_entity("ContinueButton")
    if continue_id ~= 0 then
        if done == 0 or next_level == 0 then
            ui.set_button_enabled(continue_id, false)
        else
            ui.set_text(continue_id, "Continue: level " .. next_level)
        end
    end
    gamestate.set("sokoban_levels_done", done)
end

function on_update(dt)
    if gamestate.get("sokoban_how_to", false) and input.is_action_just_pressed("ui_cancel") then
        on_how_to_closed()
    end
end

function on_play_clicked()
    leave("scenes/level_select.owl", false)
end

function on_continue_clicked()
    if next_level == 0 then return end
    gamestate.set("sokoban_level", next_level)
    leave(LEVEL_SCENES[next_level] or LEVEL_SCENES[1], true)
end

function on_how_to_clicked()
    if leaving or window_id == 0 then return end
    ui.set_visible(window_id, true)
    ui.raise_window(window_id)
    ui.set_focus(back_id)
    gamestate.set("sokoban_how_to", true)
end

function on_how_to_closed()
    if window_id == 0 then return end
    ui.set_visible(window_id, false)
    ui.set_focus(how_to_id)
    gamestate.set("sokoban_how_to", false)
end

function on_settings_clicked()
    leave("scenes/settings.owl", false)
end

function on_quit_clicked()
    scene.quit()
end
