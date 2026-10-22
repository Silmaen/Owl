-- Tavern scene script: the level of the tavern. The key of the cellar hides by the hearth; once it is taken, the
-- trapdoor opens on the cellar (scenes/tavern_cellar.owl), where the chest ends the level. Escape (or Start) goes back
-- to the hub without completing it. The fire and the candles flicker through `LightFlicker` components, a
-- `FirstPersonController` walks the player.
-- Demonstrates: a scene script, gamestate shared by the scripts of a level, ui.set_text, scene.transition_to.

properties = {
    { name = "hub_scene", type = "string", default = "scenes/world_map.owl" },
}

local objective_id = 0
local leaving = false
local shown = -1

function on_create()
    objective_id = ui.find("ObjectiveText")
    gamestate.set("tavern_key", 0)
    gamestate.set("tavern_message", "")
    ui.transition_fade_in(0.4)
end

function on_update(_)
    if leaving or ui.is_transition_active() then
        return
    end
    if input.is_action_just_pressed("pause") then
        leaving = true
        scene.transition_to(hub_scene, "fade_out", 0.3)
        return
    end
    local has_key = gamestate.get("tavern_key", 0)
    local message = gamestate.get("tavern_message", "")
    local state = has_key * 2 + (message ~= "" and 1 or 0)
    if state == shown then
        return
    end
    shown = state
    if message ~= "" then
        ui.set_text(objective_id, message)
    elseif has_key > 0 then
        ui.set_text(objective_id, "Objective: open the cellar trapdoor")
    else
        ui.set_text(objective_id, "Objective: find the key of the cellar")
    end
end
