-- Title screen of Ring Runner: Launch starts the level, How to play shows the rules and the controls in a window,
-- Settings, Quit. The best score and its medal come from the settings (`rail_best`, `rail_best_medal`). Every change
-- of screen is a deep blue sweep (`scene.transition_to` with a colour); Launch plays the start jingle.

properties = {
    { name = "sweep_color", type = "color", default = { 0.0, 0.05, 0.14, 1 } },
}

local leaving = false
local window_id, back_id, how_to_id = 0, 0, 0

local function leave(path, jingle)
    if leaving then return end
    leaving = true
    if jingle then sound.play("sounds/ui/start.wav") end
    scene.transition_to(path, "wipe_left", 0.45, sweep_color.r, sweep_color.g, sweep_color.b)
end

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("wipe_left", 0.5, sweep_color.r, sweep_color.g, sweep_color.b)
    end
    window_id = scene.find_entity("HowToWindow")
    back_id = scene.find_entity("HowToBackButton")
    how_to_id = scene.find_entity("HowToButton")
    local best = settings.get("rail_best", 0)
    local medal = settings.get("rail_best_medal", 0)
    gamestate.set("rail_title_best", best)
    local best_id = scene.find_entity("BestText")
    if best_id ~= 0 then
        ui.set_text(best_id, best > 0 and string.format("BEST %06d", best) or "NO FLIGHT YET")
    end
    local medal_id = scene.find_entity("BestMedal")
    if medal_id ~= 0 then
        ui.set_image_frame(medal_id, medal)
        ui.set_visible(medal_id, medal > 0)
    end
    gamestate.set("rail_how_to", false)
end

function on_update(dt)
    if gamestate.get("rail_how_to", false) and input.is_action_just_pressed("ui_cancel") then
        on_how_to_closed()
    end
end

function on_play_clicked()
    gamestate.clear()
    leave("scenes/level.owl", true)
end

function on_how_to_clicked()
    if leaving or window_id == 0 then return end
    ui.set_visible(window_id, true)
    ui.raise_window(window_id)
    ui.set_focus(back_id)
    gamestate.set("rail_how_to", true)
end

function on_how_to_closed()
    if window_id == 0 then return end
    ui.set_visible(window_id, false)
    ui.set_focus(how_to_id)
    gamestate.set("rail_how_to", false)
end

function on_settings_clicked()
    leave("scenes/settings.owl", false)
end

function on_quit_clicked()
    scene.quit()
end
