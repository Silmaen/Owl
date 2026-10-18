-- Title screen of Wolfenstein mini: new game, continue, settings, quit, and the best tally of each floor.
-- Continue loads the newest of the autosave (slot 1, written when a floor starts) and the quick save (slot 2, F6).

local FLOORS = 3

local pending_slot = 0
local leaving = false

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
    ui.transition_fade_in(0.5)
    local continue_id = scene.find_entity("ContinueButton")
    if continue_id ~= 0 then
        ui.set_button_enabled(continue_id, newest_slot() ~= 0)
    end
    local lines = { "Best score " .. settings.get("wolf_best_score", 0) }
    for floor = 1, FLOORS do
        local best = "wolf_best_" .. floor .. "_"
        lines[#lines + 1] = string.format("Floor %d: items %d%%, secrets %d%%, map %d%%", floor,
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
end

function on_new_game_clicked()
    if leaving then
        return
    end
    leaving = true
    sound.play("sounds/ui/start.wav")
    gamestate.clear()
    gamestate.set("wolf_score", 0)
    scene.transition_to("scenes/level_1.owl", "fade", 0.4)
end

function on_continue_clicked()
    local slot = newest_slot()
    if leaving or slot == 0 then
        return
    end
    leaving = true
    sound.play("sounds/ui/start.wav")
    pending_slot = slot
    ui.transition_fade_out(0.3)
end

function on_settings_clicked()
    if not leaving then
        leaving = true
        scene.transition_to("scenes/settings.owl", "fade", 0.3)
    end
end

function on_quit_clicked()
    scene.quit()
end
