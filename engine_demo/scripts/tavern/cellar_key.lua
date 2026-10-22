-- The key of the cellar: E nearby takes it (the key and its glint disappear) and lets the trapdoor open.
-- Demonstrates: an Interaction trigger in 3D (the body box of a FirstPersonController), ui.set_visible on world entities.

properties = {
    { name = "glint", type = "string", default = "KeyGlint" },
}

local taken = false

-- Bound by Trigger.Type=Interaction with CallbackName=on_key_interact.
function on_key_interact()
    if taken then
        return
    end
    taken = true
    gamestate.set("tavern_key", 1)
    gamestate.set("tavern_message", "")
    sound.play("sounds/platformer/coin_collect.wav")
    ui.set_visible(entity_id, false)
    local glint_id = scene.find_entity(glint)
    if glint_id ~= 0 then
        ui.set_visible(glint_id, false)
    end
end
