-- The cellar trapdoor: E on it opens the cellar once the key is taken, else says it is locked.

properties = {
    { name = "cellar_scene", type = "string", default = "scenes/tavern_cellar.owl" },
}

local opening = false

-- Bound by Trigger.Type=Interaction with CallbackName=on_trapdoor_interact.
function on_trapdoor_interact()
    if opening then
        return
    end
    if gamestate.get("tavern_key", 0) == 0 then
        gamestate.set("tavern_message", "The trapdoor is locked: its key must be somewhere in the room")
        return
    end
    opening = true
    sound.play("sounds/raycast/door.wav")
    scene.transition_to(cellar_scene, "fade", 0.5)
end
