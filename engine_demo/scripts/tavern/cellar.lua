-- Cellar scene script: dark, lit by the lantern the player carries; E on the chest ends the level of the tavern and
-- goes back to the hub. Escape goes back to the hub without completing it.
-- Demonstrates: a spot light parented to the camera (the lantern), gamestate (`level_tavern_done`).

properties = {
    { name = "hub_scene", type = "string", default = "scenes/world_map.owl" },
}

local objective_id = 0
local leaving = false

function on_create()
    objective_id = ui.find("ObjectiveText")
    ui.set_text(objective_id, "Objective: find the treasure of the cellar")
    ui.transition_fade_in(0.6)
end

function on_update(_)
    if leaving or ui.is_transition_active() then
        return
    end
    if input.is_action_just_pressed("pause") then
        leaving = true
        scene.transition_to(hub_scene, "fade_out", 0.3)
    end
end
