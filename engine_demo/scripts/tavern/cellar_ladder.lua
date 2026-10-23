-- The cellar ladder: E on it (or on the hatch above) climbs back to the tavern, by the trapdoor.

properties = {
    { name = "tavern_scene", type = "string", default = "scenes/tavern.owl" },
}

local climbing = false

-- Bound by Trigger.Type=Interaction with CallbackName=on_ladder_interact.
function on_ladder_interact()
    if climbing then
        return
    end
    climbing = true
    gamestate.set("tavern_from_cellar", 1)
    sound.play("sounds/raycast/door.wav")
    scene.transition_to(tavern_scene, "fade", 0.5)
end
