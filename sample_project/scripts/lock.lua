-- Key-locked door or switch: interacting (E, gamepad X) opens it only when the matching key is in the inventory.
-- Demonstrates: Trigger.Type=Interaction, string properties, gamestate as the inventory, entity destruction.
--
-- `key` is the inventory item that opens it, `target` the entity removed when it opens (a door or a gate). The key is
-- used up; `unlocked_<tag>` remembers the opening, so a scene loaded again finds it open.

properties = {
    { name = "key", type = "string", default = "" },
    { name = "target", type = "string", default = "" },
    { name = "need", type = "string", default = "the key" },
    { name = "opened", type = "string", default = "Unlocked" },
}

local done = false

local function open()
    done = true
    local target_id = scene.find_entity(target)
    if target_id ~= 0 then
        scene.destroy_entity(target_id)
    end
    -- The switch stays in the scene, a door lock zone has nothing left to guard.
    if entity.has_component(entity_id, "SpriteRenderer") == false then
        scene.destroy_entity(entity_id)
    end
end

function on_create()
    if gamestate.get("unlocked_" .. entity.get_name(entity_id), false) then
        open()
    end
end

-- Bound by Trigger.Type=Interaction with CallbackName=on_lock_interact.
function on_lock_interact()
    if done then
        return
    end
    local count = gamestate.get("inv_" .. key, 0)
    if count <= 0 then
        gamestate.set("inv_toast", "Locked: you need " .. need)
        return
    end
    gamestate.set("inv_" .. key, count - 1)
    gamestate.set("unlocked_" .. entity.get_name(entity_id), true)
    gamestate.set("inv_toast", opened)
    sound.play("sounds/start.wav")
    open()
end
