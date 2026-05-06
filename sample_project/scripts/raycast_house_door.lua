-- Raycast house door — opens the raycast E1M1-inspired demo level when the
-- player presses E nearby. Stores the world position so the player lands
-- back on the same spot when they exit the raycast scene.

local pending_scene = nil

function on_create()
    log.trace("RaycastHouseDoor ready")
end

function on_update(dt)
    if pending_scene and not ui.is_transition_active() then
        scene.load_scene(pending_scene)
        pending_scene = nil
    end
end

-- Bound by Trigger.Type=Interaction with CallbackName=on_raycast_door_interact.
function on_raycast_door_interact(player_id)
    local player = scene.find_entity("Player")
    if player ~= 0 then
        local px, py, pz = transform.get_position(player)
        gamestate.set("world_return_x", px)
        gamestate.set("world_return_y", py)
    end
    log.info("Entering raycast house — loading raycast demo")
    pending_scene = "scenes/raycast_demo.owl"
    ui.transition_fade_out(0.3)
end
