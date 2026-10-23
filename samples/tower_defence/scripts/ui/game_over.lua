-- Game over screen of Tower defence: the keep has fallen; the wave reached and the enemies stopped (`tower_result_*`
-- written by defence.lua), the parade standing before the fallen keep. Play again replays the map, Title goes back.

properties = {
    { name = "wipe_color", type = "color", default = { 0.1, 0.14, 0.24, 1 } },
}

local leaving = false

local function set(name, text)
    local id = scene.find_entity(name)
    if id ~= 0 then ui.set_text(id, text) end
end

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("wipe_down", 0.5, wipe_color.r, wipe_color.g, wipe_color.b)
    end
    set("StatLine1", "Wave reached    " .. gamestate.get("tower_result_wave", 0) .. " / "
            .. gamestate.get("tower_result_waves", 8))
    set("StatLine2", "Enemies stopped " .. gamestate.get("tower_result_kills", 0))
    set("StatLine3", "Towers built    " .. gamestate.get("tower_result_towers", 0))
    set("StatLine4", "Try the frost tower at a bend, or call waves later.")
end

function on_update(dt)
end

local function leave(path)
    if leaving then return end
    leaving = true
    scene.transition_to(path, "wipe_down", 0.35, wipe_color.r, wipe_color.g, wipe_color.b)
end

function on_again_clicked()
    leave(gamestate.get("tower_scene", "scenes/map_01.owl"))
end

function on_title_clicked()
    leave("scenes/title.owl")
end
