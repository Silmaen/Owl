-- Title screen of Sokoban, after the Engine Demo main menu (scripts/ui/main_menu.lua): Play opens the level select,
-- Continue starts the first level not done yet (disabled when there is none to continue), Settings, Quit.
-- The progress is the `sokoban_done_NN` settings written by sokoban.lua.

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

local leaving = false
local next_level = 1

local function leave(path, jingle)
    if leaving then return end
    leaving = true
    if jingle then sound.play("sounds/ui/start.wav") end
    scene.transition_to(path, "fade_out", 0.3)
end

function on_create()
    ui.transition_fade_in(0.5)
    local done = 0
    next_level = 0
    for i = 1, #LEVEL_SCENES do
        if settings.get(string.format("sokoban_done_%02d", i), false) then
            done = done + 1
        elseif next_level == 0 then
            next_level = i
        end
    end
    local progress_id = scene.find_entity("ProgressLabel")
    if progress_id ~= 0 then
        ui.set_text(progress_id, "Levels done: " .. done .. " / " .. #LEVEL_SCENES)
    end
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

function on_play_clicked()
    leave("scenes/level_select.owl", false)
end

function on_continue_clicked()
    if next_level == 0 then return end
    leave(LEVEL_SCENES[next_level] or LEVEL_SCENES[1], true)
end

function on_settings_clicked()
    leave("scenes/settings.owl", false)
end

function on_quit_clicked()
    scene.quit()
end
