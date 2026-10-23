-- A locked door of Wolfenstein mini: a `RaycastDoor` whose `interactionAction` is empty, so the engine never opens it
-- by itself. On `interact` near it, the door opens (`door.activate`) when the player carries its key, and says which key
-- it needs otherwise. `wolf_locked_tries` counts the refusals and `wolf_doors_unlocked` the openings, for the scenarios.

properties = {
    { name = "key", type = "string", default = "gold" },
    { name = "range", type = "float", default = 1.5, min = 0 },  -- cells between the player and the door centre, as the built-in doors
}

local player_id = 0

function on_create()
    player_id = scene.find_entity("Player")
end

function on_update(dt)
    if player_id == 0 or gamestate.get("wolf_paused", false) or not input.is_action_just_pressed("interact")
            or door.get_state(entity_id) ~= "idle" then
        return
    end
    local px, py = transform.get_position(player_id)
    local x, y = transform.get_position(entity_id)
    if (px - x) ^ 2 + (py - y) ^ 2 > range * range then
        return
    end
    if gamestate.get("wolf_key_" .. key, 0) == 1 then
        door.activate(entity_id)
        gamestate.set("wolf_doors_unlocked", gamestate.get("wolf_doors_unlocked", 0) + 1)
    else
        gamestate.set("wolf_locked_tries", gamestate.get("wolf_locked_tries", 0) + 1)
        gamestate.set("wolf_toast", "Locked: find the " .. key .. " key")
    end
end
