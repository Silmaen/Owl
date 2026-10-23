-- The treasure chest of the cellar: E nearby completes the level of the tavern and goes back to the hub.

properties = {
    { name = "hub_scene", type = "string", default = "scenes/world_map.owl" },
}

local opened = false

-- Bound by Trigger.Type=Interaction with CallbackName=on_chest_interact.
function on_chest_interact()
    if opened then
        return
    end
    opened = true
    gamestate.set("level_tavern_done", 1)
    sound.play("sounds/raycast/treasure.wav")
    sound.play("sounds/ui/victory.wav")
    scene.transition_to(hub_scene, "fade_out", 0.6)
end
