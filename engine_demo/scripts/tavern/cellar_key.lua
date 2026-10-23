-- The key of the cellar: E nearby takes it (the key and its glint disappear) and lets the trapdoor open.
-- Demonstrates: an Interaction trigger in 3D (the body box of a FirstPersonController), ui.set_visible on world entities.

properties = {
    { name = "glint", type = "string", default = "KeyGlint" },
}

local taken = false

function on_create()
    -- Taken on an earlier visit (back from the cellar): the key and its glint stay gone.
    if gamestate.get("tavern_key", 0) > 0 then
        taken = true
        ui.set_visible(entity_id, false)
        local glint_id = scene.find_entity(glint)
        if glint_id ~= 0 then
            ui.set_visible(glint_id, false)
        end
    end
end

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
