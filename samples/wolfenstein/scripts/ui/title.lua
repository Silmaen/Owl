-- Title screen of Wolfenstein mini: new game, continue, How to play (the rules and the controls in a window),
-- settings, quit, and the service record: the best score and the best tally of each floor (the `wolf_best_*`
-- settings). Continue loads the newest of the autosave (slot 1, written when a floor starts) and the quick save
-- (slot 2, F6). Every change of screen is a fade through dark red (`scene.transition_to` with a colour).

local FLOORS = 3
local FADE = { 0.22, 0.02, 0.02 }

local pending_slot = 0
local leaving = false
local window_id, back_id, how_to_id = 0, 0, 0

-- The newest save slot, 0 when there is none.
local function newest_slot()
    local slot, stamp = 0, ""
    for _, info in ipairs(save.list_saves()) do
        if (info.slot == 1 or info.slot == 2) and info.timestamp > stamp then
            slot, stamp = info.slot, info.timestamp
        end
    end
    return slot
end

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("fade_in", 0.6, FADE[1], FADE[2], FADE[3])
    end
    window_id = scene.find_entity("HowToWindow")
    back_id = scene.find_entity("HowToBackButton")
    how_to_id = scene.find_entity("HowToButton")
    local continue_id = scene.find_entity("ContinueButton")
    if continue_id ~= 0 then
        ui.set_button_enabled(continue_id, newest_slot() ~= 0)
    end
    local lines = { string.format("BEST SCORE  %d", settings.get("wolf_best_score", 0)) }
    for floor = 1, FLOORS do
        local best = "wolf_best_" .. floor .. "_"
        lines[#lines + 1] = string.format("FLOOR %d\n  items %d%%  secrets %d%%  map %d%%", floor,
                settings.get(best .. "items", 0), settings.get(best .. "secrets", 0), settings.get(best .. "explored", 0))
    end
    local best_id = scene.find_entity("BestText")
    if best_id ~= 0 then
        ui.set_text(best_id, table.concat(lines, "\n"))
    end
end

function on_update(dt)
    -- A save loads once the fade is over (`save.load_game` replaces the scene, there is no transition for it).
    if pending_slot ~= 0 and not ui.is_transition_active() then
        save.load_game(pending_slot)
        pending_slot = 0
    end
    if gamestate.get("wolf_how_to", false) and input.is_action_just_pressed("ui_cancel") then
        on_how_to_closed()
    end
end

local function leave(path, jingle)
    if leaving then
        return
    end
    leaving = true
    if jingle then
        sound.play("sounds/ui/start.wav")
    end
    scene.transition_to(path, "fade_out", 0.5, FADE[1], FADE[2], FADE[3])
end

function on_new_game_clicked()
    if leaving then
        return
    end
    gamestate.clear()
    gamestate.set("wolf_score", 0)
    leave("scenes/level_1.owl", true)
end

function on_continue_clicked()
    local slot = newest_slot()
    if leaving or slot == 0 then
        return
    end
    leaving = true
    sound.play("sounds/ui/start.wav")
    pending_slot = slot
    ui.transition_play("fade_out", 0.4, FADE[1], FADE[2], FADE[3])
end

function on_how_to_clicked()
    if leaving or window_id == 0 then
        return
    end
    ui.set_visible(window_id, true)
    ui.raise_window(window_id)
    ui.set_focus(back_id)
    gamestate.set("wolf_how_to", true)
end

function on_how_to_closed()
    if window_id == 0 then
        return
    end
    ui.set_visible(window_id, false)
    ui.set_focus(how_to_id)
    gamestate.set("wolf_how_to", false)
end

function on_settings_clicked()
    leave("scenes/settings.owl", false)
end

function on_quit_clicked()
    scene.quit()
end
